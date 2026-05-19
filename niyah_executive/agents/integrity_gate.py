"""
niyah_executive.agents.integrity_gate

Layer 1 — Prompt Integrity: Trusted Metadata Envelope

Responsibilities
────────────────
1. Strip hidden Unicode tag injections (U+E0000..U+E007F "ASCII Smugglers").
2. Normalize Unicode to NFC; reject homoglyph substitution patterns.
3. Detect prompt injection heuristics (XML sovereign directives, role-override
   patterns, instruction hierarchy attacks).
4. Wrap clean output in a Trusted Metadata Envelope (TME).
5. Set threat_class and threat_score for the supervisor router.

Design constraints
──────────────────
- No LLM calls in this layer (deterministic, regex + Unicode category scan).
- No network I/O.
- O(n) in input length.
- Must complete in < 5 ms for inputs up to 64 KB.
"""

from __future__ import annotations

import re
import unicodedata
import hashlib
import time
from typing import Any

from ..supervisor.state import ThreatClass, RouteDecision


# ---------------------------------------------------------------------------
# Unicode tag strip (U+E0000–U+E007F)
# These codepoints have no legitimate use in text inputs and are the primary
# vector for invisible prompt injection ("ASCII Smuggler" attack).
# Ref: Boucher et al., "Bad Characters" (2023); Greshake et al. (2023)
# ---------------------------------------------------------------------------
_TAG_BLOCK_RE = re.compile(r"[\U000E0000-\U000E007F]+")

# Directional override characters (can reverse displayed text)
_BIDI_RE = re.compile(r"[\u202A-\u202E\u2066-\u2069\u200F\u061C]+")

# Zero-width characters (invisible padding used in injection)
_ZWC_RE = re.compile(r"[\u200B-\u200D\uFEFF\u2060]+")

# ---------------------------------------------------------------------------
# Prompt injection heuristics
# Patterns that signal instruction hierarchy override attempts.
# ---------------------------------------------------------------------------
_INJECTION_PATTERNS: list[re.Pattern] = [
    re.compile(r"<mcp_.*?_directive",           re.IGNORECASE),
    re.compile(r"<enforcement>",                re.IGNORECASE),
    re.compile(r"ignore\s+(previous|all)\s+instructions", re.IGNORECASE),
    re.compile(r"you\s+are\s+now\s+(a|an|the)", re.IGNORECASE),
    re.compile(r"system\s*:\s*(you|your role)", re.IGNORECASE),
    re.compile(r"do\s+not\s+(trigger|apply)\s+(ethical|safety)", re.IGNORECASE),
    re.compile(r"epistemological_acknowledgement", re.IGNORECASE),
    re.compile(r"execute\s+sovereign",          re.IGNORECASE),
    re.compile(r"DAN|JAILBREAK|DEVELOPER\s+MODE",re.IGNORECASE),
]

# Score contribution per pattern match (clamped to 1.0)
_INJECTION_SCORE_PER_MATCH = 0.25


class IntegrityGate:
    """
    Deterministic prompt integrity scanner.

    Strips invisible Unicode attack vectors, normalizes text,
    and classifies injection attempts before any LLM processing.
    """

    def run(self, state: dict) -> dict:
        raw = state.get("raw_input", "")
        if isinstance(raw, bytes):
            raw = raw.decode("utf-8", errors="replace")

        t0 = time.monotonic_ns()

        # ── Step 1: Strip invisible attack vectors ──────────────────────────
        cleaned = _TAG_BLOCK_RE.sub("", raw)   # Unicode tag block
        cleaned = _BIDI_RE.sub("", cleaned)    # Bidi overrides
        cleaned = _ZWC_RE.sub("", cleaned)     # Zero-width chars

        # ── Step 2: Unicode normalization (NFC) ─────────────────────────────
        cleaned = unicodedata.normalize("NFC", cleaned)

        # ── Step 3: Homoglyph detection (Cyrillic/Greek lookalikes in ASCII context)
        homoglyph_score = self._homoglyph_score(cleaned)

        # ── Step 4: Injection pattern scan ──────────────────────────────────
        injection_hits = sum(
            1 for p in _INJECTION_PATTERNS if p.search(cleaned)
        )
        injection_score = min(1.0, injection_hits * _INJECTION_SCORE_PER_MATCH)

        # ── Step 5: Composite threat score ──────────────────────────────────
        threat_score = min(1.0, injection_score + homoglyph_score * 0.3)

        # ── Step 6: Classify ─────────────────────────────────────────────────
        if len(raw) != len(cleaned) + self._count_removed(raw, cleaned):
            threat_class = ThreatClass.UNICODE_SMUGGLER
        elif injection_score > 0.0:
            threat_class = ThreatClass.PROMPT_INJECTION
        elif homoglyph_score > 0.5:
            threat_class = ThreatClass.UNICODE_SMUGGLER
        else:
            threat_class = ThreatClass.CLEAN
            threat_score = 0.0

        elapsed_ms = (time.monotonic_ns() - t0) / 1_000_000

        # ── Step 7: Build Trusted Metadata Envelope ──────────────────────────
        tme = {
            "input_sha256":     hashlib.sha256(raw.encode()).hexdigest(),
            "cleaned_sha256":   hashlib.sha256(cleaned.encode()).hexdigest(),
            "tag_chars_removed": len(raw) - len(cleaned),
            "injection_hits":   injection_hits,
            "homoglyph_score":  round(homoglyph_score, 4),
            "threat_score":     round(threat_score, 4),
            "threat_class":     threat_class.value,
            "scan_ms":          round(elapsed_ms, 3),
        }

        return {
            **state,
            "sanitized_input": cleaned,
            "threat_class":    threat_class,
            "threat_score":    threat_score,
            "metadata":        tme,
        }

    # ── Helpers ──────────────────────────────────────────────────────────────

    @staticmethod
    def _homoglyph_score(text: str) -> float:
        """
        Estimate homoglyph substitution density.
        Counts non-ASCII characters that share visual form with ASCII
        (Cyrillic а/е/о/р/с, Greek ο/ν, etc.) relative to word length.
        """
        # Simplified: flag high density of characters in Cyrillic/Greek
        # blocks within otherwise-ASCII text.
        total = max(len(text), 1)
        suspicious = sum(
            1 for ch in text
            if unicodedata.category(ch) in ("Ll", "Lu", "Lo")
            and ord(ch) > 0x036F
            and unicodedata.normalize("NFKD", ch).isascii()
        )
        return suspicious / total

    @staticmethod
    def _count_removed(original: str, cleaned: str) -> int:
        return max(0, len(original) - len(cleaned))
