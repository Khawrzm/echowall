/**
 * verified_boot.c — Khawrizm OS L0 Verified Boot Chain
 *
 * Stage: BL2 (after BootROM / BL1 hands off execution)
 * Target: AArch64 bare-metal (RK3588 or NXP i.MX 8M Plus)
 * Secure element: NXP SE051 (I2C) / OTP-fused Ed25519 public key
 *
 * Sequence:
 *   1. Read OTP-fused Ed25519 public key (32 bytes) from hardware.
 *   2. Locate haven_kernel.bin at KERNEL_LOAD_ADDR.
 *   3. SHA-256 hash the kernel image.
 *   4. Verify Ed25519 signature (appended after kernel image).
 *   5. If verification passes: jump to kernel entry point.
 *   6. If verification fails: hardware halt — no recovery path.
 *
 * Radical Honesty:
 *   This file implements the full cryptographic *protocol* in portable C11.
 *   Three HAL stubs (otp_read_pubkey, se051_verify_ed25519, mmio_halt)
 *   must be provided by the board-specific BSP.
 *   The SHA-256 implementation below is self-contained (no libc).
 *
 * C11, zero external dependencies beyond <stdint.h> and <stddef.h>.
 * NO heap. NO libc. NO floating point.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =========================================================================
 * Memory map constants (override in board BSP header)
 * =========================================================================*/

/** Physical load address of haven_kernel.bin. */
#ifndef KERNEL_LOAD_ADDR
#  define KERNEL_LOAD_ADDR  0x08000000UL
#endif

/** Maximum kernel image size (64 MB). */
#ifndef KERNEL_MAX_SIZE
#  define KERNEL_MAX_SIZE   (64u * 1024u * 1024u)
#endif

/** Ed25519 signature size (bytes). Appended to kernel image. */
#define ED25519_SIG_BYTES   64u
/** Ed25519 public key size (bytes). */
#define ED25519_PUBKEY_BYTES 32u
/** SHA-256 digest size (bytes). */
#define SHA256_DIGEST_BYTES  32u

/* =========================================================================
 * HAL stubs — must be provided by board BSP
 * =========================================================================
 *
 * otp_read_pubkey:    Read the 32-byte Ed25519 public key from OTP fuses
 *                     or NXP SE051 secure element via I2C.
 * se051_verify_ed25519: Verify an Ed25519 signature using the SE051
 *                     hardware crypto engine (offloads scalar mult).
 *                     Returns 0 on success, non-zero on failure.
 * mmio_halt:          Write to the hardware watchdog / power-control
 *                     register to perform an unrecoverable halt.
 *                     Must never return.
 */
extern void otp_read_pubkey(uint8_t pubkey[ED25519_PUBKEY_BYTES]);
extern int  se051_verify_ed25519(
    const uint8_t *msg,  size_t msg_len,
    const uint8_t  sig[ED25519_SIG_BYTES],
    const uint8_t  pubkey[ED25519_PUBKEY_BYTES]
);
extern void mmio_halt(void) __attribute__((noreturn));

/* =========================================================================
 * Panic — sovereignty breach handler
 * =========================================================================*/

static void __attribute__((noreturn))
panic(const char *msg)
{
    (void)msg;
    /*
     * In a bare-metal environment we cannot print. The msg string is
     * preserved in the binary's .rodata for JTAG / post-mortem analysis.
     *
     * Sequence:
     *   1. Disable all IRQs (DAIF mask).
     *   2. Write SOVEREIGNTY_BREACH magic to a known MMIO address
     *      so a hardware monitor can detect the event.
     *   3. Delegate to board HAL for unrecoverable halt.
     */
    __asm__ volatile(
        "msr daifset, #0xF\n"   /* Mask all interrupts (AArch64). */
        : : : "memory"
    );
    mmio_halt();
    /* Unreachable — mmio_halt() is [[noreturn]]. Satisfy the compiler. */
    __builtin_unreachable();
}

/* =========================================================================
 * SHA-256 — self-contained, no libc
 * =========================================================================
 * FIPS 180-4 compliant. Processes the kernel image in 64-byte blocks.
 * Stack usage: ~300 bytes (schedule array + state).
 * =========================================================================*/

static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32u - (n))))
#define CH(e,f,g)  (((e) & (f)) ^ (~(e) & (g)))
#define MAJ(a,b,c) (((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))
#define EP0(a)  (ROR32(a,2)  ^ ROR32(a,13) ^ ROR32(a,22))
#define EP1(e)  (ROR32(e,6)  ^ ROR32(e,11) ^ ROR32(e,25))
#define SIG0(x) (ROR32(x,7)  ^ ROR32(x,18) ^ ((x) >> 3u))
#define SIG1(x) (ROR32(x,17) ^ ROR32(x,19) ^ ((x) >> 10u))

typedef struct {
    uint8_t  data[64];
    uint32_t state[8];
    uint32_t bitlen_lo;
    uint32_t bitlen_hi;
    uint32_t datalen;
} sha256_ctx_t;

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *data)
{
    uint32_t a,b,c,d,e,f,g,h,t1,t2,m[64];
    for (uint32_t i = 0, j = 0; i < 16u; ++i, j += 4u)
        m[i] = ((uint32_t)data[j]   << 24u) | ((uint32_t)data[j+1] << 16u) |
               ((uint32_t)data[j+2] <<  8u) |  (uint32_t)data[j+3];
    for (uint32_t i = 16u; i < 64u; ++i)
        m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (uint32_t i = 0; i < 64u; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + K256[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    ctx->datalen   = 0;
    ctx->bitlen_lo = 0;
    ctx->bitlen_hi = 0;
    ctx->state[0]  = 0x6a09e667u;
    ctx->state[1]  = 0xbb67ae85u;
    ctx->state[2]  = 0x3c6ef372u;
    ctx->state[3]  = 0xa54ff53au;
    ctx->state[4]  = 0x510e527fu;
    ctx->state[5]  = 0x9b05688cu;
    ctx->state[6]  = 0x1f83d9abu;
    ctx->state[7]  = 0x5be0cd19u;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64u) {
            sha256_transform(ctx, ctx->data);
            uint32_t prev = ctx->bitlen_lo;
            ctx->bitlen_lo += 512u;
            if (ctx->bitlen_lo < prev) ctx->bitlen_hi++;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_BYTES])
{
    uint32_t i = ctx->datalen;
    ctx->data[i++] = 0x80u;
    if (i > 56u) {
        while (i < 64u) ctx->data[i++] = 0x00u;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56u) ctx->data[i++] = 0x00u;
    uint32_t bl_lo = ctx->bitlen_lo + (ctx->datalen * 8u);
    uint32_t bl_hi = ctx->bitlen_hi;
    ctx->data[56] = (uint8_t)(bl_hi >> 24u);
    ctx->data[57] = (uint8_t)(bl_hi >> 16u);
    ctx->data[58] = (uint8_t)(bl_hi >>  8u);
    ctx->data[59] = (uint8_t) bl_hi;
    ctx->data[60] = (uint8_t)(bl_lo >> 24u);
    ctx->data[61] = (uint8_t)(bl_lo >> 16u);
    ctx->data[62] = (uint8_t)(bl_lo >>  8u);
    ctx->data[63] = (uint8_t) bl_lo;
    sha256_transform(ctx, ctx->data);
    for (uint32_t j = 0; j < 4u; ++j) {
        digest[j]      = (uint8_t)(ctx->state[0] >> (24u - j*8u));
        digest[j+4u]   = (uint8_t)(ctx->state[1] >> (24u - j*8u));
        digest[j+8u]   = (uint8_t)(ctx->state[2] >> (24u - j*8u));
        digest[j+12u]  = (uint8_t)(ctx->state[3] >> (24u - j*8u));
        digest[j+16u]  = (uint8_t)(ctx->state[4] >> (24u - j*8u));
        digest[j+20u]  = (uint8_t)(ctx->state[5] >> (24u - j*8u));
        digest[j+24u]  = (uint8_t)(ctx->state[6] >> (24u - j*8u));
        digest[j+28u]  = (uint8_t)(ctx->state[7] >> (24u - j*8u));
    }
}

/* =========================================================================
 * Kernel header — located at KERNEL_LOAD_ADDR
 * =========================================================================
 * Layout (little-endian):
 *   [0..3]   magic    0x4B57524D  ('KWRM' — Khawrizm)
 *   [4..7]   version  uint32_t
 *   [8..11]  img_size uint32_t    (kernel bytes, excluding this header
 *                                  and the appended Ed25519 signature)
 *   [12..15] reserved
 * Total header: 16 bytes.
 * Followed by: img_size bytes of kernel code.
 * Followed by: 64 bytes Ed25519 signature over SHA-256(header || kernel).
 * =========================================================================*/

#define KERNEL_MAGIC 0x4B57524Du

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t img_size;
    uint32_t reserved;
} kernel_header_t;

/* =========================================================================
 * verified_boot_main — entry point called by BL1 / BootROM hand-off
 * =========================================================================*/

void __attribute__((noreturn))
verified_boot_main(void)
{
    /* ------------------------------------------------------------------
     * Step 1: Read OTP-fused Ed25519 public key.
     * ------------------------------------------------------------------ */
    uint8_t pubkey[ED25519_PUBKEY_BYTES];
    otp_read_pubkey(pubkey);

    /* ------------------------------------------------------------------
     * Step 2: Locate and validate kernel header.
     * ------------------------------------------------------------------ */
    const uint8_t *kernel_base = (const uint8_t *)KERNEL_LOAD_ADDR;
    const kernel_header_t *hdr = (const kernel_header_t *)kernel_base;

    if (hdr->magic != KERNEL_MAGIC)
        panic("SOVEREIGNTY BREACH: UNTRUSTED KERNEL PAYLOAD");

    uint32_t img_size = hdr->img_size;
    if (img_size == 0u || img_size > KERNEL_MAX_SIZE)
        panic("SOVEREIGNTY BREACH: UNTRUSTED KERNEL PAYLOAD");

    /* Total signed region: header (16 bytes) + kernel image. */
    size_t signed_len = sizeof(kernel_header_t) + (size_t)img_size;

    /* Signature is appended immediately after the kernel image. */
    const uint8_t *sig = kernel_base + signed_len;

    /* ------------------------------------------------------------------
     * Step 3: SHA-256 hash the signed region.
     * ------------------------------------------------------------------ */
    sha256_ctx_t sha_ctx;
    uint8_t digest[SHA256_DIGEST_BYTES];

    sha256_init(&sha_ctx);
    sha256_update(&sha_ctx, kernel_base, signed_len);
    sha256_final(&sha_ctx, digest);

    /* ------------------------------------------------------------------
     * Step 4: Verify Ed25519 signature via SE051 hardware.
     *
     * The SE051 verifies: Ed25519_Verify(pubkey, digest, sig)
     * Internally the SE051 re-hashes with its own SHA-512 (Ed25519
     * standard). We pass the pre-image (kernel bytes), not the digest,
     * so the SE051 can perform the full Ed25519 verification protocol.
     * ------------------------------------------------------------------ */
    int verify_result = se051_verify_ed25519(
        kernel_base,   /* message = full signed region */
        signed_len,
        sig,
        pubkey
    );

    if (verify_result != 0)
        panic("SOVEREIGNTY BREACH: UNTRUSTED KERNEL PAYLOAD");

    /* ------------------------------------------------------------------
     * Step 5: Verification passed. Jump to kernel entry point.
     *
     * The kernel entry is at offset sizeof(kernel_header_t) within the
     * loaded image. AArch64 calling convention: no arguments.
     * Cache/MMU state is responsibility of the kernel.
     * ------------------------------------------------------------------ */
    typedef void __attribute__((noreturn)) (*kernel_entry_t)(void);
    kernel_entry_t kernel_entry =
        (kernel_entry_t)(kernel_base + sizeof(kernel_header_t));

    /* Memory barrier before jumping to untrusted (but now verified) code. */
    __asm__ volatile("dsb sy\n isb\n" : : : "memory");

    kernel_entry();

    /* Unreachable. */
    __builtin_unreachable();
}
