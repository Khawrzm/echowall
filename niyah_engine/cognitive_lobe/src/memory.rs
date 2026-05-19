//! # Pattern Memory — Temporal world-state ring buffer
//!
//! Maintains a bounded ring-buffer of recent `WorldStateTensor` frames
//! for temporal anomaly detection by the Cognitive Lobe.
//!
//! No heap. Fixed capacity via const generic.

use heapless::Deque;
use sensory_lobe::fusion::{WorldStateTensor, WORLD_STATE_DIM};

/// Capacity of the pattern memory ring buffer (number of frames).
pub const MEMORY_DEPTH: usize = 16;

/// Errors from pattern memory operations.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MemoryError {
    /// Ring buffer is full. Oldest frame will be evicted.
    Full,
}

/// Fixed-capacity ring buffer of `WorldStateTensor` frames.
///
/// When full, the oldest frame is silently evicted to make room
/// for the newest. This is intentional: recent context is prioritized.
pub struct PatternMemory {
    frames: Deque<WorldStateTensor, MEMORY_DEPTH>,
}

impl PatternMemory {
    /// Construct an empty pattern memory.
    pub const fn new() -> Self {
        Self {
            frames: Deque::new(),
        }
    }

    /// Push a new frame. If the buffer is full, the oldest is evicted.
    pub fn push(&mut self, tensor: WorldStateTensor) {
        if self.frames.is_full() {
            // Evict oldest.
            self.frames.pop_front();
        }
        // push_back cannot fail after eviction.
        let _ = self.frames.push_back(tensor);
    }

    /// Return the number of frames currently stored.
    pub fn len(&self) -> usize {
        self.frames.len()
    }

    /// Return true if the memory is empty.
    pub fn is_empty(&self) -> bool {
        self.frames.is_empty()
    }

    /// Compute the temporal activation delta between the most recent
    /// frame and the frame `lag` steps back.
    ///
    /// Returns the L1 norm of the difference as a u32, or `None` if
    /// fewer than `lag + 1` frames are available.
    pub fn activation_delta(&self, lag: usize) -> Option<u32> {
        let n = self.frames.len();
        if n < lag + 1 {
            return None;
        }
        let recent = &self.frames[n - 1];
        let prior  = &self.frames[n - 1 - lag];

        let delta: u32 = recent
            .features
            .iter()
            .zip(prior.features.iter())
            .map(|(&a, &b)| (a as i32 - b as i32).unsigned_abs())
            .sum();

        Some(delta)
    }
}

impl Default for PatternMemory {
    fn default() -> Self {
        Self::new()
    }
}
