"""RF + Acoustic signal fusion."""

from __future__ import annotations
import numpy as np
from dataclasses import dataclass
from typing import Optional
from echowall.core.csi.capture import CSIFrame


@dataclass
class FusedFrame:
    """Combined RF + acoustic feature tensor ready for EchoNet."""
    csi_amplitude: np.ndarray   # (antennas, subcarriers)
    csi_phase: np.ndarray       # (antennas, subcarriers)
    csi_diff: np.ndarray        # differential CSI (frame-to-frame delta)
    acoustic_range: Optional[np.ndarray]  # range profile from chirp echo
    timestamp: float


class SignalFuser:
    """Fuses Wi-Fi CSI and acoustic chirp data into a unified feature frame."""

    def __init__(self, acoustic_enabled: bool = True, history_len: int = 10):
        self.acoustic_enabled = acoustic_enabled
        self.history_len = history_len
        self._csi_history: list[np.ndarray] = []

    def fuse(self, frame: CSIFrame) -> FusedFrame:
        """Compute fused feature frame from latest CSI + acoustic snapshot."""
        amp = frame.amplitude
        phase = frame.phase

        # Differential CSI: captures Doppler / motion
        if self._csi_history:
            diff = amp - self._csi_history[-1]
        else:
            diff = np.zeros_like(amp)

        self._csi_history.append(amp)
        if len(self._csi_history) > self.history_len:
            self._csi_history.pop(0)

        acoustic_range = None
        if self.acoustic_enabled:
            acoustic_range = self._get_acoustic_range()

        return FusedFrame(
            csi_amplitude=amp,
            csi_phase=phase,
            csi_diff=diff,
            acoustic_range=acoustic_range,
            timestamp=frame.timestamp,
        )

    def _get_acoustic_range(self) -> np.ndarray:
        """FMCW-style range profile via speaker chirp + microphone echo.
        
        Generates a short 20-22kHz linear sweep (inaudible) and cross-correlates
        with the recorded echo to produce a 1D range profile (0-8m, 128 bins).
        Returns zeros if audio hardware unavailable.
        """
        RANGE_BINS = 128
        try:
            import sounddevice as sd
            FS = 44100
            T = 0.02          # 20ms chirp
            f0, f1 = 20000, 22000
            t = np.linspace(0, T, int(FS * T), endpoint=False)
            chirp = np.sin(2 * np.pi * (f0 * t + (f1 - f0) / (2 * T) * t**2)).astype(np.float32)

            # Play and record simultaneously
            recorded = sd.playrec(chirp[:, None], samplerate=FS, channels=1)
            sd.wait()
            echo = recorded[:, 0]

            # Cross-correlate to get range profile
            xc = np.abs(np.fft.ifft(np.fft.fft(chirp, len(echo)) * np.conj(np.fft.fft(echo))))
            range_profile = xc[:RANGE_BINS]
            return range_profile.astype(np.float32)
        except Exception:
            return np.zeros(RANGE_BINS, dtype=np.float32)
