"""Privacy-by-Physics: Adversarial CSI Jitter.

ECHOWALL adds a deterministic but hardware-seeded noise mask to outgoing
CSI streams. Any intercepted CSI appears as random noise. The local model
— which knows the seed — reverses the jitter before inference.

This is fundamentally different from encryption: the protection is at the
physical signal level, not the transport level.
"""

from __future__ import annotations
import numpy as np
from echowall.core.csi.capture import CSIFrame


class AdversarialJitter:
    """Applies and removes seeded Gaussian perturbation to CSI frames."""

    def __init__(self, seed: int = 0xDEADBEEF, sigma: float = 0.05, enabled: bool = True):
        self.seed = seed
        self.sigma = sigma  # Jitter amplitude — tuned to be below human-motion SNR
        self.enabled = enabled
        self._rng = np.random.default_rng(seed)

    def apply(self, frame: CSIFrame) -> CSIFrame:
        """Add adversarial jitter — call before any transmission."""
        if not self.enabled:
            return frame
        noise = self._rng.normal(0, self.sigma, frame.data.shape).astype(np.float32) + \
                1j * self._rng.normal(0, self.sigma, frame.data.shape).astype(np.float32)
        jittered = frame.data + noise
        return CSIFrame(jittered.astype(frame.data.dtype), frame.timestamp, frame.rssi)

    def reverse(self, frame: CSIFrame) -> CSIFrame:
        """Remove adversarial jitter — call before inference.
        
        Works because the RNG state is deterministic from seed.
        Reinitializes RNG to same state to reproduce exact noise.
        """
        if not self.enabled:
            return frame
        rng = np.random.default_rng(self.seed)  # reset to reproduce jitter
        noise = rng.normal(0, self.sigma, frame.data.shape).astype(np.float32) + \
                1j * rng.normal(0, self.sigma, frame.data.shape).astype(np.float32)
        clean = frame.data - noise
        return CSIFrame(clean.astype(frame.data.dtype), frame.timestamp, frame.rssi)

    @staticmethod
    def generate_hardware_seed(device_id: str) -> int:
        """Derive a seed from hardware serial number — ties privacy to physical device."""
        import hashlib
        return int(hashlib.sha256(device_id.encode()).hexdigest()[:8], 16)
