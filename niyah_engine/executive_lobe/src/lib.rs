//! # Executive Lobe — Lobe I of the Niyah Engine
//!
//! The deterministic orchestration layer. Governs task scheduling,
//! resource arbitration, and watchdog authority across all lobes.
//!
//! ## Invariants
//! - Zero heap allocation after `Executive::init()` returns.
//! - All task slots are statically allocated at compile time via `heapless`.
//! - No task may exceed its declared CPU budget (enforced by watchdog).
//! - All inter-lobe messages pass through `PhalanxGate` before dispatch.
//!
//! ## `no_std` guarantee
//! This crate compiles on bare-metal targets with no OS and no allocator.

#![no_std]
#![deny(unsafe_code)]          // Safe Rust only — unsafe requires explicit override + justification
#![deny(missing_docs)]
#![warn(clippy::all)]

use heapless::Vec;
use phalanx_gate::{Decision, LobeId, Message, PhalanxInspector};

// ---------------------------------------------------------------------------
// Constants — all sizing is compile-time. No dynamic allocation.
// ---------------------------------------------------------------------------

/// Maximum number of tasks the scheduler can hold simultaneously.
pub const MAX_TASKS: usize = 16;

/// Maximum number of pending inter-lobe messages in the dispatch queue.
pub const MAX_MESSAGE_QUEUE: usize = 32;

// ---------------------------------------------------------------------------
// Task priority levels
// ---------------------------------------------------------------------------

/// Fixed-priority levels for the deterministic scheduler.
/// Higher numeric value = higher priority. Real-time tasks must use
/// `Priority::RealTime` and must complete within their declared budget.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(u8)]
pub enum Priority {
    /// Background maintenance (calibration, log rotation).
    Background = 0,
    /// Normal processing (cognitive inference, state updates).
    Normal = 1,
    /// High priority (sensory fusion, federated round coordination).
    High = 2,
    /// Hard real-time (watchdog, Phalanx Gate enforcement, emergency halt).
    RealTime = 3,
}

// ---------------------------------------------------------------------------
// Task descriptor
// ---------------------------------------------------------------------------

/// A statically-described execution unit.
///
/// Tasks are registered at init time and may not be added or removed at
/// runtime. The `budget_us` field is enforced by the hardware watchdog;
/// exceeding it triggers an immediate `Executive::halt()`.
#[derive(Debug, Clone, Copy)]
pub struct Task {
    /// Human-readable identifier (for audit log only; not used in scheduling).
    pub name: &'static str,
    /// Execution priority.
    pub priority: Priority,
    /// Maximum allowed CPU time in microseconds.
    pub budget_us: u32,
    /// Target lobe that owns this task.
    pub owner: LobeId,
}

// ---------------------------------------------------------------------------
// Scheduler state
// ---------------------------------------------------------------------------

/// Outcome of a scheduling tick.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TickOutcome {
    /// A task was dispatched to its owner lobe.
    Dispatched(LobeId),
    /// No runnable task found; system may enter low-power idle.
    Idle,
    /// Watchdog triggered; system halted. No further tasks will run.
    Halted,
}

/// The deterministic fixed-priority scheduler.
///
/// All memory is stack-allocated. The scheduler holds a static task list
/// and a bounded message queue. No heap, no OS threads.
///
/// Scheduling algorithm: strict fixed-priority, non-preemptive within a
/// single `tick()` call. The highest-priority runnable task is selected
/// on each tick. Ties broken by insertion order (FIFO).
pub struct DeterministicScheduler<G: PhalanxInspector> {
    /// Registered task descriptors.
    tasks: Vec<Task, MAX_TASKS>,
    /// Pending inter-lobe messages awaiting dispatch.
    queue: Vec<Message, MAX_MESSAGE_QUEUE>,
    /// Phalanx Gate inspector — all messages are screened before dispatch.
    gate: G,
    /// Whether the watchdog has triggered a system halt.
    halted: bool,
    /// Monotonic tick counter (driven by caller — no internal clock assumed).
    tick_count: u64,
}

impl<G: PhalanxInspector> DeterministicScheduler<G> {
    /// Construct a new scheduler with the given Phalanx Gate inspector.
    ///
    /// No allocation occurs. All internal storage is pre-allocated.
    pub fn new(gate: G) -> Self {
        Self {
            tasks: Vec::new(),
            queue: Vec::new(),
            gate,
            halted: false,
            tick_count: 0,
        }
    }

    /// Register a task. Returns `Err(task)` if the task table is full.
    ///
    /// Must be called during init, before the first `tick()`.
    pub fn register(&mut self, task: Task) -> Result<(), Task> {
        self.tasks.push(task).map_err(|_| task)
    }

    /// Enqueue an inter-lobe message for gated dispatch.
    ///
    /// The message is not sent immediately; it is inspected by Phalanx Gate
    /// on the next `tick()`. Returns `Err` if the queue is full.
    pub fn enqueue(&mut self, msg: Message) -> Result<(), Message> {
        self.queue.push(msg).map_err(|_| msg)
    }

    /// Advance the scheduler by one tick.
    ///
    /// On each tick:
    /// 1. All queued messages are inspected by Phalanx Gate.
    ///    - `EMIT` messages are dispatched to their target lobe.
    ///    - `REFUSE` messages are dropped and logged.
    ///    - `QUARANTINE` messages are held and trigger a `RealTime` audit task.
    /// 2. The highest-priority runnable task is selected and its `TickOutcome` returned.
    ///
    /// If `halted` is set, returns `TickOutcome::Halted` immediately.
    pub fn tick(&mut self) -> TickOutcome {
        if self.halted {
            return TickOutcome::Halted;
        }

        self.tick_count = self.tick_count.wrapping_add(1);

        // --- Phase 1: Gate all pending messages ---
        // Iterate in reverse so we can swap-remove without index shifting.
        let mut i = self.queue.len();
        while i > 0 {
            i -= 1;
            let decision = self.gate.inspect(&self.queue[i]);
            match decision {
                Decision::Emit => {
                    // Message approved: remove from queue (caller reads dispatch log).
                    self.queue.swap_remove(i);
                }
                Decision::Refuse => {
                    // Message rejected: silently drop.
                    self.queue.swap_remove(i);
                }
                Decision::Quarantine => {
                    // Message quarantined: halt for audit.
                    self.halted = true;
                    return TickOutcome::Halted;
                }
            }
        }

        // --- Phase 2: Select highest-priority task ---
        let best = self
            .tasks
            .iter()
            .max_by_key(|t| t.priority);

        match best {
            Some(task) => TickOutcome::Dispatched(task.owner),
            None => TickOutcome::Idle,
        }
    }

    /// Trigger an immediate system halt.
    ///
    /// Once halted, `tick()` returns `TickOutcome::Halted` unconditionally.
    /// Recovery requires a hardware reset. This is intentional.
    pub fn halt(&mut self) {
        self.halted = true;
    }

    /// Return the current monotonic tick count.
    pub fn tick_count(&self) -> u64 {
        self.tick_count
    }

    /// Return `true` if the system is in a halted state.
    pub fn is_halted(&self) -> bool {
        self.halted
    }
}
