"""Unit tests for signal fusion."""
import numpy as np
import pytest
from echowall.core.csi.capture import CSIFrame, CSI_ANTENNAS, CSI_SUBCARRIERS
from echowall.core.fusion.fuser import SignalFuser


def make_frame(seed=0) -> CSIFrame:
    rng = np.random.default_rng(seed)
    d = (rng.normal(size=(CSI_ANTENNAS, CSI_SUBCARRIERS)) +
         1j * rng.normal(size=(CSI_ANTENNAS, CSI_SUBCARRIERS))).astype(np.complex64)
    return CSIFrame(d, timestamp=float(seed))


def test_fused_shapes():
    fuser = SignalFuser(acoustic_enabled=False)
    f = fuser.fuse(make_frame(0))
    assert f.csi_amplitude.shape == (CSI_ANTENNAS, CSI_SUBCARRIERS)
    assert f.csi_diff.shape == (CSI_ANTENNAS, CSI_SUBCARRIERS)


def test_diff_first_frame_is_zero():
    fuser = SignalFuser(acoustic_enabled=False)
    f = fuser.fuse(make_frame(0))
    assert np.all(f.csi_diff == 0.0)


def test_diff_second_frame_nonzero():
    fuser = SignalFuser(acoustic_enabled=False)
    fuser.fuse(make_frame(0))
    f2 = fuser.fuse(make_frame(99))
    assert not np.all(f2.csi_diff == 0.0)


def test_history_bounded():
    fuser = SignalFuser(acoustic_enabled=False, history_len=3)
    for i in range(10):
        fuser.fuse(make_frame(i))
    assert len(fuser._csi_history) <= 3
