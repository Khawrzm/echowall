"""EchoNet — Transformer-based through-wall presence model."""

from __future__ import annotations
import logging
import time
import numpy as np
from typing import Optional

logger = logging.getLogger("echowall.echonet")

try:
    import torch
    import torch.nn as nn
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False
    logger.warning("PyTorch not available — EchoNet running in stub mode.")


CSI_ANTENNAS = 3
CSI_SUBCARRIERS = 64
RANGE_BINS = 128
D_MODEL = 128
N_HEADS = 4
N_LAYERS = 3
N_CLASSES = 5   # 0-person, 1-person, 2-people, 3-people, fallen
POSTURES = ["standing", "sitting", "fallen", "moving", "unknown"]


if TORCH_AVAILABLE:
    class CSIEmbedding(nn.Module):
        """Projects flattened CSI features into D_MODEL space."""
        def __init__(self):
            super().__init__()
            input_dim = CSI_ANTENNAS * CSI_SUBCARRIERS * 3 + RANGE_BINS  # amp+phase+diff+acoustic
            self.proj = nn.Sequential(
                nn.Linear(input_dim, D_MODEL * 2),
                nn.GELU(),
                nn.LayerNorm(D_MODEL * 2),
                nn.Linear(D_MODEL * 2, D_MODEL),
            )

        def forward(self, x):
            return self.proj(x)

    class EchoNetBackbone(nn.Module):
        """Transformer encoder for spatiotemporal CSI sequence modeling."""
        def __init__(self):
            super().__init__()
            self.embedding = CSIEmbedding()
            encoder_layer = nn.TransformerEncoderLayer(
                d_model=D_MODEL, nhead=N_HEADS, dim_feedforward=512,
                dropout=0.1, batch_first=True, norm_first=True,
            )
            self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=N_LAYERS)
            self.head = nn.Sequential(
                nn.LayerNorm(D_MODEL),
                nn.Linear(D_MODEL, N_CLASSES),
            )

        def forward(self, x):  # x: (B, T, features)
            emb = self.embedding(x)          # (B, T, D_MODEL)
            enc = self.encoder(emb)          # (B, T, D_MODEL)
            pooled = enc.mean(dim=1)         # global average pool over time
            return self.head(pooled)         # (B, N_CLASSES)


class EchoNet:
    """Inference wrapper for EchoNetBackbone."""

    def __init__(self, device: str = "cpu"):
        self.device = device
        self._model = None
        self._window: list = []
        self.window_size = 10   # frames

    def load_pretrained(self, path: Optional[str] = None):
        """Load pretrained weights. Falls back to random init for testing."""
        if not TORCH_AVAILABLE:
            logger.warning("PyTorch unavailable — stub inference active.")
            return
        import torch
        self._model = EchoNetBackbone().to(self.device)
        if path:
            self._model.load_state_dict(torch.load(path, map_location=self.device))
            logger.info("Loaded weights from %s", path)
        else:
            logger.warning("No weights path — using random initialization (demo mode).")
        self._model.eval()

    def infer(self, fused) -> "PresenceResult":
        """Run inference on a FusedFrame, return PresenceResult."""
        from echowall.core.pipeline import PresenceResult
        if not TORCH_AVAILABLE or self._model is None:
            return self._stub_infer(fused)

        import torch
        # Build feature vector
        feat = np.concatenate([
            fused.csi_amplitude.flatten(),
            fused.csi_phase.flatten(),
            fused.csi_diff.flatten(),
            fused.acoustic_range if fused.acoustic_range is not None else np.zeros(RANGE_BINS),
        ]).astype(np.float32)

        self._window.append(feat)
        if len(self._window) > self.window_size:
            self._window.pop(0)

        seq = np.stack(self._window)  # (T, features)
        x = torch.from_numpy(seq).unsqueeze(0).to(self.device)  # (1, T, features)

        with torch.no_grad():
            logits = self._model(x)  # (1, N_CLASSES)
            probs = torch.softmax(logits, dim=-1)[0].cpu().numpy()

        count_idx = int(np.argmax(probs[:4]))  # first 4 = occupancy
        posture_idx = int(np.argmax(probs))
        confidence = float(probs.max())

        return PresenceResult(
            presence=count_idx > 0,
            count=count_idx,
            posture=POSTURES[min(posture_idx, len(POSTURES) - 1)],
            confidence=confidence,
            timestamp=fused.timestamp,
        )

    def _stub_infer(self, fused) -> "PresenceResult":
        """Heuristic stub when PyTorch unavailable."""
        from echowall.core.pipeline import PresenceResult
        energy = float(np.mean(np.abs(fused.csi_diff)))
        presence = energy > 0.1
        return PresenceResult(
            presence=presence,
            count=1 if presence else 0,
            posture="unknown",
            confidence=0.5,
            timestamp=fused.timestamp,
        )
