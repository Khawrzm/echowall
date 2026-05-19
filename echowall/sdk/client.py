"""EchoWallClient — connect to any EchoWall node on your local network.

No cloud. No API keys. Works over plain HTTP + WebSocket on LAN.
"""

from __future__ import annotations

import json
import logging
import threading
import time
import urllib.request
from typing import Callable, Optional

log = logging.getLogger("echowall.sdk")


class EchoWallClient:
    """Connect to an EchoWall node and receive presence events.

    Parameters
    ----------
    host:      IP or hostname of EchoWall node. None = auto-discover on LAN.
    port:      API port (default 8765).
    interval:  Polling interval in seconds when WebSocket unavailable.

    Example
    -------
    >>> client = EchoWallClient(host="192.168.1.50")
    >>> client.on_presence(lambda r: print("People:", r["count"]))
    >>> client.start()   # non-blocking background thread
    """

    def __init__(
        self,
        host: Optional[str] = None,
        port: int = 8765,
        interval: float = 1.0,
    ) -> None:
        if host is None:
            from echowall.sdk.node_discovery import discover_nodes
            nodes = discover_nodes(timeout=3)
            if not nodes:
                raise RuntimeError(
                    "No EchoWall node found on local network.\n"
                    "Start one with: echowall run"
                )
            host = nodes[0]
            log.info("Auto-discovered EchoWall node: %s", host)

        self.host = host
        self.port = port
        self.interval = interval
        self._base = f"http://{host}:{port}"
        self._callbacks: dict[str, list[Callable]] = {
            "presence": [],
            "empty": [],
            "intrusion": [],
            "fall": [],
            "any": [],
        }
        self._last: Optional[dict] = None
        self._thread: Optional[threading.Thread] = None
        self._running = False

    # ------------------------------------------------------------------
    # Event registration
    # ------------------------------------------------------------------

    def on_presence(self, fn: Callable) -> "EchoWallClient":
        """Called when at least one person is detected."""
        self._callbacks["presence"].append(fn)
        return self

    def on_empty(self, fn: Callable) -> "EchoWallClient":
        """Called when room becomes empty."""
        self._callbacks["empty"].append(fn)
        return self

    def on_intrusion(self, fn: Callable) -> "EchoWallClient":
        """Called when presence detected outside normal hours or unexpected."""
        self._callbacks["intrusion"].append(fn)
        return self

    def on_fall(self, fn: Callable) -> "EchoWallClient":
        """Called when posture = fallen (fall detection)."""
        self._callbacks["fall"].append(fn)
        return self

    def on_any(self, fn: Callable) -> "EchoWallClient":
        """Called on every result update."""
        self._callbacks["any"].append(fn)
        return self

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------

    def start(self) -> "EchoWallClient":
        """Start background polling. Non-blocking."""
        self._running = True
        self._thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._thread.start()
        return self

    def stop(self) -> None:
        self._running = False

    def get(self) -> Optional[dict]:
        """Synchronous one-shot presence read."""
        return self._fetch()

    def health(self) -> dict:
        """Check if node is alive."""
        return self._fetch("/health")

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _fetch(self, path: str = "/presence") -> Optional[dict]:
        try:
            with urllib.request.urlopen(  # noqa: S310
                f"{self._base}{path}", timeout=5
            ) as resp:
                return json.loads(resp.read())
        except Exception as exc:
            log.warning("EchoWall fetch failed: %s", exc)
            return None

    def _poll_loop(self) -> None:
        prev_presence = None
        while self._running:
            result = self._fetch()
            if result:
                self._last = result
                self._fire("any", result)

                presence = result.get("presence", False)
                posture = result.get("posture", "unknown")

                # fire presence / empty on state change
                if presence != prev_presence:
                    if presence:
                        self._fire("presence", result)
                    else:
                        self._fire("empty", result)
                    prev_presence = presence

                # fall detection — every time posture = fallen
                if posture == "fallen":
                    self._fire("fall", result)

            time.sleep(self.interval)

    def _fire(self, event: str, result: dict) -> None:
        for fn in self._callbacks.get(event, []):
            try:
                fn(result)
            except Exception as exc:  # noqa: BLE001
                log.warning("Callback error [%s]: %s", event, exc)
