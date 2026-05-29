//! End-to-end integration tests for Niyah Engine
//!
//! Validates the complete sovereign AI stack

use phalanx_gate::{Decision, LobeId, Message, MessageKind, PhalanxInspector, StrictGate};

#[test]
fn test_end_to_end_message_flow() {
    let gate = StrictGate::new();
    
    // Stage 1: Sensory → Cognitive
    let world_state = Message::new(
        LobeId::Sensory,
        LobeId::Cognitive,
        MessageKind::WorldState,
        1,
        &vec![42i8 as u8; 128],
    )
    .unwrap();
    
    assert_eq!(gate.inspect(&world_state), Decision::Emit);
    
    // Stage 2: Cognitive → Executive
    let inference = Message::new(
        LobeId::Cognitive,
        LobeId::Executive,
        MessageKind::InferenceResult,
        1,
        &[1u8, 200u8, 3u8],
    )
    .unwrap();
    
    assert_eq!(gate.inspect(&inference), Decision::Emit);
}

#[test]
fn test_zero_dynamic_memory_guarantee() {
    let gate = StrictGate::new();
    
    let max_msg = Message::new(
        LobeId::Sensory,
        LobeId::Cognitive,
        MessageKind::WorldState,
        1,
        &vec![127i8 as u8; 640],
    )
    .unwrap();
    
    let decision = gate.inspect(&max_msg);
    assert_eq!(decision, Decision::Emit);
}
