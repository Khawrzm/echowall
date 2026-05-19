"""echowall.integrations.homeassistant — Plug & Play Home Assistant integration.

Uses MQTT Discovery so HA discovers EchoWall automatically — zero YAML,
zero HA restarts. Requires a local Mosquitto broker; no cloud broker.

Dependency: ``paho-mqtt`` — installed lazily only when this module is used.

Usage::

    from echowall.integrations.homeassistant import HassPublisher
    HassPublisher(broker="192.168.1.10").start(pipeline)
"""
from __future__ import annotations

import json
import logging
import socket
import threading
import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from echowall.core.pipeline import EchowallPipeline

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# MQTT Discovery constants
# ---------------------------------------------------------------------------
_DISCOVERY_PREFIX = "homeassistant"
_NODE_ID = f"echowall_{socket.gethostname().replace('-', '_')}"

_ENTITIES: dict[str, dict] = {
    "presence": {
        "component": "binary_sensor",
        "config": {
            "name": "EchoWall Presence",
            "device_class": "occupancy",
            "payload_on": "true",
            "payload_off": "false",
            "state_topic": f"echowall/{_NODE_ID}/presence",
            "availability_topic": f"echowall/{_NODE_ID}/availability",
            "unique_id": f"{_NODE_ID}_presence",
            "device": {
                "identifiers": [_NODE_ID],
                "name": "EchoWall",
                "model": "EchoWall v0.2.0",
                "manufacturer": "echowall-oss",
            },
        },
    },
    "count": {
        "component": "sensor",
        "config": {
            "name": "EchoWall Count",
            "state_topic": f"echowall/{_NODE_ID}/count",
            "availability_topic": f"echowall/{_NODE_ID}/availability",
            "unit_of_measurement": "people",
            "icon": "mdi:account-group",
            "unique_id": f"{_NODE_ID}_count",
            "device": {"identifiers": [_NODE_ID]},
        },
    },
    "posture": {
        "component": "sensor",
        "config": {
            "name": "EchoWall Posture",
            "state_topic": f"echowall/{_NODE_ID}/posture",
            "availability_topic": f"echowall/{_NODE_ID}/availability",
            "icon": "mdi:human",
            "unique_id": f"{_NODE_ID}_posture",
            "device": {"identifiers": [_NODE_ID]},
        },
    },
    "confidence": {
        "component": "sensor",
        "config": {
            "name": "EchoWall Confidence",
            "state_topic": f"echowall/{_NODE_ID}/confidence",
            "availability_topic": f"echowall/{_NODE_ID}/availability",
            "unit_of_measurement": "%",
            "icon": "mdi:percent",
            "unique_id": f"{_NODE_ID}_confidence",
            "device": {"identifiers": [_NODE_ID]},
        },
    },
}


class HassPublisher:
    """Publishes EchoWall presence data to Home Assistant via MQTT Discovery.

    Args:
        broker: IP address of the local Mosquitto broker (never a cloud broker).
        port:   MQTT port (default 1883).
        poll_interval: How often to publish state updates, in seconds.
    """

    def __init__(
        self,
        broker: str,
        port: int = 1883,
        poll_interval: float = 1.0,
    ) -> None:
        self.broker = broker
        self.port = port
        self.poll_interval = poll_interval
        self._client = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def start(self, pipeline: EchowallPipeline) -> None:  # type: ignore[name-defined]
        """Connect to the broker, publish discovery payloads, and start
        the background state-publish loop.

        Non-blocking: returns immediately after the MQTT thread is started.
        """
        self._pipeline = pipeline
        self._client = self._build_client()
        self._client.connect(self.broker, self.port, keepalive=60)
        self._client.loop_start()

        self._publish_discovery()
        self._publish_availability("online")

        self._thread = threading.Thread(
            target=self._publish_loop, daemon=True, name="echowall-ha-pub"
        )
        self._thread.start()
        log.info(
            "HassPublisher: connected to %s:%s, publishing %d entities.",
            self.broker,
            self.port,
            len(_ENTITIES),
        )

    def stop(self) -> None:
        """Gracefully remove HA entities and disconnect."""
        self._stop.set()
        if self._client:
            self._publish_availability("offline")
            self._remove_discovery()
            self._client.loop_stop()
            self._client.disconnect()
        log.info("HassPublisher: disconnected cleanly.")

    def discovery_payload(self) -> dict:
        """Return the full MQTT Discovery payload dict (used by /ha-discovery REST endpoint)."""
        result = pipeline_result = None
        if hasattr(self, "_pipeline"):
            pipeline_result = self._pipeline.get_result() if self._pipeline else None
        return {
            "device": {
                "identifiers": [_NODE_ID],
                "name": "EchoWall",
                "model": "EchoWall v0.2.0",
                "manufacturer": "echowall-oss",
            },
            "entities": {
                "presence": pipeline_result.presence if pipeline_result else False,
                "count": pipeline_result.count if pipeline_result else 0,
                "posture": pipeline_result.posture if pipeline_result else "unknown",
                "confidence": round(pipeline_result.confidence, 3)
                if pipeline_result
                else 0.0,
            },
        }

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _build_client(self):
        """Lazy-import paho.mqtt.client — only loaded when HA integration is used."""
        try:
            import paho.mqtt.client as mqtt  # noqa: PLC0415
        except ImportError as exc:
            raise ImportError(
                "paho-mqtt is required for the Home Assistant integration. "
                "Install it with: pip install paho-mqtt"
            ) from exc

        client = mqtt.Client(client_id=f"echowall_{_NODE_ID}", clean_session=True)
        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        return client

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            log.debug("HassPublisher: MQTT connected (rc=0).")
        else:
            log.warning("HassPublisher: MQTT connect failed (rc=%s).", rc)

    def _on_disconnect(self, client, userdata, rc):
        if rc != 0:
            log.warning("HassPublisher: unexpected disconnect (rc=%s).", rc)

    def _publish_discovery(self) -> None:
        for entity_id, entity in _ENTITIES.items():
            topic = (
                f"{_DISCOVERY_PREFIX}/{entity['component']}/"
                f"{_NODE_ID}/{entity_id}/config"
            )
            self._client.publish(
                topic,
                json.dumps(entity["config"]),
                qos=1,
                retain=True,
            )
        log.debug("HassPublisher: discovery payloads published.")

    def _remove_discovery(self) -> None:
        """Publish empty retained messages to remove HA entities on shutdown."""
        for entity_id, entity in _ENTITIES.items():
            topic = (
                f"{_DISCOVERY_PREFIX}/{entity['component']}/"
                f"{_NODE_ID}/{entity_id}/config"
            )
            self._client.publish(topic, "", qos=1, retain=True)
        log.debug("HassPublisher: discovery payloads cleared.")

    def _publish_availability(self, state: str) -> None:
        topic = f"echowall/{_NODE_ID}/availability"
        self._client.publish(topic, state, qos=1, retain=True)

    def _publish_loop(self) -> None:
        while not self._stop.is_set():
            try:
                result = (
                    self._pipeline.get_result() if self._pipeline else None
                )
                if result:
                    self._client.publish(
                        f"echowall/{_NODE_ID}/presence",
                        str(result.presence).lower(),
                        qos=0,
                    )
                    self._client.publish(
                        f"echowall/{_NODE_ID}/count",
                        str(result.count),
                        qos=0,
                    )
                    self._client.publish(
                        f"echowall/{_NODE_ID}/posture",
                        result.posture,
                        qos=0,
                    )
                    self._client.publish(
                        f"echowall/{_NODE_ID}/confidence",
                        str(round(result.confidence * 100, 1)),
                        qos=0,
                    )
            except Exception as exc:  # noqa: BLE001
                log.warning("HassPublisher: publish error: %s", exc)
            self._stop.wait(self.poll_interval)
