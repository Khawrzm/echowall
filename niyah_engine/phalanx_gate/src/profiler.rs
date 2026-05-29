//! Runtime profiling and performance monitoring for Phalanx Gate
//!
//! Tracks security decision latency, throughput, and attack patterns
//! for zero-overhead monitoring in production environments.

#![no_std]

use core::sync::atomic::{AtomicU64, Ordering};

/// Performance metrics for security decisions
#[derive(Debug, Clone, Copy)]
pub struct SecurityMetrics {
    /// Total messages inspected
    pub messages_inspected: u64,
    /// Messages emitted (passed)
    pub messages_emitted: u64,
    /// Messages refused (blocked)
    pub messages_refused: u64,
    /// Messages quarantined (suspicious)
    pub messages_quarantined: u64,
    /// Total processing time (nanoseconds)
    pub total_latency_ns: u64,
    /// Peak latency (nanoseconds)
    pub peak_latency_ns: u64,
}

impl SecurityMetrics {
    /// Calculate throughput in messages per second
    #[inline]
    pub fn throughput_mps(&self) -> f64 {
        if self.total_latency_ns == 0 {
            return 0.0;
        }
        (self.messages_inspected as f64 * 1_000_000_000.0) / self.total_latency_ns as f64
    }

    /// Calculate average latency in microseconds
    #[inline]
    pub fn avg_latency_us(&self) -> f64 {
        if self.messages_inspected == 0 {
            return 0.0;
        }
        (self.total_latency_ns as f64 / 1000.0) / self.messages_inspected as f64
    }

    /// Calculate block rate (percentage of refused messages)
    #[inline]
    pub fn block_rate(&self) -> f64 {
        if self.messages_inspected == 0 {
            return 0.0;
        }
        (self.messages_refused as f64 / self.messages_inspected as f64) * 100.0
    }

    /// Calculate quarantine rate
    #[inline]
    pub fn quarantine_rate(&self) -> f64 {
        if self.messages_inspected == 0 {
            return 0.0;
        }
        (self.messages_quarantined as f64 / self.messages_inspected as f64) * 100.0
    }
}

/// Lock-free profiler for zero-contention monitoring
pub struct PhalanxProfiler {
    inspected: AtomicU64,
    emitted: AtomicU64,
    refused: AtomicU64,
    quarantined: AtomicU64,
    total_latency: AtomicU64,
    peak_latency: AtomicU64,
}

impl PhalanxProfiler {
    /// Create a new profiler
    pub const fn new() -> Self {
        Self {
            inspected: AtomicU64::new(0),
            emitted: AtomicU64::new(0),
            refused: AtomicU64::new(0),
            quarantined: AtomicU64::new(0),
            total_latency: AtomicU64::new(0),
            peak_latency: AtomicU64::new(0),
        }
    }

    /// Record a security decision
    #[inline]
    pub fn record_decision(&self, decision: DecisionType, latency_ns: u64) {
        self.inspected.fetch_add(1, Ordering::Relaxed);
        self.total_latency.fetch_add(latency_ns, Ordering::Relaxed);

        // Update peak latency if necessary
        let mut current_peak = self.peak_latency.load(Ordering::Relaxed);
        while latency_ns > current_peak {
            match self.peak_latency.compare_exchange_weak(
                current_peak,
                latency_ns,
                Ordering::Relaxed,
                Ordering::Relaxed,
            ) {
                Ok(_) => break,
                Err(actual) => current_peak = actual,
            }
        }

        // Record decision type
        match decision {
            DecisionType::Emit => self.emitted.fetch_add(1, Ordering::Relaxed),
            DecisionType::Refuse => self.refused.fetch_add(1, Ordering::Relaxed),
            DecisionType::Quarantine => self.quarantined.fetch_add(1, Ordering::Relaxed),
        };
    }

    /// Get current metrics snapshot
    #[inline]
    pub fn snapshot(&self) -> SecurityMetrics {
        SecurityMetrics {
            messages_inspected: self.inspected.load(Ordering::Relaxed),
            messages_emitted: self.emitted.load(Ordering::Relaxed),
            messages_refused: self.refused.load(Ordering::Relaxed),
            messages_quarantined: self.quarantined.load(Ordering::Relaxed),
            total_latency_ns: self.total_latency.load(Ordering::Relaxed),
            peak_latency_ns: self.peak_latency.load(Ordering::Relaxed),
        }
    }

    /// Reset all metrics
    #[inline]
    pub fn reset(&self) {
        self.inspected.store(0, Ordering::Relaxed);
        self.emitted.store(0, Ordering::Relaxed);
        self.refused.store(0, Ordering::Relaxed);
        self.quarantined.store(0, Ordering::Relaxed);
        self.total_latency.store(0, Ordering::Relaxed);
        self.peak_latency.store(0, Ordering::Relaxed);
    }
}

impl Default for PhalanxProfiler {
    fn default() -> Self {
        Self::new()
    }
}

/// Type of security decision made
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecisionType {
    /// Message passed all security checks
    Emit,
    /// Message blocked (security violation)
    Refuse,
    /// Message quarantined (suspicious but not conclusive)
    Quarantine,
}

/// Attack pattern detector
#[derive(Debug, Clone, Copy)]
pub struct AttackPattern {
    /// Number of refused messages in time window
    pub refused_count: u64,
    /// Number of quarantined messages in time window
    pub quarantined_count: u64,
    /// Time window in milliseconds
    pub window_ms: u64,
}

impl AttackPattern {
    /// Detect if an attack is likely in progress
    #[inline]
    pub fn is_under_attack(&self) -> bool {
        // Simple heuristic: >10% block rate in last window suggests attack
        let total = self.refused_count + self.quarantined_count;
        total > 100 && (self.refused_count as f64 / total as f64) > 0.1
    }

    /// Calculate attack severity (0.0 = none, 1.0 = severe)
    #[inline]
    pub fn severity(&self) -> f64 {
        let total = self.refused_count + self.quarantined_count;
        if total == 0 {
            return 0.0;
        }
        let block_rate = self.refused_count as f64 / total as f64;
        block_rate.min(1.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_profiler_basic() {
        let profiler = PhalanxProfiler::new();
        
        profiler.record_decision(DecisionType::Emit, 1000);
        profiler.record_decision(DecisionType::Refuse, 2000);
        profiler.record_decision(DecisionType::Quarantine, 1500);

        let metrics = profiler.snapshot();
        assert_eq!(metrics.messages_inspected, 3);
        assert_eq!(metrics.messages_emitted, 1);
        assert_eq!(metrics.messages_refused, 1);
        assert_eq!(metrics.messages_quarantined, 1);
        assert_eq!(metrics.total_latency_ns, 4500);
        assert_eq!(metrics.peak_latency_ns, 2000);
    }

    #[test]
    fn test_metrics_calculations() {
        let metrics = SecurityMetrics {
            messages_inspected: 1000,
            messages_emitted: 800,
            messages_refused: 150,
            messages_quarantined: 50,
            total_latency_ns: 1_000_000_000, // 1 second
            peak_latency_ns: 10_000_000,
        };

        assert!((metrics.throughput_mps() - 1000.0).abs() < 0.1);
        assert!((metrics.avg_latency_us() - 1.0).abs() < 0.1);
        assert!((metrics.block_rate() - 15.0).abs() < 0.1);
        assert!((metrics.quarantine_rate() - 5.0).abs() < 0.1);
    }

    #[test]
    fn test_attack_detection() {
        let pattern = AttackPattern {
            refused_count: 200,
            quarantined_count: 800,
            window_ms: 1000,
        };

        assert!(pattern.is_under_attack());
        assert!((pattern.severity() - 0.2).abs() < 0.01);
    }

    #[test]
    fn test_profiler_reset() {
        let profiler = PhalanxProfiler::new();
        profiler.record_decision(DecisionType::Emit, 1000);
        profiler.reset();

        let metrics = profiler.snapshot();
        assert_eq!(metrics.messages_inspected, 0);
        assert_eq!(metrics.total_latency_ns, 0);
    }
}
