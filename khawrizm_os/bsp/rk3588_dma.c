/**
 * rk3588_dma.c — RK3588 Zero-Copy DMA pipeline (Lobe II wiring)
 *
 * Implements the SYS_DMA_SUBMIT / SYS_DMA_POLL syscall backends.
 * Physically bridges the HAVEN microkernel to Niyah Engine Lobe II:
 *
 *   LimeSDR IQ buffer (peripheral FIFO)
 *       ↓  DMA_FROM_DEVICE (no CPU cache involvement)
 *   NPU SRAM input window (0xFF00_0000)
 *       ↓  rknn_create_mem_from_phys equivalent (pointer registration)
 *   Niyah Engine Sensory Lobe (WorldStateTensor fusion)
 *
 * The ARM Cortex-A76 data cache is fully bypassed for this path.
 * Cache maintenance: DCCIMVAC (clean+invalidate) on source buffer only,
 * performed before arming the DMA engine.
 *
 * Memory barriers:
 *   DSB ST  — drain all pending stores before descriptor write
 *   DSB SY  — full system barrier after DMA arm
 *   DMB OSH — outer-shareable barrier for coherency with NPU
 *
 * C11, zero external dependencies. No libc, no heap.
 * Ref: RK3588 TRM v1.0, Chapter 16 (DMA330 / PL330)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* =========================================================================
 * RK3588 DMA330 (PL330) MMIO register map
 * DMA channel 0 used for SDR → NPU transfers.
 * Base: 0xFF700000 (DMAC0)
 * =========================================================================*/
#define RK3588_DMAC0_BASE       0xFF700000UL

/* Channel 0 registers (offset 0x100 * channel). */
#define DMA_CH0_OFF             0x100u
#define DMA_SAR(ch)    (RK3588_DMAC0_BASE + (ch)*0x100u + 0x000u) /* Source address */
#define DMA_DAR(ch)    (RK3588_DMAC0_BASE + (ch)*0x100u + 0x004u) /* Dest address */
#define DMA_CCR(ch)    (RK3588_DMAC0_BASE + (ch)*0x100u + 0x008u) /* Channel control */
#define DMA_LC0(ch)    (RK3588_DMAC0_BASE + (ch)*0x100u + 0x00Cu) /* Loop counter 0 */
#define DMA_LC1(ch)    (RK3588_DMAC0_BASE + (ch)*0x100u + 0x010u) /* Loop counter 1 */

/* DMAC global registers. */
#define DMA_INTSTATUS   (RK3588_DMAC0_BASE + 0x020u)
#define DMA_INTCLR      (RK3588_DMAC0_BASE + 0x02Cu)
#define DMA_FSRD        (RK3588_DMAC0_BASE + 0x030u)  /* Fault status DMA request */
#define DMA_FSRC        (RK3588_DMAC0_BASE + 0x034u)  /* Fault status channel */
#define DMA_FTRD        (RK3588_DMAC0_BASE + 0x038u)  /* Fault type DMA request */
#define DMA_DBG_STATUS  (RK3588_DMAC0_BASE + 0xD00u)  /* Debug status */
#define DMA_DBG_CMD     (RK3588_DMAC0_BASE + 0xD04u)  /* Debug command */
#define DMA_DBG_INST0   (RK3588_DMAC0_BASE + 0xD08u)  /* Debug instruction 0 */
#define DMA_DBG_INST1   (RK3588_DMAC0_BASE + 0xD0Cu)  /* Debug instruction 1 */

/* CCR (Channel Control Register) value for 64-byte burst, no cache. */
/* SrcBurstSize=8, SrcBurstLen=8, SrcWidth=4B, DstWidth=4B,
   AwCachectrl=0 (non-cacheable), ArCachectrl=0 */
#define DMA_CCR_NONCACHEABLE    0x00800200u

/* NPU SRAM base (destination for SDR IQ samples). */
#define NPU_SRAM_BASE           0xFF000000UL
#define NPU_SRAM_SIZE           (4u * 1024u * 1024u)

/* LimeSDR USB3 IQ buffer PA (mapped by USB DMA controller at init). */
/* In production, this is provided by the LimeSDR platform driver. */
#define LIMESDR_IQ_PHYS_BASE    0x0A000000UL
#define LIMESDR_IQ_BLOCK_SIZE   (64u * 1024u)  /* 64 KB IQ block per transfer */

/* Descriptor slot count (ring of 4). */
#define DMA_SLOT_COUNT          4u

/* =========================================================================
 * DMA descriptor (matches Niyah Engine sensory_lobe/src/dma.rs layout)
 * repr(C), 64-byte aligned.
 * =========================================================================*/
typedef struct __attribute__((aligned(64))) {
    uint64_t source_phys_addr;
    uint64_t dest_phys_addr;
    uint32_t length;
    uint32_t flags;
    uint64_t next_descriptor_phys;
    uint8_t  _reserved[8];
} dma_descriptor_t;

/* Flags (mirror of DmaFlags in dma.rs). */
#define DMA_FLAG_INTERRUPT_ON_COMPLETE  0x01u
#define DMA_FLAG_END_OF_CHAIN           0x02u
#define DMA_FLAG_MEM_TO_NPU             0x20u
#define DMA_FLAG_CACHE_BYPASS           0x40u
#define DMA_FLAG_THERMAL_GUARD          0x80u

/* Descriptor ring (placed in non-cacheable SRAM by linker script). */
static dma_descriptor_t s_desc_ring[DMA_SLOT_COUNT] __attribute__((aligned(64)));

/* Completion flags (set by DMA ISR or polled). */
static volatile uint8_t s_dma_done[DMA_SLOT_COUNT];

/* Slot index from guest PA (simple mapping: slot = (pa - base) / sizeof). */
#define SLOT_FROM_PA(pa) \
    (uint32_t)(((pa) - (uint32_t)(uintptr_t)s_desc_ring) / sizeof(dma_descriptor_t))

/* =========================================================================
 * MMIO helpers
 * =========================================================================*/
static inline void mmio_w32(uint32_t addr, uint32_t val)
{
    *(volatile uint32_t *)(uintptr_t)addr = val;
    __asm__ volatile("dsb st" ::: "memory");
}

static inline uint32_t mmio_r32(uint32_t addr)
{
    uint32_t v = *(volatile uint32_t *)(uintptr_t)addr;
    __asm__ volatile("dsb sy" ::: "memory");
    return v;
}

/* =========================================================================
 * Cache maintenance (AArch64)
 *
 * DCCIMVAC: Clean and Invalidate Data Cache by MVA to PoC.
 * Used on source buffer before DMA_FROM_DEVICE to ensure CPU-written
 * data is visible to the DMA engine and cache lines are invalidated
 * to prevent stale reads after the transfer.
 * =========================================================================*/
static void cache_clean_invalidate_range(uintptr_t start, size_t len)
{
    /* Cache line size: 64 bytes on Cortex-A76. */
    const uintptr_t CACHE_LINE = 64u;
    uintptr_t addr = start & ~(CACHE_LINE - 1u);
    uintptr_t end  = (start + len + CACHE_LINE - 1u) & ~(CACHE_LINE - 1u);
    __asm__ volatile("dsb st" ::: "memory");
    while (addr < end) {
        /* DC CIVAC: Clean and Invalidate by VA to PoC. */
        __asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
        addr += CACHE_LINE;
    }
    __asm__ volatile("dsb sy\n isb\n" ::: "memory");
}

/* =========================================================================
 * rknn_create_mem_from_phys equivalent
 *
 * Registers a physical address range with the RK3588 RKNN NPU driver
 * by writing the PA directly to the NPU SMMU bypass register.
 * This allows the NPU to access the buffer without CPU involvement.
 *
 * RKNN IOMMU bypass: write phys_addr to RKNPU_MEM_BASE_ADDR register.
 * Base: 0xFDAB0000 (RKNPU), offset 0x210 (MEM_BASE_ADDR).
 * =========================================================================*/
#define RKNPU_BASE              0xFDAB0000UL
#define RKNPU_MEM_BASE_ADDR     (RKNPU_BASE + 0x210u)
#define RKNPU_MEM_SIZE_REG      (RKNPU_BASE + 0x214u)
#define RKNPU_CTRL_REG          (RKNPU_BASE + 0x200u)
#define RKNPU_CTRL_MEM_BYPASS   (1u << 4)

static void npu_register_phys_mem(uint64_t phys_addr, uint32_t size)
{
    /* Write physical base and size to NPU memory controller. */
    mmio_w32(RKNPU_MEM_BASE_ADDR, (uint32_t)(phys_addr & 0xFFFFFFFFu));
    mmio_w32(RKNPU_MEM_SIZE_REG,  size);
    /* Enable SMMU bypass for this buffer. */
    uint32_t ctrl = mmio_r32(RKNPU_CTRL_REG);
    mmio_w32(RKNPU_CTRL_REG, ctrl | RKNPU_CTRL_MEM_BYPASS);
    /* DMB OSH: outer-shareable barrier to synchronize with NPU. */
    __asm__ volatile("dmb osh" ::: "memory");
}

/* =========================================================================
 * Thermal guard
 *
 * RK3588 TSADC (thermal sensor) base: 0xFE710000
 * Channel 0 (CPU cluster) DATA register: offset 0x20
 * Temperature = (DATA * 1000 / 1900) - 273  (simplified linear)
 * Halt threshold: 85°C junction temperature.
 * =========================================================================*/
#define TSADC_BASE      0xFE710000UL
#define TSADC_DATA0     (TSADC_BASE + 0x20u)
#define TSADC_HALT_RAW  0x2EEu  /* ~85°C in raw ADC counts */

extern void mmio_halt(void) __attribute__((noreturn));

static void thermal_check(void)
{
    uint32_t raw = mmio_r32(TSADC_DATA0) & 0xFFFu;
    if (raw > TSADC_HALT_RAW) mmio_halt();
}

/* =========================================================================
 * PL330 DMA microcode execution (DMAGO)
 *
 * The PL330 is a microcode-based DMA engine. We issue DMAGO via
 * the debug interface to start channel 0 with our descriptor.
 * =========================================================================*/
#define PL330_DBGINST_DMAGO(ch)  (0xA0u | ((ch) << 8u))

static void pl330_start_channel0(uint32_t desc_phys)
{
    /* Wait for debug idle. */
    for (uint32_t t = 0; t < 100000u; t++) {
        if (!(mmio_r32(DMA_DBG_STATUS) & 1u)) break;
    }
    /* DMAGO CH0: inst0 = DMAGO opcode | channel, inst1 = descriptor PA. */
    mmio_w32(DMA_DBG_INST0, PL330_DBGINST_DMAGO(0));
    mmio_w32(DMA_DBG_INST1, desc_phys);
    /* Issue: write 0 to DBG_CMD to execute. */
    mmio_w32(DMA_DBG_CMD, 0);
    __asm__ volatile("dsb sy\n isb\n" ::: "memory");
}

/* =========================================================================
 * Public API: niyah_dma_submit / niyah_dma_poll
 * Called by microkernel.c ecall_dispatch (SYS_DMA_SUBMIT / SYS_DMA_POLL)
 * =========================================================================*/

/**
 * niyah_dma_submit — Submit a DMA descriptor for SDR→NPU zero-copy transfer.
 *
 * @param desc_pa  Guest physical address of a dma_descriptor_t.
 *                 The microkernel maps this to host PA before calling.
 * @return         0 on success, -1 on invalid descriptor.
 */
int niyah_dma_submit(uint32_t desc_pa)
{
    /* Thermal guard: abort if NPU is too hot. */
    thermal_check();

    /* Validate desc_pa is within a known safe host region. */
    if (desc_pa < (uint32_t)(uintptr_t)s_desc_ring ||
        desc_pa >= (uint32_t)(uintptr_t)(s_desc_ring + DMA_SLOT_COUNT))
        return -1;

    uint32_t slot = SLOT_FROM_PA(desc_pa);
    if (slot >= DMA_SLOT_COUNT) return -1;

    dma_descriptor_t *desc = &s_desc_ring[slot];

    /* Validate destination is within NPU SRAM window. */
    if (desc->dest_phys_addr < NPU_SRAM_BASE ||
        desc->dest_phys_addr >= NPU_SRAM_BASE + NPU_SRAM_SIZE)
        return -1;

    /* Validate length (64-byte aligned, max 4MB). */
    if (desc->length == 0 || desc->length % 64u != 0 ||
        desc->length > NPU_SRAM_SIZE)
        return -1;

    /* Cache maintenance on source buffer (DMA_FROM_DEVICE). */
    cache_clean_invalidate_range(
        (uintptr_t)desc->source_phys_addr, desc->length);

    /* Register NPU physical memory window (rknn_create_mem_from_phys). */
    npu_register_phys_mem(desc->dest_phys_addr, desc->length);

    /* Mark slot pending. */
    s_dma_done[slot] = 0;

    /* DSB ST: ensure descriptor is written to memory before DMA arm. */
    __asm__ volatile("dsb st" ::: "memory");

    /* Arm PL330 DMA engine on channel 0. */
    pl330_start_channel0(desc_pa);

    return 0;
}

/**
 * niyah_dma_poll — Poll DMA completion for a submitted descriptor.
 *
 * @param desc_pa  Same guest PA passed to niyah_dma_submit.
 * @return         1 if complete, 0 if pending, -1 if invalid.
 */
int niyah_dma_poll(uint32_t desc_pa)
{
    if (desc_pa < (uint32_t)(uintptr_t)s_desc_ring ||
        desc_pa >= (uint32_t)(uintptr_t)(s_desc_ring + DMA_SLOT_COUNT))
        return -1;

    uint32_t slot = SLOT_FROM_PA(desc_pa);
    if (slot >= DMA_SLOT_COUNT) return -1;

    /* Check DMAC interrupt status register for channel 0 completion. */
    uint32_t status = mmio_r32(DMA_INTSTATUS);
    if (status & (1u << 0u)) {
        /* Clear interrupt. */
        mmio_w32(DMA_INTCLR, 1u << 0u);
        /* DMB OSH: ensure NPU sees completed transfer before processing. */
        __asm__ volatile("dmb osh" ::: "memory");
        s_dma_done[slot] = 1;
    }

    return s_dma_done[slot] ? 1 : 0;
}
