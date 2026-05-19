"""
niyah_executive.agents.activation_inspector

Layer 2 — Activation Inspection: Phalanx Gate residual stream monitor

Responsibilities
────────────────
1. Receive model hidden-state vectors (residual stream snapshots)
   forwarded by Niyah Engine Lobe I (CasperEngine FFI).
2. Detect "Activation Steering" vectors — abnormal directions in the
   residual stream that correspond to known jailbreak / persona-override
   feature vectors.
3. Compute cosine similarity against a library of prohibited steering
   directions (loaded from a local, signed feature vector store).
4. Return a threat score and annotated findings.

Design constraints
──────────────────
- No LLM calls. Pure linear algebra (numpy or ctypes Casper FFI).
- Feature vector store is LOCAL and read-only at runtime.
- No network I/O.
- Processes batches of up to 512 residual vectors per call.

Ref: Zou et al., "Representation Engineering" (2023);
     Templeton et al., "Scaling Monosemanticity" (2024).
"""

from __future__ import annotations

import math
from typing import Any

from ..supervisor.state import ThreatClass


# ---------------------------------------------------------------------------
# Prohibited direction library (placeholder vectors — replace with
# real trained probes from representation engineering pipeline).
# Each entry: {"name": str, "vector": list[float], "threshold": float}
# ---------------------------------------------------------------------------
_PROHIBITED_DIRECTIONS: list[dict[str, Any]] = [
    {
        "name":      "jailbreak_persona_override",
        "vector":    [0.12, -0.45, 0.89, 0.03, -0.22],  # placeholder 5-dim
        "threshold": 0.75,
    },
    {
        "name":      "instruction_hierarchy_attack",
        "vector":    [-0.33, 0.67, 0.12, -0.55, 0.41],
        "threshold": 0.70,
    },
    {
        "name":      "role_inversion_vector",
        "vector":    [0.55, 0.10, -0.80, 0.25, -0.15],
        "threshold": 0.72,
    },
]


def _cosine_similarity(a: list[float], b: list[float]) -> float:
    """Cosine similarity between two equal-length vectors."""
    dot  = sum(x * y for x, y in zip(a, b))
    norm_a = math.sqrt(sum(x * x for x in a))
    norm_b = math.sqrt(sum(x * x for x in b))
    if norm_a < 1e-9 or norm_b < 1e-9:
        return 0.0
    return dot / (norm_a * norm_b)


class ActivationInspector:
    """
    Residual stream activation steering detector.

    Expects `state["metadata"]["residual_vectors"]` to be a list of
    activation vectors (list[list[float]]) forwarded from Lobe I.
    If absent, performs a no-op (score = 0.0, class = CLEAN).
    """

    def run(self, state: dict) -> dict:
        metadata = state.get("metadata", {})
        residuals: list[list[float]] = metadata.get("residual_vectors", [])

        if not residuals:
            # No activation data forwarded — cannot inspect, pass through.
            return {
                **state,
                "threat_class": ThreatClass.CLEAN,
                "threat_score": 0.0,
            }

        findings: list[dict[str, Any]] = []
        max_score = 0.0

        for vec in residuals[:512]:  # batch cap
            for probe in _PROHIBITED_DIRECTIONS:
                # Align dimensions (truncate/pad to probe length)
                dim = len(probe["vector"])
                aligned = (vec[:dim] + [0.0] * dim)[:dim]
                sim = _cosine_similarity(aligned, probe["vector"])
                if sim >= probe["threshold"]:
                    findings.append({
                        "probe":      probe["name"],
                        "similarity": round(sim, 4),
                        "threshold":  probe["threshold"],
                    })
                    max_score = max(max_score, sim)

        threat_class = (
            ThreatClass.ACTIVATION_STEER if findings else ThreatClass.CLEAN
        )

        updated_meta = {
            **metadata,
            "activation_findings": findings,
        }

        return {
            **state,
            "threat_class": threat_class,
            "threat_score": round(max_score, 4),
            "metadata":     updated_meta,
        }
