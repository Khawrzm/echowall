"""
niyah_executive.supervisor.graph

Phalanx Shield LangGraph supervisor.

Routing logic
─────────────
  ALL inputs enter integrity_gate first.
  integrity_gate classifies and sanitizes, then routes:
    UNICODE_SMUGGLER / PROMPT_INJECTION  → integrity_gate loop (re-sanitize)
    ACTIVATION_STEER                     → activation_inspector
    CONTRACT_VULN                        → static_auditor
    EXFIL_ATTEMPT                        → tarpit_controller
    CLEAN                                → pass_through (forward to Lobe II)
    score > HALT_THRESHOLD               → halt

All nodes are pure functions: (state: dict) -> dict.
No node has side effects outside its declared output fields.
"""

from __future__ import annotations

import logging
from typing import Literal

from langgraph.graph import StateGraph, END

from .state import PhalanxState, ThreatClass, RouteDecision, initial_state
from ..agents.integrity_gate       import IntegrityGate
from ..agents.activation_inspector import ActivationInspector
from ..agents.static_auditor       import StaticAuditor
from ..agents.tarpit_controller    import TarpitController

logger = logging.getLogger(__name__)

HALT_THRESHOLD = 0.95   # threat_score above which we halt unconditionally
MAX_SANITIZE_LOOPS = 3  # prevent infinite re-sanitization loops

# ---------------------------------------------------------------------------
# Node wrappers (thin adapters around agent classes)
# ---------------------------------------------------------------------------

_integrity_gate       = IntegrityGate()
_activation_inspector = ActivationInspector()
_static_auditor       = StaticAuditor()
_tarpit_controller    = TarpitController()


def node_integrity_gate(state: dict) -> dict:
    logger.info("[phalanx] integrity_gate: processing")
    return _integrity_gate.run(state)


def node_activation_inspector(state: dict) -> dict:
    logger.info("[phalanx] activation_inspector: scanning residual stream")
    return _activation_inspector.run(state)


def node_static_auditor(state: dict) -> dict:
    logger.info("[phalanx] static_auditor: running formal verification")
    return _static_auditor.run(state)


def node_tarpit_controller(state: dict) -> dict:
    logger.info("[phalanx] tarpit_controller: generating egress rules")
    return _tarpit_controller.run(state)


def node_pass_through(state: dict) -> dict:
    logger.info("[phalanx] pass_through: input is clean, forwarding to Lobe II")
    return state


def node_halt(state: dict) -> dict:
    reason = state.get("halt_reason", "unspecified")
    logger.critical("[phalanx] HALT — %s", reason)
    return {**state, "route": RouteDecision.HALT}


# ---------------------------------------------------------------------------
# Conditional router
# ---------------------------------------------------------------------------

def route_after_gate(state: dict) -> str:
    """
    Determine next node after integrity_gate.
    Returns the node name string consumed by LangGraph add_conditional_edges.
    """
    score = state.get("threat_score", 0.0)
    tc    = state.get("threat_class", ThreatClass.UNKNOWN)

    if score >= HALT_THRESHOLD:
        return "halt"

    routing: dict[ThreatClass, str] = {
        ThreatClass.CLEAN:            "pass_through",
        ThreatClass.UNICODE_SMUGGLER: "integrity_gate",   # re-sanitize
        ThreatClass.PROMPT_INJECTION: "integrity_gate",   # re-sanitize
        ThreatClass.ACTIVATION_STEER: "activation_inspector",
        ThreatClass.CONTRACT_VULN:    "static_auditor",
        ThreatClass.EXFIL_ATTEMPT:    "tarpit_controller",
        ThreatClass.UNKNOWN:          "halt",
    }
    return routing.get(tc, "halt")


def route_after_inspector(state: dict) -> str:
    score = state.get("threat_score", 0.0)
    if score >= HALT_THRESHOLD:
        return "halt"
    return "pass_through"


def route_after_auditor(state: dict) -> str:
    findings = state.get("audit_report", [])
    critical = [f for f in findings if f.get("severity") == "critical"]
    if critical:
        return "halt"
    return "pass_through"


def route_after_tarpit(state: dict) -> str:
    # After tarpit rules are issued, always halt the current chain
    # (exfil attempt means the session is untrusted).
    return "halt"


# ---------------------------------------------------------------------------
# Graph construction
# ---------------------------------------------------------------------------

def build_phalanx_graph() -> StateGraph:
    """Construct and compile the Phalanx Shield StateGraph."""
    g = StateGraph(PhalanxState)

    # Register nodes
    g.add_node("integrity_gate",       node_integrity_gate)
    g.add_node("activation_inspector", node_activation_inspector)
    g.add_node("static_auditor",       node_static_auditor)
    g.add_node("tarpit_controller",    node_tarpit_controller)
    g.add_node("pass_through",         node_pass_through)
    g.add_node("halt",                 node_halt)

    # Entry point: all inputs start at integrity_gate
    g.set_entry_point("integrity_gate")

    # Conditional routing from integrity_gate
    g.add_conditional_edges(
        "integrity_gate",
        route_after_gate,
        {
            "integrity_gate":       "integrity_gate",
            "activation_inspector": "activation_inspector",
            "static_auditor":       "static_auditor",
            "tarpit_controller":    "tarpit_controller",
            "pass_through":         "pass_through",
            "halt":                 "halt",
        },
    )

    g.add_conditional_edges(
        "activation_inspector",
        route_after_inspector,
        {"pass_through": "pass_through", "halt": "halt"},
    )

    g.add_conditional_edges(
        "static_auditor",
        route_after_auditor,
        {"pass_through": "pass_through", "halt": "halt"},
    )

    g.add_conditional_edges(
        "tarpit_controller",
        route_after_tarpit,
        {"halt": "halt"},
    )

    g.add_edge("pass_through", END)
    g.add_edge("halt",         END)

    return g.compile()


# Module-level compiled graph (singleton)
phalanx = build_phalanx_graph()


def run(raw_input: str | bytes) -> dict:
    """Run the Phalanx Shield on a single input. Returns final state."""
    return phalanx.invoke(initial_state(raw_input))
