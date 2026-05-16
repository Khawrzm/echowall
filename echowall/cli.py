"""ECHOWALL CLI — Command line interface."""

import typer
from rich.console import Console
from rich.panel import Panel
from rich import print as rprint
from typing import Optional

app = typer.Typer(
    name="echowall",
    help="🔍 ECHOWALL — See through walls with the Wi-Fi you already own.",
    add_completion=False,
)
console = Console()


def _banner():
    console.print(
        Panel.fit(
            "[bold cyan]ECHOWALL v0.1.0[/bold cyan]\n"
            "[dim]See through walls. No cameras. No cloud.[/dim]",
            border_style="cyan",
        )
    )


@app.command()
def run(
    mode: str = typer.Option("auto", help="Sensor mode: rpi | esp32 | sim | auto"),
    simulate: bool = typer.Option(False, "--simulate", help="Run in simulation mode"),
    scene: str = typer.Option("empty", help="Simulation scene: empty | living_room | office"),
    host: str = typer.Option("0.0.0.0", help="API server host"),
    port: int = typer.Option(8765, help="API server port"),
):
    """Start the ECHOWALL sensing pipeline and API server."""
    _banner()
    if simulate:
        rprint(f"[yellow]⚡ Simulation mode — scene: {scene}[/yellow]")
        from echowall.sim.environment import SimEnvironment
        env = SimEnvironment(scene=scene)
        env.run()
    else:
        rprint(f"[green]🚀 Starting ECHOWALL in [{mode}] mode on {host}:{port}[/green]")
        from echowall.core.pipeline import EchowallPipeline
        pipeline = EchowallPipeline(mode=mode)
        pipeline.start(host=host, port=port)


@app.command()
def init(
    mode: str = typer.Option("rpi", help="Platform: rpi | esp32 | linux"),
    output: str = typer.Option("echowall.config.json", help="Config output path"),
):
    """Initialize ECHOWALL configuration for your platform."""
    _banner()
    from echowall.config import generate_default_config
    cfg = generate_default_config(mode=mode)
    import json
    with open(output, "w") as f:
        json.dump(cfg, f, indent=2)
    rprint(f"[green]✅ Config written to {output}[/green]")


@app.command()
def calibrate(
    duration: int = typer.Option(30, help="Calibration duration in seconds"),
):
    """Calibrate the environment baseline (empty room required)."""
    _banner()
    rprint(f"[cyan]📡 Calibrating for {duration}s — keep the space empty...[/cyan]")
    from echowall.core.calibration import run_calibration
    run_calibration(duration=duration)
    rprint("[green]✅ Calibration complete. Baseline saved.[/green]")


if __name__ == "__main__":
    app()
