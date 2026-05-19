"""Home Assistant integration — plug and play, no YAML required.

Two modes:
1. MQTT Discovery (recommended): EchoWall appears automatically in HA as a device.
2. REST Polling (fallback): Works without a MQTT broker, add 3 lines to configuration.yaml.

Usage (automatic):
    echowall run  # starts pipeline + HA discovery automatically if HA is on LAN

Usage (manual):
    from echowall.integrations.homeassistant import HassPublisher
    pub = HassPublisher(broker="192.168.1.10")  # optional, auto-detected if omitted
    pub.start(pipeline)
"""

from __future__ import annotations

import json
import logging
import socket
import threading
import time
from typing import Optional

log = logging.getLogger("echowall.hass")

_DISCOVERY_PREFIX = "homeassistant"
_NODE_ID = "echowall"


def _device_info() -> dict:
    return {
        "identifiers": [f"echowall_{socket.gethostname()}"],
        "name": "EchoWall",
        "model": "EchoWall v0.1",
        "manufacturer": "KHAWRIZM",
        "sw_version": "0.1.0",
    }


class HassPublisher:
    """Publishes EchoWall results to Home Assistant via MQTT auto-discovery.

    EchoWall appears as a device in HA with these entities:
    - binary_sensor.echowall_presence
    - sensor.echowall_count
    - sensor.echowall_posture
    - sensor.echowall_confidence
    - sensor.echowall_breathing_rate
    - sensor.echowall_heart_rate

    No YAML. No restart. Appears automatically.
    """

    ENTITIES = [
        {
            "id": "presence",
            "name": "Presence",
            "component": "binary_sensor",
            "device_class": "occupancy",
            "value_key": "presence",
            "payload_on": True,
            "payload_off": False,
        },
        {
            "id": "count",
            "name": "Occupancy Count",
            "component": "sensor",
            "unit": "people",
            "value_key": "count",
        },
        {
            "id": "posture",
            "name": "Posture",
            "component": "sensor",
            "value_key": "posture",
        },
        {
            "id": "confidence",
            "name": "Confidence",
            "component": "sensor",
            "unit": "%",
            "value_key": "confidence",
        },
        {
            "id": "breathing_rate",
            "name": "Breathing Rate",
            "component": "sensor",
            "unit": "bpm",
            "device_class": "frequency",
            "value_key": "breathing_rate",
        },
        {
            "id": "heart_rate",
            "name": "Heart Rate",
            "component": "sensor",
            "unit": "bpm",
            "device_class": "frequency",
            "value_key": "heart_rate",
        },
    ]

    def __init__(self, broker: Optional[str] = None, port: int = 1883):
        self.broker = broker or self._discover_broker()
        self.port = port
        self._client = None
        self._connected = False

    def _discover_broker(self) -> str:
        """Auto-detect MQTT broker on LAN (looks for Home Assistant)."""
        import urllib.request
        subnet = self._local_subnet()
        for i in [1, 2, 10, 20, 100, 200]:
            ip = f"{subnet}{i}"
            try:
                urllib.request.urlopen(f"http://{ip}:8123", timeout=0.5)  # noqa: S310
                log.info("Home Assistant detected at %s", ip)
                return ip
            except Exception:
                continue
        log.warning("HA broker not auto-detected, defaulting to 192.168.1.1")
        return "192.168.1.1"

    def _local_subnet(self) -> str:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("10.255.255.255", 1))
            ip = s.getsockname()[0]
            s.close()
            return ".".join(ip.split(".")[:3]) + "."
        except Exception:
            return "192.168.1."

    def _connect(self):
        try:
            import paho.mqtt.client as mqtt  # noqa: PLC0415
        except ImportError:
            raise RuntimeError("pip install paho-mqtt")

        self._client = mqtt.Client(client_id=f"echowall_{socket.gethostname()}")
        self._client.on_connect = lambda c, u, f, rc: setattr(self, "_connected", rc == 0)
        self._client.connect(self.broker, self.port, keepalive=60)
        self._client.loop_start()
        time.sleep(1)
        if self._connected:
            self._publish_discovery()
            log.info("EchoWall connected to HA MQTT at %s:%d", self.broker, self.port)
        else:
            log.warning("MQTT connection failed — HA integration disabled")

    def _publish_discovery(self):
        """Publish MQTT discovery messages — makes EchoWall appear in HA automatically."""
        device = _device_info()
        state_topic = f"echowall/{_NODE_ID}/state"

        for entity in self.ENTITIES:
            uid = f"echowall_{socket.gethostname()}_{entity['id']}"
            component = entity["component"]
            discovery_topic = f"{_DISCOVERY_PREFIX}/{component}/{_NODE_ID}/{entity['id']}/config"

            payload = {
                "name": entity["name"],
                "unique_id": uid,
                "state_topic": state_topic,
                "value_template": f"{{{{ value_json.{entity['value_key']} }}}}",
                "device": device,
                "availability_topic": f"echowall/{_NODE_ID}/availability",
            }
            if "unit" in entity:
                payload["unit_of_measurement"] = entity["unit"]
            if "device_class" in entity:
                payload["device_class"] = entity["device_class"]
            if component == "binary_sensor":
                payload["payload_on"] = "True"
                payload["payload_off"] = "False"

            self._client.publish(discovery_topic, json.dumps(payload), retain=True)

        # Online availability
        self._client.publish(f"echowall/{_NODE_ID}/availability", "online", retain=True)
        log.info("HA discovery published — EchoWall now visible in Home Assistant")

    def publish(self, result) -> None:
        """Publish a PresenceResult to HA."""
        if not self._connected or not self._client:
            return
        state_topic = f"echowall/{_NODE_ID}/state"
        payload = {
            "presence": str(result.presence),
            "count": result.count,
            "posture": result.posture,
            "confidence": round(result.confidence * 100, 1),
            "breathing_rate": result.breathing_rate,
            "heart_rate": result.heart_rate,
        }
        self._client.publish(state_topic, json.dumps(payload))

    def start(self, pipeline) -> None:
        """Start publishing in background thread."""
        self._connect()
        if not self._connected:
            log.warning("MQTT unavailable — use REST fallback (see INTEGRATIONS.md)")
            return
        threading.Thread(target=self._publish_loop, args=[pipeline], daemon=True).start()

    def _publish_loop(self, pipeline) -> None:
        while True:
            result = pipeline.get_result()
            if result:
                self.publish(result)
            time.sleep(1.0)
