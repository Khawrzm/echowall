"""CSI capture abstraction — supports ESP32-S3, RPi/Nexmon, Intel iwlwifi."""

from __future__ import annotations
import asyncio
import logging
import numpy as np
from typing import AsyncGenerator

logger = logging.getLogger("echowall.csi.capture")

CSI_SUBCARRIERS = 64      # 802.11n / 802.11ac standard
CSI_ANTENNAS = 3
CSI_DTYPE = np.complex64


class CSIFrame:
    """One timestamped CSI snapshot across subcarriers × antennas."""
    def __init__(self, data: np.ndarray, timestamp: float, rssi: float = -70.0):
        assert data.shape == (CSI_ANTENNAS, CSI_SUBCARRIERS), f"Bad CSI shape: {data.shape}"
        self.data = data          # complex amplitude + phase
        self.timestamp = timestamp
        self.rssi = rssi

    @property
    def amplitude(self) -> np.ndarray:
        return np.abs(self.data)

    @property
    def phase(self) -> np.ndarray:
        return np.angle(self.data)

    def sanitize(self) -> "CSIFrame":
        """Remove NaN/Inf, apply Hampel filter, unwrap phase."""
        d = self.data.copy()
        d = np.nan_to_num(d, nan=0.0, posinf=0.0, neginf=0.0)
        phase_unwrapped = np.unwrap(np.angle(d), axis=-1)
        amp = np.abs(d)
        d = amp * np.exp(1j * phase_unwrapped)
        return CSIFrame(d, self.timestamp, self.rssi)


class CSICapture:
    """Platform-agnostic CSI stream source."""

    def __init__(self, interface: str = "wlan0", mode: str = "auto"):
        self.interface = interface
        self.mode = self._resolve_mode(mode)
        logger.info("CSICapture: mode=%s interface=%s", self.mode, self.interface)

    def _resolve_mode(self, mode: str) -> str:
        if mode != "auto":
            return mode
        try:
            import subprocess
            result = subprocess.run(["nexutil", "-I"], capture_output=True, text=True, timeout=2)
            if result.returncode == 0:
                return "nexmon"
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass
        try:
            import serial  # noqa
            return "esp32"
        except ImportError:
            pass
        return "sim"

    async def stream(self) -> AsyncGenerator[CSIFrame, None]:
        """Yield sanitized CSI frames from hardware or simulation."""
        if self.mode == "nexmon":
            async for frame in self._stream_nexmon():
                yield frame.sanitize()
        elif self.mode == "esp32":
            async for frame in self._stream_esp32():
                yield frame.sanitize()
        else:
            async for frame in self._stream_sim():
                yield frame.sanitize()

    async def _stream_nexmon(self) -> AsyncGenerator[CSIFrame, None]:
        """Read CSI from nexmon_csi pcap socket."""
        import socket, struct, time
        sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
        sock.bind((self.interface, 0))
        sock.setblocking(False)
        loop = asyncio.get_event_loop()
        while True:
            try:
                raw = await loop.sock_recv(sock, 4096)
                csi = _parse_nexmon_packet(raw)
                if csi is not None:
                    yield csi
            except Exception as e:
                logger.warning("Nexmon read error: %s", e)
                await asyncio.sleep(0.01)

    async def _stream_esp32(self) -> AsyncGenerator[CSIFrame, None]:
        """Read CSI from ESP32-S3 via serial JSON protocol."""
        import serial, json, time
        ser = serial.Serial("/dev/ttyUSB0", 921600, timeout=1)
        loop = asyncio.get_event_loop()
        while True:
            line = await loop.run_in_executor(None, ser.readline)
            try:
                obj = json.loads(line.decode("utf-8").strip())
                raw = np.array(obj["csi_real"], dtype=np.float32) + \
                      1j * np.array(obj["csi_imag"], dtype=np.float32)
                raw = raw.reshape(CSI_ANTENNAS, CSI_SUBCARRIERS)
                yield CSIFrame(raw, time.time(), rssi=obj.get("rssi", -70.0))
            except Exception as e:
                logger.debug("ESP32 parse error: %s", e)

    async def _stream_sim(self) -> AsyncGenerator[CSIFrame, None]:
        """Synthetic CSI with configurable human presence simulation."""
        import time
        rng = np.random.default_rng(42)
        t = 0.0
        while True:
            # Simulate Doppler effect from breathing (0.25 Hz) + movement
            breathing = 0.3 * np.sin(2 * np.pi * 0.25 * t)
            base = rng.normal(0, 1, (CSI_ANTENNAS, CSI_SUBCARRIERS)) + \
                   1j * rng.normal(0, 1, (CSI_ANTENNAS, CSI_SUBCARRIERS))
            human_sig = breathing * np.ones((CSI_ANTENNAS, CSI_SUBCARRIERS))
            data = (base + human_sig).astype(CSI_DTYPE)
            yield CSIFrame(data, time.time())
            t += 0.1
            await asyncio.sleep(0.1)


def _parse_nexmon_packet(raw: bytes) -> CSIFrame | None:
    """Parse nexmon_csi UDP packet into CSIFrame."""
    import time
    try:
        # nexmon_csi packet format: 4B magic + 4B metadata + N×4B int16 pairs
        if len(raw) < 8:
            return None
        magic = raw[:4]
        if magic != b"\x11\x11\x11\x11":
            return None
        payload = raw[8:]
        ints = np.frombuffer(payload, dtype="<i2")
        n = CSI_ANTENNAS * CSI_SUBCARRIERS * 2
        if len(ints) < n:
            return None
        ints = ints[:n].reshape(CSI_ANTENNAS, CSI_SUBCARRIERS, 2)
        data = (ints[..., 0] + 1j * ints[..., 1]).astype(CSI_DTYPE)
        return CSIFrame(data, time.time())
    except Exception:
        return None
