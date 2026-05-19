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
    scene: str = typer.Option("empty", help="Simulation scene: empty | living_room | office | apartment_2br"),
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


@app.command(name="download-weights")
def download_weights(
    model: str = typer.Option(
        "echonet-v1",
        help="Model name to download. Currently only 'echonet-v1' is available.",
    ),
    force: bool = typer.Option(
        False,
        "--force",
        help="Force re-download even if the model is already cached.",
    ),
):
    """Download (or seed offline) the EchoNet model weights.

    On first run, attempts to fetch the checkpoint from GitHub Releases.
    If the network is unavailable, falls back to a physics-informed local
    seed (deterministic, numpy seed=42) that produces non-trivial results
    immediately — no internet required.

    The model is cached at ``~/.echowall/models/`` and integrity-verified
    via SHA-256 on every subsequent load.  100% offline after first run.

    Example::

        echowall download-weights
        echowall download-weights --model echonet-v1 --force
    """
    _banner()
    from echowall.model_loader import get_model_path, _CACHE_DIR
    from pathlib import Path
    import json

    dest = _CACHE_DIR / f"{model}.json"

    if force and dest.exists():
        rprint(f"[yellow]🗑  Force flag set — removing cached model at {dest}[/yellow]")
        dest.unlink(missing_ok=True)
        meta = dest.with_suffix(".meta.json")
        meta.unlink(missing_ok=True)

    rprint(f"[cyan]🔽 Fetching model: [bold]{model}[/bold][/cyan]")
    rprint(f"[dim]   Cache location : {_CACHE_DIR}[/dim]")
    rprint("[dim]   Strategy       : GitHub Releases → local seed fallback[/dim]")
    rprint("[dim]   Privacy        : Zero network calls after first run[/dim]")

    try:
        path = get_model_path(model)
    except KeyError as exc:
        rprint(f"[red]❌ Unknown model: {exc}[/red]")
        raise typer.Exit(code=1)

    # Read sidecar meta for reporting
    meta_path = path.with_suffix(".meta.json")
    source = "unknown"
    sha256 = "—"
    if meta_path.exists():
        try:
            meta = json.loads(meta_path.read_text())
            source = meta.get("source_url", "local-seed")
            sha256 = meta.get("sha256", "—")[:16] + "…"
        except Exception:
            pass

    seeded = source == "local-seed"

    console.print(
        Panel(
            f"[bold green]✅ Model ready: {model}[/bold green]\n\n"
            f"  Path   : {path}\n"
            f"  Source : {'[yellow]Local offline seed (physics-informed, numpy seed=42)[/yellow]' if seeded else f'[cyan]{source}[/cyan]'}\n"
            f"  SHA-256: [dim]{sha256}[/dim]\n\n"
            + (
                "[yellow]⚠️  Using offline seed — not real trained weights.\n"
                "   Run [cyan]echowall calibrate[/cyan] to adapt to your environment.[/yellow]"
                if seeded else
                "[green]✅ Verified GitHub Release checkpoint.[/green]"
            ),
            title="[bold]EchoNet Weights[/bold]",
            border_style="green" if not seeded else "yellow",
        )
    )


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

    import time
    while True:
        time.sleep(1)


@app.command()
def benchmark(
    dataset: str = typer.Option(
        "",
        help=(
            "Path to a CSI replay CSV. "
            "Defaults to the bundled deterministic simulation dataset "
            "at tests/data/sample_csi_fall.csv."
        ),
    ),
    real: bool = typer.Option(
        False,
        "--real",
        help="Assert that the dataset is from real ESP32-S3 hardware (suppresses simulation warning).",
    ),
):
    """Run the offline EchoWall benchmark against a CSI replay dataset.

    By default uses the bundled deterministic synthetic dataset
    (``tests/data/sample_csi_fall.csv``, numpy seed=42). Pass
    ``--dataset /path/to/real.csv`` and ``--real`` to evaluate on
    hardware-captured CSI.

    Output metrics: accuracy, per-class F1, and confusion matrix.
    All computation is 100\\% offline — no network calls during inference.
    """
    _banner()

    if not real:
        console.print(
            Panel(
                "[bold yellow]⚠️  SIMULATION REPLAY WARNING[/bold yellow]\n\n"
                "Executing offline benchmark using [bold]deterministic synthetic CSI data[/bold]\n"
                "(numpy seed=42 simulation replay, [bold]NOT real RF measurements[/bold]).\n\n"
                "Metrics reflect model behaviour on physics-informed synthetic data only.\n"
                "Run on physical ESP32-S3 hardware for real RF metrics:\n"
                "  [cyan]echowall benchmark --dataset /path/to/real_csi.csv --real[/cyan]",
                border_style="yellow",
                title="[yellow]Benchmark Data Source[/yellow]",
            )
        )

    import pathlib

    if dataset:
        data_path = pathlib.Path(dataset)
        if not data_path.exists():
            rprint(f"[red]❌ Dataset not found: {data_path}[/red]")
            raise typer.Exit(code=1)
    else:
        _pkg_root = pathlib.Path(__file__).parent.parent
        data_path = _pkg_root / "tests" / "data" / "sample_csi_fall.csv"
        if not data_path.exists():
            rprint(
                "[red]❌ Bundled dataset not found at tests/data/sample_csi_fall.csv.\n"
                "Re-clone the repository or pass --dataset /path/to/csi.csv[/red]"
            )
            raise typer.Exit(code=1)

    rprint(f"[cyan]📊 Loading dataset: {data_path}[/cyan]")

    import csv
    import hashlib

    labels_true: list[int] = []
    features: list[list[int]] = []

    with data_path.open(newline="") as fh:
        reader = csv.reader(fh)
        next(reader)  # skip header row
        for row in reader:
            if not row:
                continue
            labels_true.append(int(row[0]))
            features.append([int(v) for v in row[1:]])

    n_samples = len(labels_true)
    n_features = len(features[0]) if features else 0
    sha256_hex = hashlib.sha256(data_path.read_bytes()).hexdigest()

    rprint(f"  Samples  : [bold]{n_samples}[/bold]")
    rprint(f"  Features : [bold]{n_features}[/bold]")
    rprint(f"  SHA-256  : [dim]{sha256_hex}[/dim]")

    rprint("[cyan]🧠 Loading model via offline-first model loader...[/cyan]")
    from echowall.model_loader import get_model_path
    import json

    model_path = get_model_path("echonet-v1")
    model_data = json.loads(model_path.read_text())

    enc_flat = model_data["weights"]["encoder"]
    hid_flat = model_data["weights"]["hidden"]
    out_flat = model_data["weights"]["output"]

    input_dim  = model_data["architecture"]["input_dim"]
    hidden_dim = model_data["architecture"]["hidden_dim"]
    output_dim = model_data["architecture"]["output_dim"]

    def _matmul_int(x: list[int], W_flat: list[int],
                    rows: int, cols: int) -> list[int]:
        out = [0] * cols
        for c in range(cols):
            acc = 0
            for r in range(rows):
                acc += x[r] * W_flat[r * cols + c]
            out[c] = max(-128, min(127, acc >> 7))
        return out

    def _relu(x: list[int]) -> list[int]:
        return [v if v > 0 else 0 for v in x]

    labels_pred: list[int] = []

    with console.status("[cyan]Running inference...[/cyan]"):
        for feat in features:
            h1 = _relu(_matmul_int(feat,  enc_flat, input_dim,  hidden_dim))
            h2 = _relu(_matmul_int(h1,   hid_flat, hidden_dim, hidden_dim))
            logits = _matmul_int(h2, out_flat, hidden_dim, output_dim)
            pred = max(range(4), key=lambda i: logits[i])
            labels_pred.append(pred)

    class_names = {0: "empty", 1: "standing", 2: "sitting", 3: "fall"}
    n_classes = 4

    correct = sum(t == p for t, p in zip(labels_true, labels_pred))
    accuracy = correct / n_samples if n_samples else 0.0

    cm = [[0] * n_classes for _ in range(n_classes)]
    for t, p in zip(labels_true, labels_pred):
        if 0 <= t < n_classes and 0 <= p < n_classes:
            cm[t][p] += 1

    f1_scores: dict[int, float] = {}
    for c in range(n_classes):
        tp = cm[c][c]
        fp = sum(cm[r][c] for r in range(n_classes)) - tp
        fn = sum(cm[c][r] for r in range(n_classes)) - tp
        precision = tp / (tp + fp) if (tp + fp) else 0.0
        recall    = tp / (tp + fn) if (tp + fn) else 0.0
        f1_scores[c] = (
            2 * precision * recall / (precision + recall)
            if (precision + recall) else 0.0
        )

    macro_f1 = sum(f1_scores.values()) / n_classes

    from rich.table import Table

    console.print("\n")
    console.print(Panel.fit(
        f"[bold green]Accuracy: {accuracy * 100:.1f}%[/bold green]  "
        f"Macro-F1: [bold]{macro_f1:.3f}[/bold]  "
        f"Samples: {n_samples}",
        title="[bold]Benchmark Results[/bold]",
        border_style="green",
    ))

    tbl = Table(title="Per-class F1", show_header=True, header_style="bold cyan")
    tbl.add_column("Class", style="cyan")
    tbl.add_column("Precision", justify="right")
    tbl.add_column("Recall", justify="right")
    tbl.add_column("F1", justify="right")
    tbl.add_column("Support", justify="right")

    for c in range(n_classes):
        tp = cm[c][c]
        fp = sum(cm[r][c] for r in range(n_classes)) - tp
        fn = sum(cm[c][r] for r in range(n_classes)) - tp
        prec = tp / (tp + fp) if (tp + fp) else 0.0
        rec  = tp / (tp + fn) if (tp + fn) else 0.0
        sup  = sum(cm[c])
        tbl.add_row(
            class_names[c],
            f"{prec:.3f}",
            f"{rec:.3f}",
            f"{f1_scores[c]:.3f}",
            str(sup),
        )
    console.print(tbl)

    cm_tbl = Table(title="Confusion Matrix (row=true, col=pred)",
                   show_header=True, header_style="bold")
    cm_tbl.add_column("true \\ pred")
    for c in range(n_classes):
        cm_tbl.add_column(class_names[c], justify="right")
    for r in range(n_classes):
        cm_tbl.add_row(class_names[r], *[str(cm[r][c]) for c in range(n_classes)])
    console.print(cm_tbl)

    if not real:
        rprint(
            "\n[dim]⚠️  Metrics are on synthetic simulation data (seed=42). "
            "See README for hardware benchmark instructions.[/dim]"
        )


if __name__ == "__main__":
    app()
