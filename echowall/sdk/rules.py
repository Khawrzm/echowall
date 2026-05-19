"""RulesEngine — if/then automation on EchoWall events. No cloud logic engine.

Example
-------
    from echowall.sdk import EchoWallClient, RulesEngine

    client = EchoWallClient(host="192.168.1.50")
    rules = RulesEngine(client)

    # Turn lights on when someone enters
    rules.when("presence").then(lambda r: lights.on())

    # Alert if someone falls
    rules.when("fall").then(lambda r: send_local_alert("Fall detected!"))

    # Turn AC off when room empty for 5 minutes
    rules.when("empty").after(seconds=300).then(lambda r: ac.off())

    rules.start()
"""

from __future__ import annotations

import logging
import threading
import time
from typing import Callable, Optional

log = logging.getLogger("echowall.rules")


class _Rule:
    def __init__(self, event: str):
        self.event = event
        self._delay: float = 0.0
        self._actions: list[Callable] = []
        self._condition: Optional[Callable] = None

    def after(self, seconds: float) -> "_Rule":
        """Wait N seconds after event before firing."""
        self._delay = seconds
        return self

    def when_confidence(self, above: float) -> "_Rule":
        """Only fire when confidence is above threshold (0.0–1.0)."""
        self._condition = lambda r: r.get("confidence", 0) > above
        return self

    def then(self, fn: Callable) -> "_Rule":
        """Action to execute when rule fires."""
        self._actions.append(fn)
        return self

    def _execute(self, result: dict) -> None:
        if self._condition and not self._condition(result):
            return
        if self._delay > 0:
            threading.Timer(self._delay, self._run_actions, args=[result]).start()
        else:
            self._run_actions(result)

    def _run_actions(self, result: dict) -> None:
        for fn in self._actions:
            try:
                fn(result)
            except Exception as exc:  # noqa: BLE001
                log.warning("Rule action error: %s", exc)


class RulesEngine:
    """Attach if/then rules to EchoWall events."""

    def __init__(self, client) -> None:
        self._client = client
        self._rules: list[_Rule] = []

    def when(self, event: str) -> _Rule:
        """Create a new rule for the given event."""
        rule = _Rule(event)
        self._rules.append(rule)
        return rule

    def start(self) -> None:
        """Wire rules to client and start."""
        for rule in self._rules:
            self._client._callbacks.setdefault(rule.event, []).append(rule._execute)
        self._client.start()
        log.info("RulesEngine started with %d rules.", len(self._rules))
