# Khawrizm OS — BSP (Board Support Package)

Hardware integration layer for RK3588 SoC + NXP SE051 secure element.

## Files

| File | Purpose |
|---|---|
| `se051_i2c.c` | NXP SE051 (CC EAL6+) bare-metal I2C driver |
| `rk3588_dma.c` | RK3588 PL330 zero-copy DMA pipeline (SDR→NPU) |

## SE051 Hardware Root of Trust

The SE051 is connected to **I2C5** (RK3588, MMIO `0xFEC90000`)  
I2C address: `0x48` (NXP factory default)  
Protocol: ISO 7816-4 T=1 framing over I2C  
Speed: 400 kHz fast-mode (24 MHz input clock ÷ 8 ÷ 7 = ~428 kHz)

### Halt conditions (physical tamper detection)
Any of the following cause an **immediate, unrecoverable `mmio_halt()`**:
- I2C timeout (SE051 unresponsive)
- T=1 LRC checksum mismatch (bus tamper)
- NAK from SE051
- SE051 returns SW ≠ 9000 on verify command
- Signature verification failure

### Production note
Full production BSP must use **NXP Plug & Trust MW** (`se_hostlib`)  
for complete SCP03 + TLV encoding. The APDU encoding here covers  
the minimal boot-context path (Ed25519 verify + TRNG).

## RK3588 Zero-Copy DMA Pipeline

```
LimeSDR IQ FIFO (PHY: 0x0A000000)
    │
    │  DC CIVAC cache clean+invalidate (Cortex-A76)
    │  DSB ST
    ↓
PL330 DMA engine (CH0, DMAC0: 0xFF700000)
    │  DMAGO via debug interface
    │  CCR: non-cacheable, 64-byte burst
    ↓
NPU SRAM window (0xFF000000, 4 MB)
    │  rknn_create_mem_from_phys equiv.
    │  RKNPU SMMU bypass (0xFDAB0000+0x210)
    │  DMB OSH
    ↓
Niyah Engine Sensory Lobe (WorldStateTensor fusion)
```

**CPU cache involvement: zero.** The Cortex-A76 cores are not on the  
data path between the SDR and NPU after DMA is armed.

### Thermal guard
Every `niyah_dma_submit()` call reads `TSADC_DATA0` (`0xFE710000+0x20`).  
If the raw ADC value exceeds `0x2EE` (~85°C junction), `mmio_halt()` fires  
before the DMA is armed. This enforces the **15W passive cooling envelope**.

## Memory barriers summary

| Barrier | Location | Purpose |
|---|---|---|
| `dsb sy` | After every MMIO write | Ensure write visibility |
| `dsb st` | Before DMA arm | Drain all pending stores |
| `dc civac` | Source buffer range | Clean+invalidate D-cache |
| `dmb osh` | After DMA complete | NPU coherency barrier |
| `dsb sy; isb` | I2C init, OTP read | Pipeline flush |
