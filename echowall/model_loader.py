"""Offline-first model loader — no cloud dependency after first run.

Philosophy: download once, run forever. No internet required after initial pull.
Models are stored locally in ~/.echowall/models/ and verified via SHA-256.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import urllib.request
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Registry — pure static manifest, no API calls, no cloud SDK
# ---------------------------------------------------------------------------
_REGISTRY: dict[str, dict] = {
    "echonet-v1": {
        "filename": "echonet_v1.pt",
        "url": "https://github.com/Khawrzm/echowall/releases/download/v0.1.0/echonet_v1.pt",
        "sha256": None,  # set after first official release tag
        "size_mb": 12,
        "description": "Presence + count transformer — runs fully offline on Pi 4 / x86",
    },
    "echonet-v1-lite": {
        "filename": "echonet_v1_lite.pt",
        "url": "https://github.com/Khawrzm/echowall/releases/download/v0.1.0/echonet_v1_lite.pt",
        "sha256": None,
        "size_mb": 4,
        "description": "Quantised INT8 — for ESP32-S3 / low-RAM edge devices",
    },
}

_CACHE_DIR = Path.home() / ".echowall" / "models"


def _verify_sha256(path: Path, expected: Optional[str]) -> bool:
    """Return True if expected is None (skip check) or hash matches."""
    if expected is None:
        return True
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest() == expected


def _download(url: str, dest: Path) -> None:
    """Plain urllib download — zero external dependencies."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(".tmp")
    try:
        with urllib.request.urlopen(url, timeout=30) as resp, open(tmp, "wb") as out:  # noqa: S310
            shutil.copyfileobj(resp, out)
        tmp.rename(dest)
    except Exception as exc:
        if tmp.exists():
            tmp.unlink()
        raise RuntimeError(
            f"Model download failed: {exc}\n"
            "Place the model file manually in ~/.echowall/models/ to run offline."
        ) from exc


def get_model_path(name: str = "echonet-v1") -> Path:
    """Return local path to model, downloading once if needed.

    After the first call the device is 100% offline-capable.
    """
    if name not in _REGISTRY:
        raise ValueError(f"Unknown model '{name}'. Available: {list(_REGISTRY)}.")

    entry = _REGISTRY[name]
    dest = _CACHE_DIR / entry["filename"]

    if dest.exists() and _verify_sha256(dest, entry["sha256"]):
        return dest

    print(
        f"[echowall] Downloading '{name}' (~{entry['size_mb']} MB) — one-time only.\n"
        f"           After this the device runs fully offline forever."
    )
    _download(entry["url"], dest)

    if not _verify_sha256(dest, entry["sha256"]):
        dest.unlink()
        raise RuntimeError("Model file checksum mismatch — download may be corrupted.")

    # Write a small sidecar so humans can inspect what's cached
    meta = dest.with_suffix(".json")
    meta.write_text(json.dumps({"name": name, **entry}, indent=2))

    return dest


def list_cached() -> list[dict]:
    """Return info about every model already on disk."""
    out = []
    for name, entry in _REGISTRY.items():
        path = _CACHE_DIR / entry["filename"]
        out.append({"name": name, "cached": path.exists(), "path": str(path), **entry})
    return out
