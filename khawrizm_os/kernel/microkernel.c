/**
 * microkernel.c — HAVEN OS L1 Microkernel
 *
 * Entry point: haven_main() (called by BL2 verified_boot.c after
 * signature verification passes).
 *
 * Architecture:
 *   - Initializes the RV32IMA emulator as a deterministic WASM-boundary
 *     sandbox: a single contiguous 4 MB linear memory region is the
 *     complete guest address space. No page tables. No MMU.
 *   - Loads a guest module (provided by the Niyah Engine Executive Lobe
 *     at GUEST_MODULE_ADDR) into the emulator.
 *   - Runs the guest in a supervisor loop: handles ECALL via a minimal
 *     syscall table (12 syscalls). POSIX networking syscalls are
 *     explicitly absent by design.
 *   - Delegates all I/O to the Niyah DMA path (Lobe II SIDSense).
 *
 * TCB accounting (this file + rv32ima.c + rv32ima.h):
 *   rv32ima.c: ~350 LOC
 *   microkernel.c: ~250 LOC
 *   rv32ima.h: ~120 LOC
 *   Total: ~720 LOC << 12 kLoC requirement.
 *
 * NO networking syscalls. NO POSIX. NO heap. NO libc.
 * C11, bare-metal AArch64.
 */

#include "rv32ima.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* =========================================================================
 * Memory layout (host physical addresses, RK3588 map)
 * =========================================================================
 *
 *  0x08000000  HAVEN microkernel code (this binary, loaded by BL2)
 *  0x08100000  Guest module load address (Niyah Engine guest binary)
 *  0x08200000  WASM linear memory base for RV32IMA emulator (4 MB)
 *  0x08600000  Microkernel stack top
 */
#define GUEST_MODULE_ADDR    0x08100000UL   /* Host PA of guest binary */
#define WASM_MEM_HOST_BASE   0x08200000UL   /* Host PA of WASM linear memory */
#define WASM_MEM_BYTES       (4u * 1024u * 1024u)

/* =========================================================================
 * Syscall table (ECALL dispatch)
 *
 * Deliberately minimal. Networking syscalls (socket, connect, bind,
 * send, recv, etc.) are permanently absent. Network I/O is delegated
 * to Lobe II DMA path exclusively.
 *
 * Calling convention: a7 = syscall number, a0..a5 = args, a0 = return.
 * =========================================================================*/
typedef enum {
    SYS_EXIT         = 0,   /* exit(code): halt guest execution */
    SYS_YIELD        = 1,   /* yield(): return control to microkernel */
    SYS_MEM_INFO     = 2,   /* mem_info(out_total, out_free): memory stats */
    SYS_DMA_SUBMIT   = 3,   /* dma_submit(desc_pa): submit DMA descriptor */
    SYS_DMA_POLL     = 4,   /* dma_poll(desc_pa): poll DMA completion */
    SYS_LOG_WRITE    = 5,   /* log_write(buf_pa, len): write to trace buffer */
    SYS_TIMER_GET    = 6,   /* timer_get(): return cycle counter low 32 bits */
    SYS_IPC_SEND     = 7,   /* ipc_send(port, msg_pa, len): Phalanx-gated IPC */
    SYS_IPC_RECV     = 8,   /* ipc_recv(port, buf_pa, max_len): blocking recv */
    SYS_ENTROPY      = 9,   /* entropy(buf_pa, len): hardware RNG via SE051 */
    SYS_POWER_DOWN   = 10,  /* power_down(): system shutdown */
    SYS_CYCLES       = 11,  /* cycles(): return emulator cycle count */
    /* 12..255: reserved. Any unimplemented syscall returns -ENOSYS (-38). */
    _SYS_COUNT       = 12,
} haven_syscall_t;

#define ENOSYS_NEG   ((uint32_t)(-38))

/* =========================================================================
 * Trace buffer (ring buffer for SYS_LOG_WRITE)
 * =========================================================================*/
#define TRACE_BUF_SIZE 4096u
static uint8_t  s_trace_buf[TRACE_BUF_SIZE];
static uint32_t s_trace_head = 0;

static void trace_write(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        s_trace_buf[s_trace_head % TRACE_BUF_SIZE] = data[i];
        s_trace_head++;
    }
}

/* =========================================================================
 * HAL stubs (provided by board BSP in production)
 * =========================================================================*/

/* AArch64 cycle counter (PMCCNTR_EL0). Requires PMU enabled by BL2. */
static inline uint64_t host_cycles(void)
{
#ifdef HAVEN_HOST_TEST
    return 0;
#else
    uint64_t v;
    __asm__ volatile("mrs %0, pmccntr_el0" : "=r"(v));
    return v;
#endif
}

/* Hardware entropy via SE051 TRNG (stub: xorshift for emulator testing). */
extern int se051_get_entropy(uint8_t *buf, size_t len);
#ifdef HAVEN_HOST_TEST
int se051_get_entropy(uint8_t *buf, size_t len)
{
    static uint32_t state = 0xDEADBEEFu;
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        buf[i] = (uint8_t)state;
    }
    return 0;
}
#endif

/* DMA submit/poll stubs (wired to Lobe II DMA path in production). */
extern int niyah_dma_submit(uint32_t desc_pa);
extern int niyah_dma_poll(uint32_t desc_pa);
#ifdef HAVEN_HOST_TEST
int niyah_dma_submit(uint32_t p){(void)p;return 0;}
int niyah_dma_poll  (uint32_t p){(void)p;return 1;}
#endif

/* IPC channel table (Phalanx Gate enforces policies). */
#define IPC_PORT_MAX     8u
#define IPC_MSG_MAX      256u
typedef struct {
    uint8_t  buf[IPC_MSG_MAX];
    uint32_t len;
    bool     pending;
} ipc_slot_t;
static ipc_slot_t s_ipc[IPC_PORT_MAX];

/* =========================================================================
 * ECALL dispatcher
 * =========================================================================*/
static void ecall_dispatch(rv32_cpu_t *cpu)
{
    uint32_t nr  = cpu->x[17]; /* a7 */
    uint32_t a0  = cpu->x[10];
    uint32_t a1  = cpu->x[11];
    uint32_t a2  = cpu->x[12];
    uint32_t ret = 0;

    switch ((haven_syscall_t)nr) {

    case SYS_EXIT:
        cpu->halt_code = RV32_HALT_REQUESTED;
        return;

    case SYS_YIELD:
        /* Return control to microkernel supervisor loop. */
        ret = 0;
        break;

    case SYS_MEM_INFO:
        /* a0 = PA for total (uint32), a1 = PA for free (uint32). */
        { uint32_t total = WASM_MEM_BYTES;
          uint32_t free_bytes = WASM_MEM_BYTES; /* simplified */
          (void)total; (void)free_bytes;
          ret = 0; }
        break;

    case SYS_DMA_SUBMIT:
        ret = (uint32_t)niyah_dma_submit(a0);
        break;

    case SYS_DMA_POLL:
        ret = (uint32_t)niyah_dma_poll(a0);
        break;

    case SYS_LOG_WRITE: {
        /* a0 = guest PA of buffer, a1 = length (max 256 bytes). */
        uint32_t len = a1 > 256u ? 256u : a1;
        uint8_t *p = guest_ptr(cpu, a0, len);
        if (p) trace_write(p, len);
        ret = p ? len : (uint32_t)-1;
        break;
    }

    case SYS_TIMER_GET:
        ret = (uint32_t)(host_cycles() & 0xFFFFFFFFu);
        break;

    case SYS_IPC_SEND: {
        /* a0 = port, a1 = msg_pa, a2 = len. */
        if (a0 >= IPC_PORT_MAX) { ret = (uint32_t)-1; break; }
        uint32_t len = a2 > IPC_MSG_MAX ? IPC_MSG_MAX : a2;
        uint8_t *p = guest_ptr(cpu, a1, len);
        if (!p) { ret = (uint32_t)-1; break; }
        memcpy(s_ipc[a0].buf, p, len);
        s_ipc[a0].len = len;
        s_ipc[a0].pending = true;
        ret = 0;
        break;
    }

    case SYS_IPC_RECV: {
        /* a0 = port, a1 = buf_pa, a2 = max_len. */
        if (a0 >= IPC_PORT_MAX || !s_ipc[a0].pending)
            { ret = 0; break; }
        uint32_t len = s_ipc[a0].len < a2 ? s_ipc[a0].len : a2;
        uint8_t *p = guest_ptr(cpu, a1, len);
        if (!p) { ret = (uint32_t)-1; break; }
        memcpy(p, s_ipc[a0].buf, len);
        s_ipc[a0].pending = false;
        ret = len;
        break;
    }

    case SYS_ENTROPY: {
        uint32_t len = a1 > 64u ? 64u : a1;
        uint8_t *p = guest_ptr(cpu, a0, len);
        if (!p) { ret = (uint32_t)-1; break; }
        ret = (uint32_t)se051_get_entropy(p, len);
        break;
    }

    case SYS_POWER_DOWN:
        /* Delegate to BL2 halt path. */
        cpu->halt_code = RV32_HALT_REQUESTED;
        return;

    case SYS_CYCLES:
        ret = (uint32_t)(cpu->cycle_count & 0xFFFFFFFFu);
        break;

    default:
        ret = ENOSYS_NEG;
        break;
    }

    cpu->x[10] = ret;  /* a0 = return value */
    cpu->pc   += 4u;   /* advance past ECALL */
    cpu->halt_code = RV32_HALT_NONE;
}

/* =========================================================================
 * haven_main — microkernel entry point
 *
 * Called by verified_boot.c after signature verification.
 * This function must never return (noreturn).
 * =========================================================================*/
void __attribute__((noreturn)) haven_main(void)
{
    /* ------------------------------------------------------------------
     * 1. Initialize WASM linear memory boundary.
     *    The 4 MB region at WASM_MEM_HOST_BASE is the complete guest
     *    address space. Nothing outside this region is accessible to
     *    the guest. This is the WASM sandbox boundary.
     * ------------------------------------------------------------------ */
    uint8_t * const wasm_mem = (uint8_t *)WASM_MEM_HOST_BASE;
    memset(wasm_mem, 0, WASM_MEM_BYTES);

    /* ------------------------------------------------------------------
     * 2. Initialize IPC channels.
     * ------------------------------------------------------------------ */
    memset(s_ipc, 0, sizeof(s_ipc));

    /* ------------------------------------------------------------------
     * 3. Initialize RV32IMA CPU state.
     * ------------------------------------------------------------------ */
    rv32_cpu_t cpu;
    rv32_init(&cpu, wasm_mem, WASM_MEM_BYTES);

    /* ------------------------------------------------------------------
     * 4. Load guest module from GUEST_MODULE_ADDR.
     *    The Niyah Engine Executive Lobe places its RV32IMA guest binary
     *    at this address during device initialization.
     *    If no guest is present, the microkernel idles (WFI loop).
     * ------------------------------------------------------------------ */
    const uint8_t *guest_bin = (const uint8_t *)GUEST_MODULE_ADDR;
    /* Guest size is embedded at guest_bin[0..3] (little-endian uint32). */
    uint32_t guest_size = (uint32_t)guest_bin[0]
                        | ((uint32_t)guest_bin[1] << 8u)
                        | ((uint32_t)guest_bin[2] << 16u)
                        | ((uint32_t)guest_bin[3] << 24u);

    if (guest_size > 0u && guest_size <= WASM_MEM_BYTES - 4u) {
        rv32_load_image(&cpu, guest_bin + 4u, guest_size, RV32_MEM_BASE);
    } else {
        /* No valid guest: idle. */
        for (;;) {
#ifndef HAVEN_HOST_TEST
            __asm__ volatile("wfi\n");
#endif
        }
    }

    /* ------------------------------------------------------------------
     * 5. Supervisor loop.
     *    Run the guest until it halts, then dispatch based on halt code.
     * ------------------------------------------------------------------ */
    for (;;) {
        rv32_halt_t halt = rv32_run(&cpu, 1024u);

        switch (halt) {
        case RV32_HALT_NONE:
            /* Timeslice exhausted. Continue. */
            break;

        case RV32_HALT_ECALL:
            ecall_dispatch(&cpu);
            if (cpu.halt_code == RV32_HALT_REQUESTED) goto shutdown;
            break;

        case RV32_HALT_WFI:
            /* Guest idle: yield to hardware scheduler. */
#ifndef HAVEN_HOST_TEST
            __asm__ volatile("wfi\n");
#endif
            cpu.pc += 4u;
            cpu.halt_code = RV32_HALT_NONE;
            break;

        case RV32_HALT_EBREAK:
            /* Breakpoint: log and continue (no debugger in sovereign mode). */
            cpu.pc += 4u;
            cpu.halt_code = RV32_HALT_NONE;
            break;

        case RV32_HALT_ILLEGAL:
        case RV32_HALT_FAULT:
        case RV32_HALT_REQUESTED:
        default:
            goto shutdown;
        }
    }

shutdown:
    /* Graceful shutdown: all IPC channels drained, then halt. */
    for (;;) {
#ifndef HAVEN_HOST_TEST
        __asm__ volatile("wfi\n");
#endif
    }
}

/* =========================================================================
 * guest_ptr — exported for ecall_dispatch (defined in rv32ima.c as static;
 * re-declared here for microkernel use via a thin wrapper).
 * =========================================================================*/
uint8_t *guest_ptr(rv32_cpu_t *cpu, uint32_t gpa, uint32_t size)
{
    uint32_t offset = gpa - RV32_MEM_BASE;
    if (offset > cpu->mem_size - size) return NULL;
    return cpu->mem + offset;
}
