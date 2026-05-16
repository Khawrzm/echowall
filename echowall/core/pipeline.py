"""Main ECHOWALL sensing pipeline."""

from __future__ import annotations
import asyncio
import logging
from dataclasses import dataclass, field
from typing import Optional

logger = logging.getLogger("echowall.pipeline")


@dataclass
class EchowallConfig:
    mode: str = "auto"  # rpi | esp32 | sim | auto
    csi_interface: str = "wlan0"
    acoustic_enabled: bool = True
    federated_enabled: bool = False
    privacy_jitter: bool = True
    jitter_seed: int = 0xDEADBEEF
    inference_device: str = "cpu"  # cpu | cuda | mps
    api_host: str = "0.0.0.0"
    api_port: int = 8765
    mqtt_broker: Optional[str] = None


@dataclass
class PresenceResult:
    """Semantic output — never exposes raw CSI."""
    presence: bool = False
    count: int = 0
    posture: str = "unknown"  # standing | sitting | fallen | unknown
    breathing_rate: Optional[float] = None  # breaths per minute
    heart_rate: Optional[float] = None       # BPM
    confidence: float = 0.0
    timestamp: float = 0.0
    zone_map: dict = field(default_factory=dict)


class EchowallPipeline:
    """Orchestrates CSI capture, acoustic fusion, inference, and API serving."""

    def __init__(self, mode: str = "auto", config: Optional[EchowallConfig] = None):
        self.config = config or EchowallConfig(mode=mode)
        self._running = False
        self._latest: Optional[PresenceResult] = None
        logger.info("ECHOWALL Pipeline initialized in [%s] mode", mode)

    def start(self, host: str = "0.0.0.0", port: int = 8765):
        """Start pipeline (blocking)."""
        self.config.api_host = host
        self.config.api_port = port
        asyncio.run(self._run())

    async def _run(self):
        self._running = True
        logger.info("Pipeline starting...")
        try:
            await asyncio.gather(
                self._capture_loop(),
                self._serve_api(),
            )
        except KeyboardInterrupt:
            logger.info("Pipeline stopped.")
        finally:
            self._running = False

    async def _capture_loop(self):
        """Main sensing loop: capture → fuse → infer → publish."""
        from echowall.core.csi.capture import CSICapture
        from echowall.core.fusion.fuser import SignalFuser
        from echowall.models.echonet.model import EchoNet
        from echowall.privacy.jitter import AdversarialJitter

        capture = CSICapture(interface=self.config.csi_interface, mode=self.config.mode)
        fuser = SignalFuser(acoustic_enabled=self.config.acoustic_enabled)
        jitter = AdversarialJitter(seed=self.config.jitter_seed, enabled=self.config.privacy_jitter)
        model = EchoNet(device=self.config.inference_device)
        model.load_pretrained()

        logger.info("Sensing loop started.")
        async for frame in capture.stream():
            clean = jitter.reverse(frame)    # undo privacy jitter
            fused = fuser.fuse(clean)         # acoustic + RF fusion
            result = model.infer(fused)       # run EchoNet
            self._latest = result
            logger.debug("Result: presence=%s count=%d", result.presence, result.count)

    async def _serve_api(self):
        """Launch FastAPI server."""
        import uvicorn
        from echowall.api.server import build_app
        api_app = build_app(pipeline=self)
        config = uvicorn.Config(
            api_app,
            host=self.config.api_host,
            port=self.config.api_port,
            log_level="warning",
        )
        server = uvicorn.Server(config)
        await server.serve()

    def get_result(self) -> Optional[PresenceResult]:
        return self._latest
