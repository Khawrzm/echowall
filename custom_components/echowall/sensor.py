"""EchoWall HA sensor — async REST polling sensor for HA setups without MQTT.

Uses HA's async_add_executor_job to wrap the blocking urllib call,
preventing event-loop freezes on every 2-second poll cycle.
"""
from __future__ import annotations

import logging
import urllib.request
import urllib.error
import json
from typing import Any

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

log = logging.getLogger(__name__)

DEFAULT_PORT = 8765
_ENDPOINT = "/ha-discovery"
_SCAN_INTERVAL_SECONDS = 2


def _fetch_blocking(base_url: str) -> dict:
    """Blocking urllib fetch — must only be called via async_add_executor_job."""
    url = f"{base_url.rstrip('/')}{_ENDPOINT}"
    try:
        with urllib.request.urlopen(url, timeout=3) as resp:
            return json.loads(resp.read().decode())
    except (urllib.error.URLError, json.JSONDecodeError, OSError) as exc:
        log.warning("EchoWall sensor: fetch failed (%s): %s", url, exc)
        return {}


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up EchoWall sensors from a config entry."""
    host = config_entry.data["host"]
    port = config_entry.data.get("port", DEFAULT_PORT)
    base_url = f"http://{host}:{port}"

    async_add_entities(
        [
            EchoWallPresenceSensor(hass, base_url),
            EchoWallCountSensor(hass, base_url),
            EchoWallPostureSensor(hass, base_url),
        ],
        update_before_add=True,
    )


class _EchoWallBaseSensor(SensorEntity):
    """Base class: handles async polling without blocking the HA event loop."""

    _attr_should_poll = True

    def __init__(self, hass: HomeAssistant, base_url: str) -> None:
        self.hass = hass
        self._base_url = base_url
        self._data: dict = {}

    async def async_update(self) -> None:
        """Fetch latest state via executor to avoid blocking the event loop.

        urllib.request.urlopen is synchronous (blocking I/O). Calling it
        directly inside an async method would stall the entire HA event loop
        for the duration of the network round-trip. async_add_executor_job
        runs the blocking call in a thread-pool worker instead.
        """
        self._data = await self.hass.async_add_executor_job(
            _fetch_blocking, self._base_url
        )

    @property
    def _entities(self) -> dict:
        return self._data.get("entities", {})

    @property
    def available(self) -> bool:
        return bool(self._data)


class EchoWallPresenceSensor(_EchoWallBaseSensor):
    _attr_name = "EchoWall Presence"
    _attr_unique_id = "echowall_presence"
    _attr_icon = "mdi:motion-sensor"

    @property
    def state(self) -> str:
        return "on" if self._entities.get("presence", False) else "off"


class EchoWallCountSensor(_EchoWallBaseSensor):
    _attr_name = "EchoWall Count"
    _attr_unique_id = "echowall_count"
    _attr_icon = "mdi:account-group"
    _attr_unit_of_measurement = "people"

    @property
    def state(self) -> Any:
        return self._entities.get("count", 0)


class EchoWallPostureSensor(_EchoWallBaseSensor):
    _attr_name = "EchoWall Posture"
    _attr_unique_id = "echowall_posture"
    _attr_icon = "mdi:human"

    @property
    def state(self) -> str:
        return self._entities.get("posture", "unknown")
