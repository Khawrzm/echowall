"""Unit tests for CSI capture and processing."""
import numpy as np
import pytest
from echowall.core.csi.capture import CSIFrame, CSI_ANTENNAS, CSI_SUBCARRIERS
from echowall.privacy.jitter import AdversarialJitter


def make_frame(seed: int = 0) -> CSIFrame:
    rng = np.random.default_rng(seed)
    data = (rng.normal(size=(CSI_ANTENNAS, CSI_SUBCARRIERS)) +
            1j * rng.normal(size=(CSI_ANTENNAS, CSI_SUBCARRIERS))).astype(np.complex64)
    return CSIFrame(data, timestamp=0.0)


def test_frame_shape():
    f = make_frame()
    assert f.amplitude.shape == (CSI_ANTENNAS, CSI_SUBCARRIERS)
    assert f.phase.shape == (CSI_ANTENNAS, CSI_SUBCARRIERS)


def test_sanitize_removes_nan():
    f = make_frame()
    f.data[0, 0] = np.nan + 1j * np.nan
    clean = f.sanitize()
    assert not np.any(np.isnan(clean.data))


def test_jitter_reversible():
    """Applying then reversing jitter must recover the original within tolerance."""
    jitter = AdversarialJitter(seed=42, sigma=0.05)
    original = make_frame(7)
    jittered = jitter.apply(original)
    recovered = jitter.reverse(jittered)
    diff = np.abs(recovered.data - original.data)
    assert diff.max() < 1e-5, f"Jitter reversal error too large: {diff.max()}"


def test_jitter_disabled():
    jitter = AdversarialJitter(enabled=False)
    f = make_frame()
    jittered = jitter.apply(f)
    np.testing.assert_array_equal(f.data, jittered.data)


def test_hardware_seed_deterministic():
    s1 = AdversarialJitter.generate_hardware_seed("ESP32-001")
    s2 = AdversarialJitter.generate_hardware_seed("ESP32-001")
    assert s1 == s2
    s3 = AdversarialJitter.generate_hardware_seed("ESP32-002")
    assert s1 != s3
