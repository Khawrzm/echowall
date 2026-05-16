"""Configuration helpers."""

from __future__ import annotations
from typing import Any


def generate_default_config(mode: str = "rpi") -> dict[str, Any]:
    return {
        "echowall_version": "0.1.0",
        "mode": mode,
        "csi": {
            "interface": "wlan0",
            "channel": 6,
            "bandwidth": 20,
            "antennas": 3,
        },
        "acoustic": {
            "enabled": True,
            "freq_start_hz": 20000,
            "freq_end_hz": 22000,
            "chirp_duration_ms": 20,
            "sample_rate": 44100,
        },
        "privacy": {
            "adversarial_jitter": True,
            "jitter_sigma": 0.05,
            "federated_learning": False,
        },
        "inference": {
            "device": "cpu",
            "window_size": 10,
            "weights_path": None,
        },
        "api": {
            "host": "0.0.0.0",
            "port": 8765,
            "mqtt_broker": None,
        },
    }
