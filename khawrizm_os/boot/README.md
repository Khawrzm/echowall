# Khawrizm OS — L0 Verified Boot Chain

## What this is

Bare-metal AArch64 C11 implementation of the **Khawrizm OS Stage BL2 verified boot chain**.

The boot sequence:

```
BootROM (BL0, burned in silicon)
    └─> BL1 (Trusted ROM, OTP-fused)
            └─> verified_boot.c (BL2 — this file)
                    └─> haven_kernel.bin (HAVEN microkernel)
                            └─> Niyah Engine (Executive + Phalanx Gate)
```

## Cryptographic verification

| Step | Operation | Algorithm |
|---|---|---|
| 1 | Read hardware public key | OTP fuses / NXP SE051 I2C |
| 2 | Hash kernel image | SHA-256 (self-contained, FIPS 180-4) |
| 3 | Verify kernel signature | Ed25519 (SE051 hardware offload) |
| 4 | Pass/Halt decision | `panic()` → `mmio_halt()` on any failure |

## Kernel image layout

```
[ kernel_header_t (16 bytes) ]
    magic:    0x4B57524D  ('KWRM')
    version:  uint32_t
    img_size: uint32_t (kernel bytes after header)
    reserved: uint32_t
[ kernel code (img_size bytes) ]
[ Ed25519 signature (64 bytes) ]
```

The signature covers: `SHA-256(header || kernel_code)`.

## HAL requirements

Three board-specific functions must be provided by the BSP:

```c
void otp_read_pubkey(uint8_t pubkey[32]);
int  se051_verify_ed25519(const uint8_t *msg, size_t len,
                          const uint8_t sig[64],
                          const uint8_t pubkey[32]);
void mmio_halt(void) __attribute__((noreturn));
```

## Sovereignty guarantee

If `se051_verify_ed25519()` returns non-zero **for any reason** — corrupted
image, wrong key, replay attack, bitflip — execution reaches:

```c
panic("SOVEREIGNTY BREACH: UNTRUSTED KERNEL PAYLOAD");
```

Which masks all IRQs and calls `mmio_halt()`. There is no recovery path.
A hardware reset is required. This is intentional.
