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
            "[bold cyan]ECHOWALL v0.2.0[/bold cyan]\n"
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


@app.command(name="ha-setup")
def ha_setup(
    broker: str = typer.Option(
        ...,
        prompt="Local MQTT broker IP (e.g. 192.168.1.10)",
        help="IP address of your local Mosquitto broker.",
    ),
    port: int = typer.Option(1883, help="MQTT broker port."),
    sensor_mode: str = typer.Option(
        "auto", help="Sensor mode for the pipeline: rpi | esp32 | sim | auto"
    ),
    poll: float = typer.Option(1.0, help="State publish interval in seconds."),
):
    """Connect EchoWall to Home Assistant via local MQTT Discovery.

    Zero YAML. Zero HA restarts. Requires a local Mosquitto broker —
    never a cloud broker. HA discovers EchoWall automatically.

    Example::

        echowall ha-setup --broker 192.168.1.10

    EchoWall will appear in HA under Settings → Devices & Services → EchoWall.
    """
    _banner()
    rprint(f"[cyan]📡 Connecting to Home Assistant MQTT broker at {broker}:{port}...[/cyan]")

    try:
        from echowall.integrations.homeassistant import HassPublisher
    except ImportError as exc:
        rprint(f"[red]❌ Import error: {exc}[/red]")
        raise typer.Exit(code=1)

    try:
        from echowall.core.pipeline import EchowallPipeline
    except ImportError as exc:
        rprint(f"[red]❌ Pipeline import error: {exc}[/red]")
        raise typer.Exit(code=1)

    pipeline = EchowallPipeline(mode=sensor_mode)

    publisher = HassPublisher(broker=broker, port=port, poll_interval=poll)
    try:
        publisher.start(pipeline)
    except Exception as exc:
        rprint(f"[red]❌ Failed to connect to MQTT broker at {broker}:{port}: {exc}[/red]")
        rprint("[yellow]Hint: ensure Mosquitto is running and reachable on your LAN.[/yellow]")
        raise typer.Exit(code=1)

    pipeline.start()  # blocking — Ctrl+C to stop

    rprint("[green]✅ EchoWall → Home Assistant publishing started.[/green]")
    rprint("[dim]HA: Settings → Devices & Services → EchoWall[/dim]")

    import signal
    import sys

    def _shutdown(sig, frame):
        rprint("\n[yellow]Shutting down — removing HA entities cleanly...[/yellow]")
        publisher.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    # Block until signal.
    import time
    while True:
        time.sleep(1)


if __name__ == "__main__":
    app()
