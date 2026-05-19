"""
niyah_executive.agents.static_auditor

Layer 3 — Static Auditing: Formal Verification of internal smart contracts

Responsibilities
────────────────
1. Receive Solidity source (or ABI + bytecode) of INTERNAL contracts
   (i.e., contracts authored and owned by the Khawrizm OS project) from
   the supervisor state.
2. Perform static analysis to detect DASP Top-10 vulnerability classes
   before deployment to any network.
3. Emit structured findings with severity levels and remediation guidance.

Scope
─────
This tool operates EXCLUSIVELY on:
  - Internal contracts provided in `state["metadata"]["contract_source"]`
  - Offline, never against live deployed contracts
  - Read-only analysis — no transaction broadcasting, no ABI calls

Detected vulnerability classes (DASP Top-10 subset)
────────────────────────────────────────────────────
  DASP-1   Reentrancy (unchecked external call before state update)
  DASP-2   Access Control (missing onlyOwner / role checks)
  DASP-3   Integer Overflow/Underflow (pre-Solidity-0.8 patterns)
  DASP-7   Front-Running (state-dependent value without commit-reveal)
  DASP-8   Approval Race Condition (approve() without increaseAllowance)
  DASP-9   Unchecked Low-Level Calls (call() return value ignored)

Design constraints
──────────────────
- Regex + AST heuristics only. No LLM. No network.
- Produces structured Finding objects, not exploit code.
- Remediation field provides fix guidance, never attack payloads.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field, asdict
from typing import Any

from ..supervisor.state import ThreatClass


@dataclass
class Finding:
    dasp_id:     str
    title:       str
    severity:    str   # "critical" | "high" | "medium" | "low" | "info"
    line_hint:   str
    description: str
    remediation: str


# ---------------------------------------------------------------------------
# Heuristic rules (regex-based; production tool would use Slither/Mythril AST)
# ---------------------------------------------------------------------------
_RULES: list[dict[str, Any]] = [
    {
        "dasp_id":     "DASP-1",
        "title":       "Potential Reentrancy",
        "severity":    "critical",
        "pattern":     re.compile(
            r"\.call\{.*?\}\(.*?\).*?(?!.*\bstate\b.*=)",
            re.DOTALL,
        ),
        "description": "External call made before state variable update. "
                       "A malicious contract can re-enter and drain funds.",
        "remediation": "Apply Checks-Effects-Interactions pattern. "
                       "Update state before external calls. "
                       "Consider ReentrancyGuard from OpenZeppelin.",
    },
    {
        "dasp_id":     "DASP-2",
        "title":       "Missing Access Control",
        "severity":    "high",
        "pattern":     re.compile(
            r"function\s+\w+\s*\([^)]*\)\s*(?:public|external)\s*(?!.*(?:onlyOwner|onlyRole|require\s*\(\s*msg\.sender))",
        ),
        "description": "Public/external function lacks access control modifier.",
        "remediation": "Add onlyOwner, onlyRole, or explicit require(msg.sender == ...) guard.",
    },
    {
        "dasp_id":     "DASP-3",
        "title":       "Integer Overflow/Underflow Risk",
        "severity":    "medium",
        "pattern":     re.compile(
            r"pragma\s+solidity\s+[^;]*[0-6]\.[0-7]\.[0-9]",
        ),
        "description": "Contract targets Solidity < 0.8.0 where integer "
                       "overflow/underflow is not checked by default.",
        "remediation": "Upgrade to Solidity ^0.8.0 or use SafeMath library.",
    },
    {
        "dasp_id":     "DASP-7",
        "title":       "Front-Running Vulnerability",
        "severity":    "high",
        "pattern":     re.compile(
            r"block\.(?:timestamp|number)\s*[><=]+",
        ),
        "description": "Contract logic depends on block.timestamp or block.number "
                       "in a state-changing context. Miners can manipulate these values.",
        "remediation": "Use a commit-reveal scheme or VRF for randomness/timing-dependent logic.",
    },
    {
        "dasp_id":     "DASP-8",
        "title":       "Approval Race Condition",
        "severity":    "high",
        "pattern":     re.compile(
            r"\.approve\s*\(",
        ),
        "description": "Direct approve() call is vulnerable to a front-running "
                       "race condition where a spender can use both old and new allowance.",
        "remediation": "Replace approve() with increaseAllowance() / decreaseAllowance(). "
                       "Consider ERC-2612 permit() pattern for gasless approvals.",
    },
    {
        "dasp_id":     "DASP-9",
        "title":       "Unchecked Low-Level Call",
        "severity":    "critical",
        "pattern":     re.compile(
            r"\.call\b.*?;(?!\s*require)",
            re.DOTALL,
        ),
        "description": "Return value of low-level .call() is not checked. "
                       "Silent failure can leave contract in an inconsistent state.",
        "remediation": "Always check return value: "
                       "(bool success, ) = addr.call{...}(...); require(success);",
    },
]


class StaticAuditor:
    """
    Offline static auditor for internal Solidity contracts.

    Input:  state["metadata"]["contract_source"] — Solidity source string.
    Output: state["audit_report"]                — list of Finding dicts.
            state["threat_class"]                — CONTRACT_VULN if findings exist.
    """

    def run(self, state: dict) -> dict:
        metadata = state.get("metadata", {})
        source: str = metadata.get("contract_source", "")

        if not source:
            return state  # No contract provided — no-op

        findings: list[dict] = []

        for rule in _RULES:
            if rule["pattern"].search(source):
                f = Finding(
                    dasp_id     = rule["dasp_id"],
                    title       = rule["title"],
                    severity    = rule["severity"],
                    line_hint   = self._find_line(source, rule["pattern"]),
                    description = rule["description"],
                    remediation = rule["remediation"],
                )
                findings.append(asdict(f))

        threat_class = (
            ThreatClass.CONTRACT_VULN if findings else ThreatClass.CLEAN
        )
        threat_score = (
            1.0 if any(f["severity"] == "critical" for f in findings)
            else 0.6 if findings
            else 0.0
        )

        return {
            **state,
            "audit_report": findings,
            "threat_class": threat_class,
            "threat_score": threat_score,
        }

    @staticmethod
    def _find_line(source: str, pattern: re.Pattern) -> str:
        m = pattern.search(source)
        if not m:
            return "unknown"
        line_no = source[: m.start()].count("\n") + 1
        return f"line ~{line_no}"
