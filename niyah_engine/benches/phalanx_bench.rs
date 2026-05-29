//! Benchmarks for Phalanx Gate zero-trust firewall
//!
//! Validates that security decisions meet latency requirements:
//! - Target: <100 nanoseconds per message inspection
//! - Throughput: >10 million messages/sec on modern CPU

use criterion::{black_box, criterion_group, criterion_main, Criterion, BenchmarkId};
use phalanx_gate::{
    Decision, LobeId, Message, MessageKind, PhalanxInspector, StrictGate,
    profiler::{PhalanxProfiler, DecisionType},
};

fn create_valid_message(kind: MessageKind, payload_size: usize) -> Message {
    let payload = match kind {
        MessageKind::InferenceResult => vec![1u8, 128u8, 2u8],
        MessageKind::ExecutiveCommand => vec![0x01, 0x00, 0x00, 0x00, 0x10],
        MessageKind::AuditEvent => vec![0x42, 0, 0, 0, 0, 0, 0, 0, 1],
        MessageKind::WorldState => vec![42i8 as u8; payload_size.min(640)],
        MessageKind::FedAvgDelta => vec![10i8 as u8; payload_size.min(512)],
    };

    Message::new(
        LobeId::Sensory,
        LobeId::Cognitive,
        kind,
        1,
        &payload,
    )
    .unwrap()
}

fn bench_gate_inspection(c: &mut Criterion) {
    let gate = StrictGate::new();

    let mut group = c.benchmark_group("gate_inspection");

    // Test each message type
    for kind in &[
        MessageKind::InferenceResult,
        MessageKind::ExecutiveCommand,
        MessageKind::AuditEvent,
        MessageKind::WorldState,
        MessageKind::FedAvgDelta,
    ] {
        let msg = create_valid_message(*kind, 64);
        
        group.bench_with_input(
            BenchmarkId::new("inspect", format!("{:?}", kind)),
            &msg,
            |b, msg| {
                b.iter(|| {
                    let decision = gate.inspect(black_box(msg));
                    black_box(decision);
                });
            },
        );
    }

    group.finish();
}

fn bench_payload_sizes(c: &mut Criterion) {
    let gate = StrictGate::new();

    let mut group = c.benchmark_group("payload_size");

    // Test varying payload sizes
    for size in &[8, 32, 128, 256, 512, 640] {
        let msg = create_valid_message(MessageKind::WorldState, *size);
        
        group.bench_with_input(
            BenchmarkId::new("worldstate", size),
            &msg,
            |b, msg| {
                b.iter(|| {
                    let decision = gate.inspect(black_box(msg));
                    black_box(decision);
                });
            },
        );
    }

    group.finish();
}

fn bench_profiler_overhead(c: &mut Criterion) {
    let profiler = PhalanxProfiler::new();

    c.bench_function("profiler_record", |b| {
        b.iter(|| {
            profiler.record_decision(
                black_box(DecisionType::Emit),
                black_box(100),
            );
        });
    });

    c.bench_function("profiler_snapshot", |b| {
        b.iter(|| {
            let metrics = profiler.snapshot();
            black_box(metrics);
        });
    });
}

fn bench_throughput(c: &mut Criterion) {
    let gate = StrictGate::new();
    let messages: Vec<Message> = (0..10000)
        .map(|i| {
            create_valid_message(
                MessageKind::WorldState,
                (i % 640) + 1,
            )
        })
        .collect();

    c.bench_function("throughput_10k", |b| {
        b.iter(|| {
            for msg in &messages {
                let decision = gate.inspect(black_box(msg));
                black_box(decision);
            }
        });
    });
}

fn bench_attack_patterns(c: &mut Criterion) {
    let gate = StrictGate::new();

    let mut group = c.benchmark_group("attack_detection");

    // Zero-fill injection attack
    let zero_fill = Message::new(
        LobeId::Sensory,
        LobeId::Cognitive,
        MessageKind::WorldState,
        1,
        &vec![0u8; 128],
    )
    .unwrap();

    group.bench_function("zero_fill_detect", |b| {
        b.iter(|| {
            let decision = gate.inspect(black_box(&zero_fill));
            assert_eq!(decision, Decision::Quarantine);
            black_box(decision);
        });
    });

    // Source spoofing attack
    let spoofed = Message::new(
        LobeId::Cognitive, // Wrong source for WorldState
        LobeId::Executive,
        MessageKind::WorldState,
        1,
        &vec![42i8 as u8; 64],
    )
    .unwrap();

    group.bench_function("source_spoof_detect", |b| {
        b.iter(|| {
            let decision = gate.inspect(black_box(&spoofed));
            assert_eq!(decision, Decision::Quarantine);
            black_box(decision);
        });
    });

    group.finish();
}

criterion_group!(
    benches,
    bench_gate_inspection,
    bench_payload_sizes,
    bench_profiler_overhead,
    bench_throughput,
    bench_attack_patterns,
);
criterion_main!(benches);
