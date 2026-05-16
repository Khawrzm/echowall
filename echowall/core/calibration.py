"""Environment baseline calibration."""

from __future__ import annotations
import asyncio
import json
import logging
import time
import numpy as np
from pathlib import Path

logger = logging.getLogger("echowall.calibration")

CALIBRATION_FILE = Path("echowall_baseline.json")


def run_calibration(duration: int = 30):
    """Collect N seconds of empty-room CSI to establish baseline."""
    asyncio.run(_calibrate(duration))


async def _calibrate(duration: int):
    from echowall.core.csi.capture import CSICapture
    capture = CSICapture()
    frames = []
    deadline = time.time() + duration
    logger.info("Collecting baseline for %ds...", duration)
    async for frame in capture.stream():
        frames.append(frame.amplitude)
        if time.time() > deadline:
            break

    if not frames:
        logger.error("No frames collected.")
        return

    stack = np.stack(frames)
    baseline = {
        "mean": stack.mean(axis=0).tolist(),
        "std": stack.std(axis=0).tolist(),
        "n_frames": len(frames),
        "timestamp": time.time(),
    }
    CALIBRATION_FILE.write_text(json.dumps(baseline, indent=2))
    logger.info("Baseline saved to %s (%d frames)", CALIBRATION_FILE, len(frames))
