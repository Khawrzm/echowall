/**
 * rv32ima.c — RV32IMA emulator implementation
 *
 * ~500 lines of C11. Zero heap. Zero libc. Zero external dependencies.
 * All guest memory accesses are bounds-checked against the WASM linear
 * memory boundary (mem_size). Out-of-bounds = RV32_HALT_FAULT.
 *
 * Instruction decoding: single switch on opcode (bits [6:0]),
 * with nested switches on funct3/funct7 for disambiguation.
 */
#include "rv32ima.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* =========================================================================
 * Memory access helpers — all bounds-checked
 * =========================================================================*/

/* Guest PA -> host pointer. Returns NULL on out-of-bounds. */
static inline uint8_t *guest_ptr(rv32_cpu_t *cpu, uint32_t gpa, uint32_t size)
{
    uint32_t offset = gpa - RV32_MEM_BASE;
    if (offset > cpu->mem_size - size) return NULL;
    return cpu->mem + offset;
}

static inline bool mem_read8 (rv32_cpu_t *c, uint32_t a, uint8_t  *v)
{ uint8_t *p = guest_ptr(c,a,1); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} *v=*p; return true; }
static inline bool mem_read16(rv32_cpu_t *c, uint32_t a, uint16_t *v)
{ uint8_t *p = guest_ptr(c,a,2); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} memcpy(v,p,2); return true; }
static inline bool mem_read32(rv32_cpu_t *c, uint32_t a, uint32_t *v)
{ uint8_t *p = guest_ptr(c,a,4); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} memcpy(v,p,4); return true; }

static inline bool mem_write8 (rv32_cpu_t *c, uint32_t a, uint8_t  v)
{ uint8_t *p = guest_ptr(c,a,1); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} *p=v; return true; }
static inline bool mem_write16(rv32_cpu_t *c, uint32_t a, uint16_t v)
{ uint8_t *p = guest_ptr(c,a,2); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} memcpy(p,&v,2); return true; }
static inline bool mem_write32(rv32_cpu_t *c, uint32_t a, uint32_t v)
{ uint8_t *p = guest_ptr(c,a,4); if(!p){c->halt_code=RV32_HALT_FAULT;return false;} memcpy(p,&v,4); return true; }

/* =========================================================================
 * Immediate decoders
 * =========================================================================*/
static inline int32_t imm_I(uint32_t i){ return (int32_t)i >> 20; }
static inline int32_t imm_S(uint32_t i){
    return (int32_t)(((i>>20)&0xFE0u)|((i>>7)&0x1Fu)) << 20 >> 20; }
static inline int32_t imm_B(uint32_t i){
    return (int32_t)((((i>>31)&1u)<<12)|(((i>>7)&1u)<<11)|
                     (((i>>25)&0x3Fu)<<5)|(((i>>8)&0xFu)<<1)) << 19 >> 19; }
static inline int32_t imm_U(uint32_t i){ return (int32_t)(i & 0xFFFFF000u); }
static inline int32_t imm_J(uint32_t i){
    return (int32_t)((((i>>31)&1u)<<20)|(((i>>12)&0xFFu)<<12)|
                     (((i>>20)&1u)<<11)|(((i>>21)&0x3FFu)<<1)) << 11 >> 11; }

/* =========================================================================
 * CSR read/write
 * =========================================================================*/
static uint32_t csr_read(rv32_cpu_t *cpu, uint32_t csr)
{
    switch (csr) {
    case CSR_MSTATUS:  return cpu->mstatus;
    case CSR_MTVEC:    return cpu->mtvec;
    case CSR_MSCRATCH: return cpu->mscratch;
    case CSR_MEPC:     return cpu->mepc;
    case CSR_MCAUSE:   return cpu->mcause;
    case CSR_MIE:      return cpu->mie;
    case CSR_MIP:      return cpu->mip;
    default:           return 0;
    }
}

static void csr_write(rv32_cpu_t *cpu, uint32_t csr, uint32_t val)
{
    switch (csr) {
    case CSR_MSTATUS:  cpu->mstatus  = val; break;
    case CSR_MTVEC:    cpu->mtvec    = val; break;
    case CSR_MSCRATCH: cpu->mscratch = val; break;
    case CSR_MEPC:     cpu->mepc     = val; break;
    case CSR_MCAUSE:   cpu->mcause   = val; break;
    case CSR_MIE:      cpu->mie      = val; break;
    case CSR_MIP:      cpu->mip      = val; break;
    default: break;
    }
}

/* =========================================================================
 * rv32_init
 * =========================================================================*/
void rv32_init(rv32_cpu_t *cpu, uint8_t *mem, uint32_t mem_size)
{
    for (int i = 0; i < (int)RV32_NUM_REGS; i++) cpu->x[i] = 0;
    cpu->pc          = RV32_RESET_PC;
    cpu->mstatus     = 0;
    cpu->mtvec       = 0;
    cpu->mscratch    = 0;
    cpu->mepc        = 0;
    cpu->mcause      = 0;
    cpu->mie         = 0;
    cpu->mip         = 0;
    cpu->mem         = mem;
    cpu->mem_size    = mem_size;
    cpu->halt_code   = RV32_HALT_NONE;
    cpu->cycle_count = 0;
    cpu->reservation = UINT32_MAX;
    /* Stack pointer: top of guest memory. */
    cpu->x[2]        = RV32_MEM_BASE + mem_size - 4u;
}

/* =========================================================================
 * rv32_load_image
 * =========================================================================*/
int rv32_load_image(
    rv32_cpu_t *cpu, const uint8_t *image,
    uint32_t image_len, uint32_t load_pa)
{
    if (image_len > cpu->mem_size) return -1;
    uint32_t offset = load_pa - RV32_MEM_BASE;
    if (offset > cpu->mem_size - image_len) return -1;
    memcpy(cpu->mem + offset, image, image_len);
    return 0;
}

/* =========================================================================
 * rv32_step — single instruction execution
 * =========================================================================*/
bool rv32_step(rv32_cpu_t *cpu)
{
#define RD   ((insn >> 7)  & 0x1Fu)
#define RS1  ((insn >> 15) & 0x1Fu)
#define RS2  ((insn >> 20) & 0x1Fu)
#define F3   ((insn >> 12) & 0x7u)
#define F7   ((insn >> 25) & 0x7Fu)
#define XRS1 (cpu->x[RS1])
#define XRS2 (cpu->x[RS2])
#define WRD(v) do { if(RD) cpu->x[RD]=(v); } while(0)

    uint32_t insn;
    if (!mem_read32(cpu, cpu->pc, &insn)) return false;

    uint32_t pc_next = cpu->pc + 4u;
    uint32_t op = insn & 0x7Fu;

    switch (op) {

    /* ---- LUI ---- */
    case 0x37: WRD((uint32_t)imm_U(insn)); break;

    /* ---- AUIPC ---- */
    case 0x17: WRD(cpu->pc + (uint32_t)imm_U(insn)); break;

    /* ---- JAL ---- */
    case 0x6F:
        WRD(pc_next);
        pc_next = cpu->pc + (uint32_t)imm_J(insn);
        break;

    /* ---- JALR ---- */
    case 0x67:
        { uint32_t t = pc_next;
          pc_next = (XRS1 + (uint32_t)imm_I(insn)) & ~1u;
          WRD(t); }
        break;

    /* ---- BRANCH ---- */
    case 0x63: {
        int32_t s1=(int32_t)XRS1, s2=(int32_t)XRS2;
        bool taken = false;
        switch(F3){
        case 0: taken = (XRS1==XRS2); break;
        case 1: taken = (XRS1!=XRS2); break;
        case 4: taken = (s1<s2);      break;
        case 5: taken = (s1>=s2);     break;
        case 6: taken = (XRS1<XRS2);  break;
        case 7: taken = (XRS1>=XRS2); break;
        }
        if (taken) pc_next = cpu->pc + (uint32_t)imm_B(insn);
        break;
    }

    /* ---- LOAD ---- */
    case 0x03: {
        uint32_t addr = XRS1 + (uint32_t)imm_I(insn);
        switch(F3){
        case 0: { uint8_t v; if(!mem_read8(cpu,addr,&v))return false; WRD((uint32_t)(int8_t)v); break; }
        case 1: { uint16_t v; if(!mem_read16(cpu,addr,&v))return false; WRD((uint32_t)(int16_t)v); break; }
        case 2: { uint32_t v; if(!mem_read32(cpu,addr,&v))return false; WRD(v); break; }
        case 4: { uint8_t v; if(!mem_read8(cpu,addr,&v))return false; WRD(v); break; }
        case 5: { uint16_t v; if(!mem_read16(cpu,addr,&v))return false; WRD(v); break; }
        default: goto illegal;
        }
        break;
    }

    /* ---- STORE ---- */
    case 0x23: {
        uint32_t addr = XRS1 + (uint32_t)imm_S(insn);
        switch(F3){
        case 0: if(!mem_write8(cpu,addr,(uint8_t)XRS2))return false; break;
        case 1: if(!mem_write16(cpu,addr,(uint16_t)XRS2))return false; break;
        case 2: if(!mem_write32(cpu,addr,XRS2))return false; break;
        default: goto illegal;
        }
        break;
    }

    /* ---- OP-IMM ---- */
    case 0x13: {
        int32_t imm = imm_I(insn);
        uint32_t shamt = (insn >> 20) & 0x1Fu;
        switch(F3){
        case 0: WRD(XRS1+(uint32_t)imm); break;
        case 1: WRD(XRS1<<shamt); break;
        case 2: WRD((int32_t)XRS1<imm?1:0); break;
        case 3: WRD(XRS1<(uint32_t)imm?1:0); break;
        case 4: WRD(XRS1^(uint32_t)imm); break;
        case 5: WRD(F7?((int32_t)XRS1>>shamt):(XRS1>>shamt)); break;
        case 6: WRD(XRS1|(uint32_t)imm); break;
        case 7: WRD(XRS1&(uint32_t)imm); break;
        }
        break;
    }

    /* ---- OP (R-type + M-ext) ---- */
    case 0x33: {
        uint32_t a=XRS1, b=XRS2;
        int32_t sa=(int32_t)a, sb=(int32_t)b;
        if (F7 == 1u) { /* M extension */
            switch(F3){
            case 0: WRD((uint32_t)((int64_t)sa*(int64_t)sb)); break;
            case 1: WRD((uint32_t)(((int64_t)sa*(int64_t)sb)>>32)); break;
            case 2: WRD((uint32_t)(((int64_t)sa*(uint64_t)b)>>32)); break;
            case 3: WRD((uint32_t)(((uint64_t)a*(uint64_t)b)>>32)); break;
            case 4: WRD(sb?((sa==INT32_MIN&&sb==-1)?sa:(uint32_t)(sa/sb)):~0u); break;
            case 5: WRD(b?(a/b):~0u); break;
            case 6: WRD(sb?((sa==INT32_MIN&&sb==-1)?0:(uint32_t)(sa%sb)):a); break;
            case 7: WRD(b?(a%b):a); break;
            }
        } else {
            switch(F3|(F7?0x10u:0u)){
            case 0x00: WRD(a+b);   break;
            case 0x10: WRD(a-b);   break;
            case 0x01: WRD(a<<(b&31u)); break;
            case 0x02: WRD(sa<sb?1u:0u); break;
            case 0x03: WRD(a<b?1u:0u); break;
            case 0x04: WRD(a^b);   break;
            case 0x05: WRD(a>>(b&31u)); break;
            case 0x15: WRD((uint32_t)(sa>>(b&31u))); break;
            case 0x06: WRD(a|b);   break;
            case 0x07: WRD(a&b);   break;
            default:   goto illegal;
            }
        }
        break;
    }

    /* ---- MISC-MEM (FENCE) — no-op in emulator ---- */
    case 0x0F: break;

    /* ---- SYSTEM ---- */
    case 0x73: {
        uint32_t funct12 = insn >> 20;
        if (F3 == 0) {
            if (funct12 == 0) { cpu->halt_code = RV32_HALT_ECALL;  return false; }
            if (funct12 == 1) { cpu->halt_code = RV32_HALT_EBREAK; return false; }
            if (funct12 == 0x105) { cpu->halt_code = RV32_HALT_WFI; return false; } /* WFI */
            if (funct12 == 0x302) { /* MRET */
                pc_next = cpu->mepc;
                cpu->mstatus = (cpu->mstatus & ~0x8u) | ((cpu->mstatus >> 4u) & 0x8u);
                break;
            }
        }
        /* CSR instructions */
        uint32_t csr = insn >> 20;
        uint32_t old = csr_read(cpu, csr);
        uint32_t src = (F3 & 4u) ? RS1 : XRS1;
        WRD(old);
        switch(F3 & 3u){
        case 1: csr_write(cpu, csr, src); break;
        case 2: csr_write(cpu, csr, old|src); break;
        case 3: csr_write(cpu, csr, old&~src); break;
        }
        break;
    }

    /* ---- AMO (A extension) ---- */
    case 0x2F: {
        if (F3 != 2u) goto illegal; /* Only 32-bit AMOs */
        uint32_t funct5 = insn >> 27;
        uint32_t addr = XRS1;
        uint32_t val;
        if (funct5 == 2u) { /* LR.W */
            if (!mem_read32(cpu, addr, &val)) return false;
            cpu->reservation = addr;
            WRD(val);
            break;
        }
        if (funct5 == 3u) { /* SC.W */
            if (cpu->reservation == addr) {
                if (!mem_write32(cpu, addr, XRS2)) return false;
                cpu->reservation = UINT32_MAX;
                WRD(0);
            } else {
                WRD(1);
            }
            break;
        }
        cpu->reservation = UINT32_MAX;
        if (!mem_read32(cpu, addr, &val)) return false;
        uint32_t res = val;
        int32_t sv=(int32_t)val, sr=(int32_t)XRS2;
        switch(funct5){
        case 0x01: res=XRS2;               break; /* AMOSWAP */
        case 0x00: res=val+XRS2;           break; /* AMOADD */
        case 0x04: res=val^XRS2;           break; /* AMOXOR */
        case 0x0C: res=val&XRS2;           break; /* AMOAND */
        case 0x08: res=val|XRS2;           break; /* AMOOR */
        case 0x10: res=(uint32_t)(sv<sr?sv:sr); break; /* AMOMIN */
        case 0x14: res=(uint32_t)(sv>sr?sv:sr); break; /* AMOMAX */
        case 0x18: res=val<XRS2?val:XRS2;  break; /* AMOMINU */
        case 0x1C: res=val>XRS2?val:XRS2;  break; /* AMOMAXU */
        default: goto illegal;
        }
        if (!mem_write32(cpu, addr, res)) return false;
        WRD(val);
        break;
    }

    default:
    illegal:
        cpu->halt_code = RV32_HALT_ILLEGAL;
        cpu->mcause    = CAUSE_ILLEGAL_INSN;
        cpu->mepc      = cpu->pc;
        return false;
    }

    cpu->x[0]        = 0; /* x0 is hardwired zero */
    cpu->pc          = pc_next;
    cpu->cycle_count++;
    cpu->halt_code   = RV32_HALT_NONE;
    return true;

#undef RD
#undef RS1
#undef RS2
#undef F3
#undef F7
#undef XRS1
#undef XRS2
#undef WRD
}

/* =========================================================================
 * rv32_run
 * =========================================================================*/
rv32_halt_t rv32_run(rv32_cpu_t *cpu, uint64_t max_cycles)
{
    uint64_t limit = max_cycles ? max_cycles : UINT64_MAX;
    while (limit--) {
        if (!rv32_step(cpu)) return cpu->halt_code;
    }
    return RV32_HALT_NONE;
}
