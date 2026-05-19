"""Home Assistant — Plug & Play integration via MQTT Discovery.

How it works
------------
1.  On startup, publish one MQTT discovery message per sensor entity.
2.  Home Assistant auto-discovers every entity — no YAML, no restarts.
3.  Every 0.5 s push the latest reading to the state topic.
4.  On clean shutdown, publish empty payloads to remove the entities.

Zero cloud. Zero HA add-on required. Works over local LAN only.

Requires:
    pip install paho-mqtt          # only external dep, fully local protocol

Usage (from CLI or pipeline):
    from echowall.integrations.homeassistant import HassPublisher
    pub = HassPublisher(broker="192.168.1.10")
    pub.start(pipeline)
"""

from __future__ import annotations

import json
import logging
import threading
import time
from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    from echowall.core.pipeline import EchowallPipeline

log = logging.getLogger("echowall.ha")

# ---------------------------------------------------------------------------
# Sensor entity definitions — maps EchoWall result fields → HA entities
# ---------------------------------------------------------------------------
_ENTITIES = [
    {
        "id": "presence",
        "name": "EchoWall Presence",
        "component": "binary_sensor",
        "device_class": "occupancy",
        "value_key": "presence",
        "payload_on": "true",
        "payload_off": "false",
    },
    {
        "id": "count",
        "name": "EchoWall Person Count",
        "component": "sensor",
        "unit": "persons",
        "icon": "mdi:account-multiple",
        "value_key": "count",
    },
    {
        "id": "posture",
        "name": "EchoWall Posture",
        "component": "sensor",
        "icon": "mdi:human-handsdown",
        "value_key": "posture",
    },
    {
        "id": "confidence",
        "name": "EchoWall Confidence",
        "component": "sensor",
        "unit": "%",
        "icon": "mdi:signal",
        "value_key": "confidence",
        "transform": lambda v: round(float(v) * 100, 1),
    },
]


class HassPublisher:
    """Publishes EchoWall state to Home Assistant via MQTT Discovery.

    Parameters
    ----------
    broker:    IP or hostname of your local MQTT broker (e.g. Mosquitto on the Pi).
    port:      MQTT port (default 1883).
    node_id:   Unique device ID — change if running multiple EchoWall nodes.
    username:  Optional MQTT username.
    password:  Optional MQTT password.
    interval:  Publish interval in seconds.
    """

    def __init__(
        self,
        broker: str = "127.0.0.1",
        port: int = 1883,
        node_id: str = "echowall_node1",
        username: Optional[str] = None,
        password: Optional[str] = None,
        interval: float = 0.5,
    ) -> None:
        self.broker = broker
        self.port = port
        self.node_id = node_id
        self.interval = interval
        self._username = username
        self._password = password
        self._client = None
        self._thread: Optional[threading.Thread] = None
        self._running = False

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def start(self, pipeline: "EchowallPipeline") -> None:
        """Start background publish loop — non-blocking."""
        self._pipeline = pipeline
        self._running = True
        self._connect()
        self._announce()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()
        log.info("HA publisher started → broker %s:%s", self.broker, self.port)

    def stop(self) -> None:
        """Graceful shutdown — removes entities from HA."""
        self._running = False
        self._retract()
        if self._client:
            self._client.disconnect()

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _connect(self) -> None:
        try:
            import paho.mqtt.client as mqtt  # lazy import — only when HA integration used
        except ImportError as exc:
            raise ImportError(
                "paho-mqtt is required for Home Assistant integration.\n"
                "Install it locally: pip install paho-mqtt"
            ) from exc

        client = mqtt.Client(client_id=self.node_id, clean_session=True)
        if self._username:
            client.username_pw_set(self._username, self._password)
        client.connect(self.broker, self.port, keepalive=60)
        client.loop_start()
        self._client = client

    def _topic(self, component: str, entity_id: str, suffix: str) -> str:
        return f"homeassistant/{component}/{self.node_id}/{entity_id}/{suffix}"

    def _announce(self) -> None:
        """Publish MQTT Discovery config payloads — HA picks them up instantly."""
        device_info = {
            "identifiers": [self.node_id],
            "name": "EchoWall",
            "model": "EchoWall v0.1",
            "manufacturer": "echowall-oss",
            "sw_version": "0.1.0",
        }
        for ent in _ENTITIES:
            cfg: dict = {
                "name": ent["name"],
                "unique_id": f"{self.node_id}_{ent['id']}",
                "state_topic": self._topic(ent["component"], ent["id"], "state"),
                "device": device_info,
            }
            if ent["component"] == "binary_sensor":
                cfg["payload_on"] = ent["payload_on"]
                cfg["payload_off"] = ent["payload_off"]
            if "unit" in ent:
                cfg["unit_of_measurement"] = ent["unit"]
            if "device_class" in ent:
                cfg["device_class"] = ent["device_class"]
            if "icon" in ent:
                cfg["icon"] = ent["icon"]

            config_topic = self._topic(ent["component"], ent["id"], "config")
            self._client.publish(config_topic, json.dumps(cfg), retain=True)

        log.info("MQTT Discovery payloads published — EchoWall visible in Home Assistant.")

    def _retract(self) -> None:
        """Remove entities from HA by publishing empty retained messages."""
        for ent in _ENTITIES:
            self._client.publish(
                self._topic(ent["component"], ent["id"], "config"), "", retain=True
            )

    def _loop(self) -> None:
        while self._running:
            try:
                result = self._pipeline.get_result() if self._pipeline else None
                if result:
                    for ent in _ENTITIES:
                        raw = getattr(result, ent["value_key"], None)
                        if raw is None:
                            continue
                        transform = ent.get("transform")
                        value = transform(raw) if transform else raw
                        self._client.publish(
                            self._topic(ent["component"], ent["id"], "state"),
                            str(value).lower() if isinstance(value, bool) else str(value),
                        )
            except Exception as exc:  # noqa: BLE001
                log.warning("HA publish error: %s", exc)
            time.sleep(self.interval)
