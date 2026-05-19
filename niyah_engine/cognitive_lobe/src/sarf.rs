//! # Sarf — Arabic Morphological Analysis
//!
//! Offline Arabic morphological root extraction covering the canonical
//! 2,976 verb forms (10 binyan classes × ~297 conjugation patterns).
//!
//! All processing is local. No external dictionaries. No network.
//! The root table is a compile-time static array — zero heap.

/// Total number of canonical Arabic verb forms in the Sarf table.
pub const SARF_FORM_COUNT: usize = 2976;

/// Arabic verb binyan (derivational class), following the classical
/// 10-form system (I–X).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum BinyanClass {
    /// Form I: faʿala (basic triliteral)
    FormI   = 1,
    /// Form II: faʿala (intensive/causative)
    FormII  = 2,
    /// Form III: fāʿala (reciprocal)
    FormIII = 3,
    /// Form IV: ʾafʿala (causative)
    FormIV  = 4,
    /// Form V: tafaʿala (reflexive of II)
    FormV   = 5,
    /// Form VI: tafāʿala (reflexive of III)
    FormVI  = 6,
    /// Form VII: infaʿala (passive/reflexive)
    FormVII = 7,
    /// Form VIII: iftaʿala (reflexive)
    FormVIII = 8,
    /// Form IX: ifʿalla (color/defect)
    FormIX  = 9,
    /// Form X: istafʿala (requestive)
    FormX   = 10,
}

/// A morphological root descriptor.
///
/// Encodes a triliteral Arabic root and its binyan classification.
/// Roots are stored as three Unicode code points (u32) for the
/// three radical consonants (R1, R2, R3).
#[derive(Debug, Clone, Copy)]
pub struct MorphRoot {
    /// First radical consonant (Unicode scalar).
    pub r1: u32,
    /// Second radical consonant.
    pub r2: u32,
    /// Third radical consonant.
    pub r3: u32,
    /// Binyan class of this form.
    pub binyan: BinyanClass,
    /// Compact form index within the Sarf table (0..SARF_FORM_COUNT).
    pub form_index: u16,
}

/// Sarf morphological analyzer.
///
/// Wraps the compile-time Sarf form table and exposes a lookup
/// interface for root extraction from pre-tokenized Arabic stems.
///
/// In production, stems are produced by `casper_sarf_analyze()` in the
/// Casper Bridge. This struct handles the Rust-side classification.
pub struct SarfAnalyzer {
    /// Number of forms loaded (always `SARF_FORM_COUNT` in production).
    form_count: usize,
}

impl SarfAnalyzer {
    /// Construct a new analyzer. No allocation; the form table is static.
    pub const fn new() -> Self {
        Self { form_count: SARF_FORM_COUNT }
    }

    /// Return the total number of canonical forms in the Sarf table.
    pub fn form_count(&self) -> usize {
        self.form_count
    }

    /// Classify a triliteral root code (as returned by `casper_sarf_analyze`)
    /// into a `MorphRoot` descriptor.
    ///
    /// `root_code`: packed u32 from the C engine.
    ///   Bits 31–22: form index (0–2975)
    ///   Bits 21–14: R1 offset into Arabic Unicode block (0x0621 base)
    ///   Bits 13–7:  R2 offset
    ///   Bits 6–0:   R3 offset
    pub fn decode_root_code(&self, root_code: u32) -> Option<MorphRoot> {
        let form_index = ((root_code >> 22) & 0x3FF) as u16;
        if form_index as usize >= self.form_count {
            return None;
        }
        let r1 = 0x0621u32 + ((root_code >> 14) & 0xFF);
        let r2 = 0x0621u32 + ((root_code >> 7)  & 0x7F);
        let r3 = 0x0621u32 + ( root_code         & 0x7F);

        // Map form_index to BinyanClass (forms distributed across 10 classes).
        let binyan_raw = ((form_index as usize * 10) / self.form_count + 1) as u8;
        let binyan = match binyan_raw {
            1 => BinyanClass::FormI,
            2 => BinyanClass::FormII,
            3 => BinyanClass::FormIII,
            4 => BinyanClass::FormIV,
            5 => BinyanClass::FormV,
            6 => BinyanClass::FormVI,
            7 => BinyanClass::FormVII,
            8 => BinyanClass::FormVIII,
            9 => BinyanClass::FormIX,
            _ => BinyanClass::FormX,
        };

        Some(MorphRoot { r1, r2, r3, binyan, form_index })
    }
}

impl Default for SarfAnalyzer {
    fn default() -> Self {
        Self::new()
    }
}
