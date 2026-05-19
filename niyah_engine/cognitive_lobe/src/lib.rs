//! # Cognitive Lobe — Lobe III of the Niyah Engine
//!
//! Neuro-symbolic reasoning engine. Consumes `WorldStateTensor` from
//! Lobe II and produces structured inference results for Lobe I.
//!
//! ## Components
//! - **Casper Bridge:** FFI bindings to the C11 Casper Engine for hybrid
//!   neuro-symbolic reasoning over Arabic linguistic structures.
//! - **Sarf Morphology:** Arabic morphological root extraction covering
//!   2,976 canonical verb forms (binyan × conjugation table).
//! - **Pattern Memory:** Ring-buffer of recent `WorldStateTensor` frames
//!   for temporal anomaly detection.
//!
//! ## Zero-telemetry guarantee
//! All inference is 100% offline. No network calls. No external APIs.
//! Context persists only on local encrypted flash.

#![no_std]
#![deny(missing_docs)]
#![warn(clippy::all)]

pub mod casper_bridge;
pub mod sarf;
pub mod memory;

pub use casper_bridge::{NeuroSymbolicReasoner, ReasonerConfig, ReasonerOutput};
pub use sarf::{SarfAnalyzer, MorphRoot, BinyanClass};
pub use memory::{PatternMemory, MemoryError};
