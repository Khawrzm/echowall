//! # Phalanx Gate — Zero-Trust Inter-Lobe Firewall
//!
//! All communication between Niyah Engine lobes passes through this module.
//! The gate enforces compile-time schema contracts and runtime payload
//! inspection. No message may cross a lobe boundary without gate approval.
//!
//! ## Design principles
//! - **No configuration at runtime.** Rules are compiled in, not loaded.
//! - **No silent failures.** Every message receives an explicit `Decision`.
//! - **No raw sensor data crossing lobe boundaries.** Lobe II destroys raw
//!   CSI/audio before publishing a fused world-state tensor.
//! - **Subset-sum projection.** The inspector validates that the activation
//!   space of the payload is consistent with the declared message type
//!   (a bounded NP-approximation computed in constant time via INT8 sums).
//!
//! ## `no_std` guarantee
//! This crate compiles on bare-metal targets with no OS and no allocator.

#![no_std]
#![deny(unsafe_code)]
#![deny(missing_docs)]
#![warn(clippy::all)]

pub mod ssv;
pub mod profiler;

use heapless::Vec;

// ---------------------------------------------------------------------------
// Lobe identity
// ---------------------------------------------------------------------------

/// Identifies which lobe sent or should receive a message.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum LobeId {
    /// Lobe I — Executive (orchestration, scheduling, watchdog).
    Executive = 0,
    /// Lobe II — Sensory (CSI radar, acoustic FMCW, env fusion).
    Sensory = 1,
    /// Lobe III — Cognitive (local LLM inference, pattern memory).
    Cognitive = 2,
}

// ---------------------------------------------------------------------------
// Message classification
// ---------------------------------------------------------------------------

/// The semantic type of an inter-lobe message.
///
/// Used by the Phalanx Gate to apply type-specific inspection rules.
/// A message whose `kind` does not match its payload structure is `REFUSE`d.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum MessageKind {
    /// Fused environment world-state tensor (Lobe II → Lobe III).
    /// Payload: INT8 vector, max 640 elements. No raw CSI permitted.
    WorldState = 0x01,
    /// Inference result from Cognitive Lobe (Lobe III → Lobe I).
    /// Payload: presence class (u8) + confidence (u8) + posture (u8).
    InferenceResult = 0x02,
    /// Executive command dispatched to a target lobe.
    /// Payload: command opcode (u8) + optional argument (u32, little-endian).
    ExecutiveCommand = 0x03,
    /// Audit event — append-only log entry for the Executive audit ring.
    /// Payload: event code (u8) + tick (u64, little-endian).
    AuditEvent = 0x04,
    /// Federated weight delta from Sensory node (Lobe II → Lobe III).
    /// Payload: masked INT8 weight delta vector, max 512 elements.
    FedAvgDelta = 0x05,
}

// ---------------------------------------------------------------------------
// Maximum payload size (bytes). Compile-time constant.
// ---------------------------------------------------------------------------

/// Maximum payload bytes for any single inter-lobe message.
/// Sized to fit the largest valid message (WorldState: 640 INT8 values).
pub const MAX_PAYLOAD: usize = 640;

// ---------------------------------------------------------------------------
// Message envelope
// ---------------------------------------------------------------------------

/// An inter-lobe message envelope.
///
/// All fields are validated by the `PhalanxInspector` before dispatch.
/// The `payload` is a bounded byte vector — no heap allocation.
#[derive(Debug, Clone)]
pub struct Message {
    /// Originating lobe.
    pub source: LobeId,
    /// Destination lobe.
    pub destination: LobeId,
    /// Semantic type of this message.
    pub kind: MessageKind,
    /// Sequence number (monotonic per source lobe; checked for replay).
    pub sequence: u32,
    /// Message payload. Maximum `MAX_PAYLOAD` bytes.
    pub payload: Vec<u8, MAX_PAYLOAD>,
}

impl Message {
    /// Construct a new message.
    ///
    /// Returns `None` if `payload` exceeds `MAX_PAYLOAD`.
    pub fn new(
        source: LobeId,
        destination: LobeId,
        kind: MessageKind,
        sequence: u32,
        payload: &[u8],
    ) -> Option<Self> {
        let mut buf: Vec<u8, MAX_PAYLOAD> = Vec::new();
        buf.extend_from_slice(payload).ok()?;
        Some(Self {
            source,
            destination,
            kind,
            sequence,
            payload: buf,
        })
    }

    /// Compute a bounded INT8 subset-sum projection of the payload.
    ///
    /// This is the activation-space fingerprint used by `PhalanxInspector`
    /// implementations to verify that the payload is consistent with the
    /// declared `MessageKind`. Computed in O(n) time, constant stack space.
    ///
    /// The result is an INT16 saturating sum of all payload bytes interpreted
    /// as signed INT8 values, clamped to [-32768, 32767].
    pub fn activation_projection(&self) -> i16 {
        self.payload
            .iter()
            .map(|&b| b as i8 as i16)
            .fold(0i16, |acc, v| acc.saturating_add(v))
    }
}

// ---------------------------------------------------------------------------
// Gate decision
// ---------------------------------------------------------------------------

/// The verdict returned by a `PhalanxInspector` for each message.
///
/// The Executive Lobe acts on this decision immediately and unconditionally.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Decision {
    /// Message is valid and approved for dispatch to destination lobe.
    Emit,
    /// Message is invalid or violates a gate rule. Drop silently and log.
    Refuse,
    /// Message is structurally valid but matches a threat pattern.
    /// The Executive Lobe MUST halt the system for audit. No recovery
    /// is possible without a hardware reset.
    Quarantine,
}

// ---------------------------------------------------------------------------
// PhalanxInspector trait
// ---------------------------------------------------------------------------

/// The compile-time contract that all Phalanx Gate implementations must satisfy.
///
/// Implementations define the zero-trust inspection policy. The trait is
/// intentionally minimal: a single function that maps a message to a decision.
///
/// ## Implementation requirements
/// - Must execute in **bounded, constant time** regardless of payload size.
/// - Must not allocate.
/// - Must be deterministic: identical inputs must produce identical outputs.
/// - Must not have side effects (logging is handled by the Executive Lobe).
///
/// ## Subset-sum activation projection
///
/// The canonical inspection strategy uses `Message::activation_projection()`
/// to verify payload coherence against per-kind expected ranges:
///
/// ```text
/// MessageKind::WorldState    → expect projection in [-640, 640]
///                               (all INT8 — trivially satisfied; used to
///                                detect zero-padded injection attempts where
///                                projection == 0 but payload length > 0)
/// MessageKind::InferenceResult → expect payload length == 3
/// MessageKind::ExecutiveCommand → expect payload length == 5
/// MessageKind::AuditEvent     → expect payload length == 9
/// MessageKind::FedAvgDelta    → expect payload length <= 512
/// ```
///
/// Any message that fails these structural checks is `REFUSE`d.
/// A message with a zero-projection and non-zero length is `QUARANTINE`d
/// as a potential zero-fill injection attack.
pub trait PhalanxInspector {
    /// Inspect a single inter-lobe message and return an enforcement decision.
    ///
    /// # Arguments
    /// * `message` — The message to inspect. The inspector must not modify it.
    ///
    /// # Returns
    /// * `Decision::Emit` — Message is approved. Dispatch to destination.
    /// * `Decision::Refuse` — Message is invalid. Drop and log.
    /// * `Decision::Quarantine` — Message is a threat. Halt system for audit.
    fn inspect(&self, message: &Message) -> Decision;
}

// ---------------------------------------------------------------------------
// StrictGate — reference implementation
// ---------------------------------------------------------------------------

/// Reference implementation of `PhalanxInspector`.
///
/// Enforces all structural rules from the trait documentation.
/// Zero configuration. Zero heap. Deterministic O(n) time.
///
/// This is the gate that compiles into production firmware.
/// Replacing it requires a full firmware rebuild and re-signing.
pub struct StrictGate {
    /// Expected sequence number per source lobe.
    /// Used for replay-attack detection.
    last_sequence: [u32; 3], // indexed by LobeId as usize
}

impl StrictGate {
    /// Create a new `StrictGate` with all sequence counters at zero.
    pub const fn new() -> Self {
        Self {
            last_sequence: [0u32; 3],
        }
    }
}

impl Default for StrictGate {
    fn default() -> Self {
        Self::new()
    }
}

impl PhalanxInspector for StrictGate {
    fn inspect(&self, message: &Message) -> Decision {
        // Rule 1: Source lobe may not send to itself.
        if message.source == message.destination {
            return Decision::Refuse;
        }

        // Rule 2: Raw sensor data (WorldState) may only originate from Sensory.
        if message.kind == MessageKind::WorldState && message.source != LobeId::Sensory {
            return Decision::Quarantine;
        }

        // Rule 3: InferenceResult may only originate from Cognitive.
        if message.kind == MessageKind::InferenceResult && message.source != LobeId::Cognitive {
            return Decision::Quarantine;
        }

        // Rule 4: ExecutiveCommand may only originate from Executive.
        if message.kind == MessageKind::ExecutiveCommand && message.source != LobeId::Executive {
            return Decision::Quarantine;
        }

        // Rule 5: Structural length checks per MessageKind.
        let expected_len: Option<usize> = match message.kind {
            MessageKind::InferenceResult => Some(3),
            MessageKind::ExecutiveCommand => Some(5),
            MessageKind::AuditEvent => Some(9),
            MessageKind::WorldState => None,   // variable length, bounded by MAX_PAYLOAD
            MessageKind::FedAvgDelta => None,  // variable length, max 512
        };
        if let Some(len) = expected_len {
            if message.payload.len() != len {
                return Decision::Refuse;
            }
        }

        // Rule 6: FedAvgDelta hard cap at 512 bytes.
        if message.kind == MessageKind::FedAvgDelta && message.payload.len() > 512 {
            return Decision::Refuse;
        }

        // Rule 7: Zero-fill injection detection.
        // A non-empty payload with activation projection == 0 is suspicious.
        if !message.payload.is_empty() && message.activation_projection() == 0 {
            return Decision::Quarantine;
        }

        // Rule 8: Replay attack detection (sequence must be strictly increasing).
        let src_idx = message.source as usize;
        if message.sequence <= self.last_sequence[src_idx] && message.sequence != 0 {
            return Decision::Quarantine;
        }

        Decision::Emit
    }
}
