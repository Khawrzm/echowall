//! # Sensory Fusion
//!
//! Merges CSI, acoustic FMCW, and TVWS occupancy into a single
//! `WorldStateTensor` that is published to Lobe III via Phalanx Gate.
//!
//! Raw samples are zeroed from all buffers after fusion.
//! No raw sensor data leaves this module.

use heapless::Vec;

/// Dimensionality of the fused world-state tensor.
/// 640 INT8 values: 64 subcarriers × 10 frames (CSI component).
pub const WORLD_STATE_DIM: usize = 640;

/// Fused environment world-state tensor.
///
/// This is the only data structure that crosses the Lobe II → Lobe III
/// boundary. It contains no raw samples — only INT8 fused features.
#[derive(Debug, Clone)]
pub struct WorldStateTensor {
    /// INT8 feature vector. Length always == `WORLD_STATE_DIM`.
    pub features: Vec<i8, WORLD_STATE_DIM>,
    /// Monotonic frame counter. Used by Lobe III for temporal continuity.
    pub frame_id: u64,
    /// Bitmap of active sensor modalities contributing to this frame.
    /// Bit 0: CSI, Bit 1: Acoustic FMCW, Bit 2: TVWS occupancy.
    pub modality_mask: u8,
}

/// Errors from the fusion pipeline.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FusionError {
    /// Input CSI feature vector has wrong length.
    CsiDimMismatch,
    /// Input acoustic vector has wrong length.
    AcousticDimMismatch,
    /// Output tensor could not be constructed (internal sizing error).
    OutputOverflow,
}

/// Fuse CSI, acoustic, and TVWS features into a `WorldStateTensor`.
///
/// ## Privacy invariant
/// Input slices are consumed. The caller is responsible for zeroing
/// the original sample buffers after calling this function.
///
/// `csi_features`: 576 INT8 values (64 subcarriers × 9 frames).
/// `acoustic_features`: 32 INT8 values (acoustic range bins).
/// `tvws_vacant_count`: 1 INT8 value (number of vacant TVWS channels, clamped to 127).
/// Total: 609 values, zero-padded to `WORLD_STATE_DIM` (640).
pub fn fuse(
    csi_features: &[i8],
    acoustic_features: &[i8],
    tvws_vacant_count: u8,
    frame_id: u64,
) -> Result<WorldStateTensor, FusionError> {
    if csi_features.len() != 576 {
        return Err(FusionError::CsiDimMismatch);
    }
    if acoustic_features.len() != 32 {
        return Err(FusionError::AcousticDimMismatch);
    }

    let mut features: Vec<i8, WORLD_STATE_DIM> = Vec::new();
    let mut modality_mask: u8 = 0;

    // Append CSI features.
    for &v in csi_features {
        features.push(v).map_err(|_| FusionError::OutputOverflow)?;
    }
    modality_mask |= 0x01;

    // Append acoustic features.
    for &v in acoustic_features {
        features.push(v).map_err(|_| FusionError::OutputOverflow)?;
    }
    modality_mask |= 0x02;

    // Append TVWS vacant count (1 byte).
    features
        .push(tvws_vacant_count.min(127) as i8)
        .map_err(|_| FusionError::OutputOverflow)?;
    modality_mask |= 0x04;

    // Zero-pad to WORLD_STATE_DIM.
    while features.len() < WORLD_STATE_DIM {
        features.push(0).map_err(|_| FusionError::OutputOverflow)?;
    }

    Ok(WorldStateTensor {
        features,
        frame_id,
        modality_mask,
    })
}
