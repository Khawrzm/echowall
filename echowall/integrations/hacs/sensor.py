"""EchoWall sensors for Home Assistant."""

from __future__ import annotations
import urllib.request
import json
from homeassistant.components.sensor import SensorEntity
from homeassistant.components.binary_sensor import BinarySensorEntity
from homeassistant.const import CONF_HOST, CONF_PORT

DOMAIN = "echowall"


async def async_setup_entry(hass, entry, async_add_entities):
    host = entry.data[CONF_HOST]
    port = entry.data.get(CONF_PORT, 8765)
    base = f"http://{host}:{port}"
    async_add_entities([
        EchoWallPresenceSensor(base),
        EchoWallCountSensor(base),
        EchoWallPostureSensor(base),
        EchoWallBreathingSensor(base),
    ], update_before_add=True)


def _fetch(base: str) -> dict:
    try:
        with urllib.request.urlopen(f"{base}/presence", timeout=3) as r:  # noqa: S310
            return json.loads(r.read())
    except Exception:
        return {}


class EchoWallPresenceSensor(BinarySensorEntity):
    _attr_name = "EchoWall Presence"
    _attr_device_class = "occupancy"
    _attr_unique_id = "echowall_presence"

    def __init__(self, base): self._base = base; self._state = False
    @property
    def is_on(self): return self._state
    def update(self): self._state = _fetch(self._base).get("presence", False)


class EchoWallCountSensor(SensorEntity):
    _attr_name = "EchoWall Count"
    _attr_unit_of_measurement = "people"
    _attr_unique_id = "echowall_count"

    def __init__(self, base): self._base = base; self._state = 0
    @property
    def state(self): return self._state
    def update(self): self._state = _fetch(self._base).get("count", 0)


class EchoWallPostureSensor(SensorEntity):
    _attr_name = "EchoWall Posture"
    _attr_unique_id = "echowall_posture"

    def __init__(self, base): self._base = base; self._state = "unknown"
    @property
    def state(self): return self._state
    def update(self): self._state = _fetch(self._base).get("posture", "unknown")


class EchoWallBreathingSensor(SensorEntity):
    _attr_name = "EchoWall Breathing Rate"
    _attr_unit_of_measurement = "bpm"
    _attr_unique_id = "echowall_breathing"

    def __init__(self, base): self._base = base; self._state = None
    @property
    def state(self): return self._state
    def update(self): self._state = _fetch(self._base).get("breathing_rate")
