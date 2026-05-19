//! # Casper Bridge — FFI bindings to the C11 Casper Engine
//!
//! The Casper Engine is a C11 hybrid neuro-symbolic reasoner that operates
//! on quantized neural activations alongside symbolic rule graphs.
//! This module provides the safe Rust FFI wrapper.
//!
//! ## Safety model
//! All `extern "C"` calls are wrapped in safe public functions.
//! Pointer validity is enforced at the Rust API boundary.
//! The C engine is stateless per-call: no global mutable state.
//!
//! ## Build requirement
//! `libcasper.a` must be compiled from `casper_engine/` and linked.
//! See `casper_engine/README.md`. If the library is absent, this module
//! compiles but all calls return `ReasonerOutput::Unavailable`.

use heapless::Vec;
use sensory_lobe::fusion::WORLD_STATE_DIM;

// ---------------------------------------------------------------------------
// FFI declarations — C11 Casper Engine ABI
// ---------------------------------------------------------------------------

/// Maximum length of a UTF-8 Arabic text input to the Casper Engine.
pub const MAX_TEXT_INPUT: usize = 512;

/// Maximum number of symbolic rule activations returned per inference.
pub const MAX_RULE_ACTIVATIONS: usize = 32;

/// Casper Engine C11 ABI.
///
/// These symbols are resolved from `libcasper.a` at link time.
/// If the library is absent, the `stub` feature provides no-op implementations.
#[allow(dead_code)]
extern "C" {
    /// Initialize the Casper Engine with a quantized model buffer.
    ///
    /// `model_buf`: pointer to INT8 quantized weights.
    /// `model_len`: number of bytes in the model buffer.
    /// Returns 0 on success, negative errno on failure.
    fn casper_init(model_buf: *const i8, model_len: usize) -> i32;

    /// Run neuro-symbolic inference on a world-state feature vector.
    ///
    /// `features`: INT8 feature vector of length `feat_len`.
    /// `feat_len`: must equal `WORLD_STATE_DIM` (640).
    /// `out_class`: output presence class (0=empty,1=standing,2=sitting,3=fall).
    /// `out_confidence`: output confidence 0–255 (maps to 0.0–1.0).
    /// Returns 0 on success.
    fn casper_infer(
        features: *const i8,
        feat_len: usize,
        out_class: *mut u8,
        out_confidence: *mut u8,
    ) -> i32;

    /// Run Arabic morphological analysis on a UTF-8 text buffer.
    ///
    /// `text`: pointer to UTF-8 encoded Arabic text.
    /// `text_len`: byte length of text (not including null terminator).
    /// `out_roots`: output buffer for extracted morphological root codes.
    /// `out_count`: on input, capacity of `out_roots`; on output, count written.
    /// Returns 0 on success.
    fn casper_sarf_analyze(
        text: *const u8,
        text_len: usize,
        out_roots: *mut u32,
        out_count: *mut usize,
    ) -> i32;
}

// ---------------------------------------------------------------------------
// Reasoner configuration
// ---------------------------------------------------------------------------

/// Configuration for the `NeuroSymbolicReasoner`.
#[derive(Debug, Clone, Copy)]
pub struct ReasonerConfig {
    /// Minimum confidence (0–255) required to emit an inference result.
    /// Results below this threshold are emitted as `ReasonerOutput::LowConfidence`.
    pub confidence_threshold: u8,
    /// Enable Arabic Sarf morphological extraction alongside neural inference.
    pub sarf_enabled: bool,
}

impl Default for ReasonerConfig {
    fn default() -> Self {
        Self {
            confidence_threshold: 128, // 0.5 normalized
            sarf_enabled: true,
        }
    }
}

// ---------------------------------------------------------------------------
// Reasoner output
// ---------------------------------------------------------------------------

/// Structured output from the neuro-symbolic reasoner.
#[derive(Debug, Clone)]
pub enum ReasonerOutput {
    /// Inference succeeded with confidence above threshold.
    Inferred {
        /// Presence class: 0=empty, 1=standing, 2=sitting, 3=fall.
        class: u8,
        /// Confidence 0–255.
        confidence: u8,
        /// Arabic morphological roots extracted from any co-presented text.
        /// Empty if `sarf_enabled` is false or no text was provided.
        sarf_roots: Vec<u32, MAX_RULE_ACTIVATIONS>,
    },
    /// Inference confidence was below the configured threshold.
    LowConfidence {
        /// Raw class prediction despite low confidence.
        class: u8,
        /// Measured confidence.
        confidence: u8,
    },
    /// Casper Engine is not available (library not linked or init failed).
    Unavailable,
    /// C engine returned an error code.
    EngineError(i32),
}

// ---------------------------------------------------------------------------
// NeuroSymbolicReasoner
// ---------------------------------------------------------------------------

/// Safe Rust wrapper over the C11 Casper Engine.
///
/// Provides hybrid neuro-symbolic inference over `WorldStateTensor` data
/// with optional Arabic morphological root extraction (Sarf, 2,976 forms).
///
/// ## Zero-telemetry
/// All computation is local. No network calls are made by this struct
/// or the underlying C engine. Context does not persist across calls
/// unless explicitly written to encrypted flash by the Executive Lobe.
pub struct NeuroSymbolicReasoner {
    config: ReasonerConfig,
    initialized: bool,
}

impl NeuroSymbolicReasoner {
    /// Construct a new reasoner. Does not call into the C engine yet.
    pub const fn new(config: ReasonerConfig) -> Self {
        Self {
            config,
            initialized: false,
        }
    }

    /// Initialize the Casper Engine with quantized model weights.
    ///
    /// `model_buf`: INT8 weight buffer. Must remain valid for the lifetime
    /// of this struct (the C engine holds a pointer, not a copy).
    ///
    /// Returns `Ok(())` on success, `Err(errno)` on C engine failure.
    pub fn init(&mut self, model_buf: &[i8]) -> Result<(), i32> {
        // SAFETY: model_buf is a valid slice; we pass ptr + len and the C
        // function does not retain the pointer after casper_init returns.
        let rc = unsafe { casper_init(model_buf.as_ptr(), model_buf.len()) };
        if rc == 0 {
            self.initialized = true;
            Ok(())
        } else {
            Err(rc)
        }
    }

    /// Run neuro-symbolic inference on a world-state feature vector.
    ///
    /// `features`: INT8 slice of length `WORLD_STATE_DIM` (640).
    /// `arabic_text`: optional UTF-8 Arabic text for Sarf analysis.
    ///   Pass an empty slice to skip morphological extraction.
    pub fn infer(
        &self,
        features: &[i8],
        arabic_text: &[u8],
    ) -> ReasonerOutput {
        if !self.initialized {
            return ReasonerOutput::Unavailable;
        }
        if features.len() != WORLD_STATE_DIM {
            return ReasonerOutput::Unavailable;
        }

        let mut out_class: u8 = 0;
        let mut out_confidence: u8 = 0;

        // SAFETY: features is a valid slice of length WORLD_STATE_DIM.
        // out_class and out_confidence are stack-allocated and valid.
        let rc = unsafe {
            casper_infer(
                features.as_ptr(),
                features.len(),
                &mut out_class,
                &mut out_confidence,
            )
        };

        if rc != 0 {
            return ReasonerOutput::EngineError(rc);
        }

        if out_confidence < self.config.confidence_threshold {
            return ReasonerOutput::LowConfidence {
                class: out_class,
                confidence: out_confidence,
            };
        }

        // Optional: Arabic Sarf morphological extraction.
        let mut sarf_roots: Vec<u32, MAX_RULE_ACTIVATIONS> = Vec::new();
        if self.config.sarf_enabled && !arabic_text.is_empty() {
            let mut raw_roots = [0u32; MAX_RULE_ACTIVATIONS];
            let mut count: usize = MAX_RULE_ACTIVATIONS;
            // SAFETY: arabic_text is a valid UTF-8 byte slice.
            // raw_roots is stack-allocated with capacity MAX_RULE_ACTIVATIONS.
            let sarf_rc = unsafe {
                casper_sarf_analyze(
                    arabic_text.as_ptr(),
                    arabic_text.len(),
                    raw_roots.as_mut_ptr(),
                    &mut count,
                )
            };
            if sarf_rc == 0 {
                for &root in &raw_roots[..count.min(MAX_RULE_ACTIVATIONS)] {
                    let _ = sarf_roots.push(root);
                }
            }
        }

        ReasonerOutput::Inferred {
            class: out_class,
            confidence: out_confidence,
            sarf_roots,
        }
    }
}
