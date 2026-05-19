//! # Sensory Lobe — Lobe II of the Niyah Engine
//!
//! Hardware-facing perception layer. Owns CSI passive radar, acoustic FMCW,
//! TVWS spectrum scanning, and environment state fusion.
//!
//! ## Zero-Copy DMA guarantee
//! Raw samples from the SDR peripheral are written directly to NPU-accessible
//! SRAM via the `dma` module. The CPU is not on the data path for sample
//! ingestion. This is mandatory to satisfy the 2.5 W thermal budget.
//!
//! ## Privacy invariant
//! Raw CSI, acoustic, and RF samples are **destroyed after fusion**.
//! Only the fused `WorldStateTensor` is published to Lobe III via Phalanx Gate.
//! No raw sample ever crosses a lobe boundary.

#![no_std]
#![deny(missing_docs)]
#![warn(clippy::all)]
// unsafe is allowed only in dma.rs for physical address manipulation.
// All other modules are safe Rust.

pub mod dma;
pub mod sidsense;
pub mod fusion;

pub use dma::{DmaDescriptor, DmaError, DmaFlags};
pub use sidsense::{SidSense, SidSenseConfig, TvwsChannel};
pub use fusion::{WorldStateTensor, FusionError};
