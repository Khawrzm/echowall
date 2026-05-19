"""
niyah_executive — Niyah Engine Executive Lobe

LangGraph-based multi-agent supervisor implementing the
4-Layer Containment Architecture (Phalanx Shield):

  Layer 0 — Kernel Isolation       (rv32ima WASM boundary, enforced in BSP)
  Layer 1 — Prompt Integrity        (integrity_gate.py)
  Layer 2 — Activation Inspection   (activation_inspector.py)
  Layer 3 — Static Auditing         (static_auditor.py)
  Layer 4 — Network Egress Control  (tarpit_controller.py)

Python 3.12+. Requires: langgraph>=0.2, langchain-core>=0.2
"""

__version__ = "0.1.0"
__all__ = ["supervisor"]
