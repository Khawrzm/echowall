/**
 * rv32ima.h — RISC-V RV32IMA emulator interface
 *
 * Bare-metal, dependency-free C11 emulator for the RV32IMA instruction set.
 * Targets ~500 lines of C. No dynamic allocation. All state in rv32_cpu_t.
 *
 * Supported extensions:
 *   I  — Base integer (all 47 instructions)
 *   M  — Integer multiply/divide (MUL, MULH, MULHU, MULHSU, DIV, DIVU, REM, REMU)
 *   A  — Atomic (LR.W, SC.W, AMOSWAP, AMOADD, AMOXOR, AMOAND, AMOOR,
 *              AMOMIN, AMOMAX, AMOMINU, AMOMAXU)
 *
 * Privilege: Machine mode only (no S/U mode — microkernel runs in M-mode).
 * CSRs: mstatus, mtvec, mepc, mcause, mscratch, mie, mip (minimal set).
 *
 * C11, zero external dependencies. No libc, no heap.
 */
#ifndef HAVEN_RV32IMA_H
#define HAVEN_RV32IMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------*/

#define RV32_NUM_REGS   32u
#define RV32_MEM_BASE   0x80000000u   /* WASM linear memory base (guest PA) */
#define RV32_MEM_SIZE   (4u * 1024u * 1024u)   /* 4 MB guest address space */
#define RV32_RESET_PC   RV32_MEM_BASE

/* CSR addresses (minimal M-mode set). */
#define CSR_MSTATUS     0x300u
#define CSR_MTVEC       0x305u
#define CSR_MSCRATCH    0x340u
#define CSR_MEPC        0x341u
#define CSR_MCAUSE      0x342u
#define CSR_MIE         0x304u
#define CSR_MIP         0x344u

/* Exception/interrupt causes. */
#define CAUSE_MISALIGN_FETCH    0x00000000u
#define CAUSE_ILLEGAL_INSN      0x00000002u
#define CAUSE_BREAKPOINT        0x00000003u
#define CAUSE_MISALIGN_LOAD     0x00000004u
#define CAUSE_FAULT_LOAD        0x00000005u
#define CAUSE_MISALIGN_STORE    0x00000006u
#define CAUSE_FAULT_STORE       0x00000007u
#define CAUSE_ECALL_M           0x0000000Bu

/* -------------------------------------------------------------------------
 * Halt codes (written to halt_code on rv32_step() return false)
 * -------------------------------------------------------------------------*/
typedef enum {
    RV32_HALT_NONE       = 0,
    RV32_HALT_ECALL      = 1,   /* Guest issued ECALL (microkernel handles) */
    RV32_HALT_EBREAK     = 2,   /* Guest breakpoint */
    RV32_HALT_ILLEGAL    = 3,   /* Illegal instruction */
    RV32_HALT_FAULT      = 4,   /* Memory fault (out-of-bounds access) */
    RV32_HALT_WFI        = 5,   /* Guest executed WFI (idle) */
    RV32_HALT_REQUESTED  = 6,   /* Explicit halt via x0-write sentinel */
} rv32_halt_t;

/* -------------------------------------------------------------------------
 * CPU state
 * -------------------------------------------------------------------------*/
typedef struct {
    uint32_t  x[RV32_NUM_REGS];   /* x0..x31 (x0 always reads 0) */
    uint32_t  pc;                  /* Program counter */

    /* Minimal CSR set. */
    uint32_t  mstatus;
    uint32_t  mtvec;
    uint32_t  mscratch;
    uint32_t  mepc;
    uint32_t  mcause;
    uint32_t  mie;
    uint32_t  mip;

    /* Guest memory: WASM linear memory boundary. */
    uint8_t  *mem;                 /* Host pointer to guest address space */
    uint32_t  mem_size;            /* Must equal RV32_MEM_SIZE */

    /* Emulator state. */
    rv32_halt_t halt_code;
    uint64_t    cycle_count;

    /* Reservation set for LR/SC atomics (guest PA, or UINT32_MAX = none). */
    uint32_t  reservation;
} rv32_cpu_t;

/* -------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------*/

/**
 * rv32_init — Initialize CPU state.
 *
 * @param cpu       CPU state to initialize.
 * @param mem       Host buffer for guest address space (must be RV32_MEM_SIZE).
 * @param mem_size  Must equal RV32_MEM_SIZE.
 */
void rv32_init(rv32_cpu_t *cpu, uint8_t *mem, uint32_t mem_size);

/**
 * rv32_load_image — Copy a guest binary into guest memory.
 *
 * @param cpu       Initialized CPU.
 * @param image     Guest binary (RV32IMA ELF or flat binary).
 * @param image_len Byte length of image. Must be <= RV32_MEM_SIZE.
 * @param load_pa   Guest physical address to load at (relative to MEM_BASE).
 * @return          0 on success, -1 if image_len exceeds memory.
 */
int rv32_load_image(
    rv32_cpu_t   *cpu,
    const uint8_t *image,
    uint32_t      image_len,
    uint32_t      load_pa
);

/**
 * rv32_step — Execute a single RV32IMA instruction.
 *
 * @param cpu  CPU state.
 * @return     true  — instruction executed, continue.
 *             false — halt condition; check cpu->halt_code.
 */
bool rv32_step(rv32_cpu_t *cpu);

/**
 * rv32_run — Execute up to `max_cycles` instructions.
 *
 * Returns when a halt condition is encountered or max_cycles is reached.
 * @param cpu        CPU state.
 * @param max_cycles Maximum instructions to execute (0 = unlimited).
 * @return           halt code.
 */
rv32_halt_t rv32_run(rv32_cpu_t *cpu, uint64_t max_cycles);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_RV32IMA_H */
