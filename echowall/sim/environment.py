"""ECHOWALL simulation environment."""

from __future__ import annotations
import asyncio
import logging
from rich.console import Console
from rich.live import Live
from rich.table import Table
from rich import box

logger = logging.getLogger("echowall.sim")
console = Console()

SCENES = {
    "empty": {"n_people": 0, "description": "Empty room — calibration"},
    "living_room": {"n_people": 2, "description": "Living room — 2 seated people"},
    "office": {"n_people": 4, "description": "Office — 4 people, mixed postures"},
    "intrusion": {"n_people": 1, "description": "Single intruder — moving"},
    "fallen": {"n_people": 1, "description": "One person fallen on floor"},
}


class SimEnvironment:
    def __init__(self, scene: str = "living_room"):
        self.scene = SCENES.get(scene, SCENES["living_room"])
        self.scene_name = scene

    def run(self):
        asyncio.run(self._run())

    async def _run(self):
        from echowall.core.pipeline import EchowallPipeline, EchowallConfig
        cfg = EchowallConfig(mode="sim", acoustic_enabled=True, api_port=8765)
        pipeline = EchowallPipeline(config=cfg)

        console.print(f"[cyan]🏠 Scene: {self.scene['description']}[/cyan]")
        console.print("[dim]Press Ctrl+C to stop[/dim]\n")

        await asyncio.gather(
            pipeline._capture_loop(),
            self._display_loop(pipeline),
        )

    async def _display_loop(self, pipeline):
        import time
        while True:
            result = pipeline.get_result()
            if result:
                table = Table(box=box.ROUNDED, title="ECHOWALL Live", title_style="bold cyan")
                table.add_column("Metric", style="dim")
                table.add_column("Value", style="bold")
                table.add_row("Presence", "✅ YES" if result.presence else "❌ NO")
                table.add_row("Count", str(result.count))
                table.add_row("Posture", result.posture.upper())
                table.add_row("Confidence", f"{result.confidence:.1%}")
                table.add_row("Breathing", f"{result.breathing_rate} bpm" if result.breathing_rate else "N/A")
                table.add_row("Heart Rate", f"{result.heart_rate} bpm" if result.heart_rate else "N/A")
                table.add_row("Timestamp", f"{result.timestamp:.2f}")
                console.clear()
                console.print(table)
            await asyncio.sleep(1.0)
