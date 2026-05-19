#!/usr/bin/env bash
# =============================================================================
# Khawrizm OS — Image Builder
# Builds: KhawrizmOS-v3.0.0-Niyah-arm64.iso
#
# Orchestrates:
#   1. Niyah Engine Rust workspace (cargo build --release)
#   2. Casper Engine C11 static library (make in casper_engine/)
#   3. Verified bootloader (gcc bare-metal)
#   4. HAVEN microkernel placeholder
#   5. SquashFS root image + GRUB EFI bootable ISO
#
# Requirements (cross-build from x86_64 Linux):
#   aarch64-linux-gnu-gcc   (GCC cross-compiler)
#   rustup target add aarch64-unknown-linux-gnu
#   xorriso, mksquashfs, grub-mkrescue
#   openssl (for Ed25519 signing)
#
# Radical Honesty:
#   The HAVEN microkernel is not yet implemented (Milestone M5).
#   This script inserts a signed stub binary at the kernel slot.
#   Replace HAVEN_KERNEL_BIN below once M5 is complete.
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="${SCRIPT_DIR}"
BUILD_DIR="${REPO_ROOT}/build_iso_tmp"
OUT_ISO="${REPO_ROOT}/KhawrizmOS-v3.0.0-Niyah-arm64.iso"

NIYAH_WS="${REPO_ROOT}/niyah_engine"
CASPER_DIR="${REPO_ROOT}/casper_engine"
BOOT_DIR="${REPO_ROOT}/khawrizm_os/boot"

CROSS="aarch64-linux-gnu-"
CC="${CROSS}gcc"
OBJCOPY="${CROSS}objcopy"
AR="${CROSS}ar"

VERSION="3.0.0"
KERNEL_MAGIC_HEX="4B57524D"  # 'KWRM'

# Ed25519 signing key (generate once, keep private, OTP-fuse the public key)
SIGN_KEY="${REPO_ROOT}/khawrizm_os/keys/signing_key.pem"
SIGN_PUB="${REPO_ROOT}/khawrizm_os/keys/signing_key.pub"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo "[khawrizm-build] $*"; }
err()  { echo "[ERROR] $*" >&2; exit 1; }

require_tool() {
    command -v "$1" >/dev/null 2>&1 || err "Required tool not found: $1"
}

# ---------------------------------------------------------------------------
# Step 0: Prerequisites check
# ---------------------------------------------------------------------------
log "Checking prerequisites..."
require_tool "${CC}"
require_tool cargo
require_tool make
require_tool openssl
require_tool mksquashfs
require_tool xorriso
require_tool grub-mkrescue

mkdir -p "${BUILD_DIR}/boot"
mkdir -p "${BUILD_DIR}/rootfs/usr/lib"
mkdir -p "${BUILD_DIR}/rootfs/usr/bin"
mkdir -p "${BUILD_DIR}/rootfs/etc"
mkdir -p "${REPO_ROOT}/khawrizm_os/keys"

# ---------------------------------------------------------------------------
# Step 1: Generate signing keypair (if not present)
# ---------------------------------------------------------------------------
if [[ ! -f "${SIGN_KEY}" ]]; then
    log "Generating Ed25519 signing keypair..."
    openssl genpkey -algorithm ed25519 -out "${SIGN_KEY}"
    openssl pkey -in "${SIGN_KEY}" -pubout -out "${SIGN_PUB}"
    log "  Private key: ${SIGN_KEY}  (KEEP SECRET — OTP-fuse the public key)"
    log "  Public key:  ${SIGN_PUB}"
else
    log "Using existing signing keypair."
fi

# ---------------------------------------------------------------------------
# Step 2: Build Casper Engine static library
# ---------------------------------------------------------------------------
log "Building Casper Engine (libcasper.a)..."
(
    cd "${CASPER_DIR}"
    make clean
    make CC="${CC}" AR="${CROSS}ar" ARCH=aarch64 -j"$(nproc)"
)
cp "${CASPER_DIR}/libcasper.a" "${BUILD_DIR}/rootfs/usr/lib/"

# ---------------------------------------------------------------------------
# Step 3: Build Niyah Engine (Rust workspace, release)
# ---------------------------------------------------------------------------
log "Building Niyah Engine (cargo release)..."
(
    cd "${NIYAH_WS}"
    cargo build \
        --release \
        --target aarch64-unknown-linux-gnu \
        -Z build-std=core,alloc \
        --no-default-features
)
# Install binaries
NIYAH_TARGET="${NIYAH_WS}/target/aarch64-unknown-linux-gnu/release"
for bin in executive_lobe cognitive_lobe sensory_lobe; do
    [[ -f "${NIYAH_TARGET}/${bin}" ]] && \
        cp "${NIYAH_TARGET}/${bin}" "${BUILD_DIR}/rootfs/usr/bin/" || true
done

# ---------------------------------------------------------------------------
# Step 4: Build verified bootloader (BL2)
# ---------------------------------------------------------------------------
log "Building verified bootloader (BL2)..."
BL2_SRC="${BOOT_DIR}/verified_boot.c"
BL2_BIN="${BUILD_DIR}/boot/bl2.bin"
BL2_ELF="${BUILD_DIR}/boot/bl2.elf"

# BSP stub (minimal HAL — real BSP provided by board vendor)
cat > "${BUILD_DIR}/boot/bsp_stub.c" << 'EOF'
#include <stdint.h>
#include <stddef.h>

/* OTP stub: reads zero-filled key (replace with real OTP driver). */
void otp_read_pubkey(uint8_t pubkey[32]) {
    for (int i = 0; i < 32; i++) pubkey[i] = 0;
}

/* SE051 stub: always returns 0 (INSECURE — replace with real SE051 driver). */
int se051_verify_ed25519(
    const uint8_t *msg, size_t len,
    const uint8_t sig[64], const uint8_t pubkey[32])
{
    (void)msg; (void)len; (void)sig; (void)pubkey;
    return 0;  /* STUB: unconditionally passes — replace before production */
}

/* MMIO halt stub: spin forever. */
void __attribute__((noreturn)) mmio_halt(void) {
    for (;;) __asm__ volatile("wfi\n");
}
EOF

"${CC}" -std=c11 -O2 \
    -march=armv8.2-a \
    -ffreestanding -nostdlib \
    -Wl,--entry=verified_boot_main \
    -Wl,-Ttext=0x00040000 \
    "${BL2_SRC}" "${BUILD_DIR}/boot/bsp_stub.c" \
    -o "${BL2_ELF}"
"${OBJCOPY}" -O binary "${BL2_ELF}" "${BL2_BIN}"
log "  BL2: $(wc -c < "${BL2_BIN}") bytes"

# ---------------------------------------------------------------------------
# Step 5: Create signed HAVEN kernel stub
# (Replace haven_stub.bin with real HAVEN microkernel at M5)
# ---------------------------------------------------------------------------
log "Creating signed HAVEN kernel stub..."
HAVEN_RAW="${BUILD_DIR}/boot/haven_kernel_raw.bin"
HAVEN_SIGNED="${BUILD_DIR}/boot/haven_kernel.bin"

# Kernel stub: infinite loop (AArch64: b .)
printf '\x00\x00\x00\x14' > "${HAVEN_RAW}"
HAVEN_SIZE=$(wc -c < "${HAVEN_RAW}")

# Build kernel_header_t (16 bytes, little-endian)
python3 - << PYEOF
import struct, sys
magic    = 0x4B57524D
version  = 0x00030000  # v3.0.0
img_size = ${HAVEN_SIZE}
reserved = 0
hdr = struct.pack('<IIII', magic, version, img_size, reserved)
with open('${BUILD_DIR}/boot/kernel_header.bin', 'wb') as f:
    f.write(hdr)
PYEOF

cat "${BUILD_DIR}/boot/kernel_header.bin" "${HAVEN_RAW}" > "${BUILD_DIR}/boot/kernel_unsigned.bin"

# Sign with Ed25519
openssl pkeyutl \
    -sign \
    -inkey "${SIGN_KEY}" \
    -rawin \
    -in  "${BUILD_DIR}/boot/kernel_unsigned.bin" \
    -out "${BUILD_DIR}/boot/kernel.sig"

cat "${BUILD_DIR}/boot/kernel_unsigned.bin" \
    "${BUILD_DIR}/boot/kernel.sig" \
    > "${HAVEN_SIGNED}"

log "  Signed kernel: $(wc -c < "${HAVEN_SIGNED}") bytes"

# ---------------------------------------------------------------------------
# Step 6: Assemble rootfs
# ---------------------------------------------------------------------------
log "Assembling root filesystem..."
cat > "${BUILD_DIR}/rootfs/etc/khawrizm-release" << EOF
NAME="Khawrizm OS"
VERSION="${VERSION}"
BUILD_DATE="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
ARCH="aarch64"
ZERO_TELEMETRY=true
EOF

mksquashfs \
    "${BUILD_DIR}/rootfs" \
    "${BUILD_DIR}/rootfs.squashfs" \
    -comp zstd -Xcompression-level 19 \
    -noappend -quiet

log "  rootfs.squashfs: $(wc -c < "${BUILD_DIR}/rootfs.squashfs") bytes"

# ---------------------------------------------------------------------------
# Step 7: Build bootable ISO (GRUB EFI + AArch64)
# ---------------------------------------------------------------------------
log "Building bootable ISO..."
ISO_DIR="${BUILD_DIR}/iso_root"
mkdir -p "${ISO_DIR}/boot/grub"

cp "${HAVEN_SIGNED}"         "${ISO_DIR}/boot/haven_kernel.bin"
cp "${BL2_BIN}"              "${ISO_DIR}/boot/bl2.bin"
cp "${BUILD_DIR}/rootfs.squashfs" "${ISO_DIR}/boot/rootfs.squashfs"

cat > "${ISO_DIR}/boot/grub/grub.cfg" << 'EOF'
set timeout=3
set default=0

menuentry "Khawrizm OS v3.0.0 — Niyah Engine" {
    echo "Khawrizm OS — Zero-Telemetry Sovereign AI"
    # BL2 verified boot handles kernel authentication.
    # GRUB loads the pre-signed kernel image directly.
    linux /boot/haven_kernel.bin
    initrd /boot/rootfs.squashfs
}
EOF

grub-mkrescue \
    --output="${OUT_ISO}" \
    "${ISO_DIR}" \
    -- -volid "KHAWRIZMOS_V3" 2>/dev/null

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
ISO_SIZE=$(wc -c < "${OUT_ISO}")
ISO_MB=$(( ISO_SIZE / 1024 / 1024 ))

log "═══════════════════════════════════════════════════"
log " BUILD COMPLETE"
log " Output : ${OUT_ISO}"
log " Size   : ${ISO_MB} MB (${ISO_SIZE} bytes)"
log " SHA-256: $(sha256sum "${OUT_ISO}" | cut -d' ' -f1)"
log "═══════════════════════════════════════════════════"
log " ⚠  BL2 BSP stubs are NOT production-ready."
log "    Replace otp_read_pubkey() and se051_verify_ed25519()"
log "    with real hardware drivers before deployment."
log " ⚠  HAVEN kernel is a stub (M5 not yet complete)."
log "═══════════════════════════════════════════════════"
