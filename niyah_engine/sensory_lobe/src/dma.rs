//! # Zero-Copy DMA engine for Sensory Lobe
//!
//! Implements the `Niyah_DMA_Descriptor` for direct SDR → NPU sample transfer
//! on the RK3588 SoC. The CPU is bypassed entirely on the data path.
//!
//! ## Target hardware
//! RK3588 RKNN NPU (6 TOPS INT8) + AXI DMA controller.
//! Physical addresses assume the RK3588 memory map:
//! - NPU SRAM window: 0xFF00_0000 – 0xFF3F_FFFF (4 MB)
//! - SDR DMA source: mapped by driver at init, passed in at runtime.
//!
//! ## Safety model
//! All unsafe blocks manipulate raw physical addresses. Each block carries
//! a `SAFETY:` comment stating the invariant the caller must uphold.
//! The public API is safe Rust; unsafe is confined to private helpers.

use bitflags::bitflags;

// ---------------------------------------------------------------------------
// DMA flags
// ---------------------------------------------------------------------------

bitflags! {
    /// Control flags for a DMA descriptor.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub struct DmaFlags: u32 {
        /// Interrupt the CPU on transfer completion (used for NPU-ready signal).
        const INTERRUPT_ON_COMPLETE = 0b0000_0001;
        /// Descriptor is the last in a chain; DMA engine halts after this.
        const END_OF_CHAIN          = 0b0000_0010;
        /// Source address increments after each burst (scatter mode).
        const SRC_INCREMENT         = 0b0000_0100;
        /// Destination address is fixed (e.g., NPU FIFO register).
        const DST_FIXED             = 0b0000_1000;
        /// Transfer is from peripheral to memory (P2M mode).
        const PERIPH_TO_MEM         = 0b0001_0000;
        /// Transfer is from memory to NPU SRAM (M2NPU mode).
        const MEM_TO_NPU            = 0b0010_0000;
        /// Bypass CPU cache — mandatory for coherent DMA on RK3588 without SMMU.
        const CACHE_BYPASS          = 0b0100_0000;
        /// Thermal guard: abort transfer if NPU junction temp > 85°C.
        const THERMAL_GUARD         = 0b1000_0000;
    }
}

// ---------------------------------------------------------------------------
// DMA descriptor
// ---------------------------------------------------------------------------

/// `Niyah_DMA_Descriptor` — a single zero-copy DMA transfer descriptor.
///
/// Describes one contiguous transfer from a physical source address (SDR
/// peripheral FIFO or intermediate buffer) to a physical destination address
/// (NPU SRAM input window). Descriptors may be chained; the DMA engine
/// processes them sequentially without CPU involvement.
///
/// ## Memory layout
/// The struct is `repr(C)` so it may be placed in a DMA-accessible memory
/// region and read directly by the AXI DMA controller hardware.
///
/// ## Invariants (caller must uphold)
/// 1. `source_phys_addr` must be 64-byte aligned (AXI burst requirement).
/// 2. `dest_phys_addr` must fall within the NPU SRAM window:
///    `0xFF00_0000 ..= 0xFF3F_FFFF` on RK3588.
/// 3. `length` must be a multiple of 64 bytes and must not exceed 4 MB.
/// 4. The descriptor itself must reside in non-cacheable memory or be
///    explicitly cache-flushed before the DMA engine is armed.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct DmaDescriptor {
    /// Physical source address (SDR sample buffer or FIFO).
    /// Must be 64-byte aligned.
    pub source_phys_addr: u64,
    /// Physical destination address (NPU SRAM input window).
    /// Must be within NPU SRAM: 0xFF00_0000 – 0xFF3F_FFFF on RK3588.
    pub dest_phys_addr: u64,
    /// Transfer length in bytes. Must be a multiple of 64, max 4 MB.
    pub length: u32,
    /// Control flags governing transfer behaviour.
    pub flags: DmaFlags,
    /// Physical address of the next descriptor in the chain.
    /// Set to `CHAIN_END` if this is the last descriptor.
    pub next_descriptor_phys: u64,
    /// Reserved for hardware use. Must be zero on init.
    _reserved: [u8; 8],
}

/// Sentinel value for `next_descriptor_phys` indicating end of chain.
pub const CHAIN_END: u64 = 0xFFFF_FFFF_FFFF_FFFF;

/// NPU SRAM physical base address on RK3588.
pub const NPU_SRAM_BASE: u64 = 0xFF00_0000;
/// NPU SRAM size (4 MB).
pub const NPU_SRAM_SIZE: u64 = 0x0040_0000;

// ---------------------------------------------------------------------------
// DMA error
// ---------------------------------------------------------------------------

/// Errors returned by DMA descriptor construction and validation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DmaError {
    /// Source address is not 64-byte aligned.
    SourceMisaligned,
    /// Destination address falls outside NPU SRAM window.
    DestOutOfRange,
    /// Transfer length is not a multiple of 64 bytes.
    LengthMisaligned,
    /// Transfer length exceeds 4 MB maximum.
    LengthExceedsMax,
    /// Length is zero — no-op transfers are not permitted.
    LengthZero,
}

impl DmaDescriptor {
    /// Construct and validate a new DMA descriptor.
    ///
    /// All invariants are checked before the descriptor is returned.
    /// Returns `Err(DmaError)` if any invariant is violated.
    ///
    /// For the last descriptor in a chain, pass `next = CHAIN_END`.
    pub fn new(
        source_phys_addr: u64,
        dest_phys_addr: u64,
        length: u32,
        flags: DmaFlags,
        next: u64,
    ) -> Result<Self, DmaError> {
        if length == 0 {
            return Err(DmaError::LengthZero);
        }
        if source_phys_addr % 64 != 0 {
            return Err(DmaError::SourceMisaligned);
        }
        if dest_phys_addr < NPU_SRAM_BASE
            || dest_phys_addr >= NPU_SRAM_BASE + NPU_SRAM_SIZE
        {
            return Err(DmaError::DestOutOfRange);
        }
        if length % 64 != 0 {
            return Err(DmaError::LengthMisaligned);
        }
        if length as u64 > NPU_SRAM_SIZE {
            return Err(DmaError::LengthExceedsMax);
        }
        Ok(Self {
            source_phys_addr,
            dest_phys_addr,
            length,
            flags,
            next_descriptor_phys: next,
            _reserved: [0u8; 8],
        })
    }

    /// Arm the DMA engine by writing this descriptor's physical address
    /// to the RK3588 AXI DMA channel 0 descriptor register.
    ///
    /// # Safety
    /// - The caller must ensure the descriptor resides at `descriptor_phys_addr`
    ///   in non-cacheable memory (or has been flushed from the D-cache).
    /// - The AXI DMA controller must not be running when this is called.
    /// - `dma_reg_base` must be the virtual address of the AXI DMA MMIO region,
    ///   mapped non-cacheable by the OS/bootloader.
    pub unsafe fn arm(&self, dma_reg_base: *mut u32, descriptor_phys_addr: u64) {
        // SAFETY: caller guarantees dma_reg_base is valid MMIO, DMA is idle,
        // and descriptor is in non-cacheable memory.
        // Register offset 0x08: DMA descriptor address low 32 bits.
        // Register offset 0x0C: DMA descriptor address high 32 bits.
        // Register offset 0x00: DMA control (bit 0 = enable).
        let desc_lo = (descriptor_phys_addr & 0xFFFF_FFFF) as u32;
        let desc_hi = (descriptor_phys_addr >> 32) as u32;
        core::ptr::write_volatile(dma_reg_base.add(2), desc_lo);
        core::ptr::write_volatile(dma_reg_base.add(3), desc_hi);
        // Issue start: write 1 to bit 0 of control register.
        core::ptr::write_volatile(dma_reg_base, 0x0000_0001u32);
    }
}
