"""EchoWall first-time setup wizard — one command does everything.

Detects platform, installs dependencies, seeds model, generates config.
Zero assumed knowledge. Works on Pi, Linux, Mac.

Run: echowall setup
"""

from __future__ import annotations
import os
import platform
import socket
import subprocess
import sys
from pathlib import Path

from rich.console import Console
from rich.panel import Panel
from rich.prompt import Confirm, Prompt
from rich import print as rprint

console = Console()


def detect_platform() -> str:
    machine = platform.machine().lower()
    system = platform.system().lower()
    if "aarch64" in machine or "armv" in machine:
        try:
            model = Path("/proc/device-tree/model").read_text()
            if "raspberry" in model.lower():
                return "rpi"
        except Exception:
            pass
        return "rpi"
    if system == "darwin":
        return "mac"
    return "linux"


def wizard():
    console.print(Panel.fit(
        "[bold cyan]Welcome to EchoWall[/bold cyan]\n"
        "[dim]This wizard takes ~2 minutes. Then you're done.[/dim]",
        border_style="cyan"
    ))

    platform_name = detect_platform()
    rprint(f"\n[green]✔ Detected platform:[/green] [bold]{platform_name}[/bold]")

    # Step 1: Wi-Fi SSID
    rprint("\n[bold]📡 Step 1 of 4: Wi-Fi[/bold]")
    ssid = Prompt.ask(
        "  Your Wi-Fi network name (SSID)",
        default=_guess_ssid()
    )

    # Step 2: Home Assistant
    rprint("\n[bold]🏠 Step 2 of 4: Smart home[/bold]")
    use_ha = Confirm.ask("  Do you use Home Assistant?", default=False)
    ha_ip = None
    if use_ha:
        ha_ip = Prompt.ask("  Home Assistant IP address", default="192.168.1.10")

    # Step 3: mode
    rprint("\n[bold]⚙️  Step 3 of 4: Hardware[/bold]")
    rprint("  [dim]1 = Raspberry Pi (recommended)")
    rprint("  2 = ESP32-S3 board")
    rprint("  3 = Simulation (no hardware, test now)[/dim]")
    choice = Prompt.ask("  Choose", choices=["1", "2", "3"], default="1")
    mode_map = {"1": "rpi", "2": "esp32", "3": "sim"}
    mode = mode_map[choice]

    # Step 4: generate config + seed model
    rprint("\n[bold]📦 Step 4 of 4: Setting up...[/bold]")

    cfg = {
        "ssid": ssid,
        "mode": mode,
        "ha_ip": ha_ip,
        "api_port": 8765,
        "acoustic_enabled": True,
        "privacy_jitter": True,
    }

    import json
    cfg_path = Path.home() / ".echowall.conf"
    cfg_path.write_text(json.dumps(cfg, indent=2))
    rprint(f"  [green]✔[/green] Config saved → {cfg_path}")

    # Seed model offline
    from echowall.model_loader import get_model_path
    model_path = get_model_path()
    rprint(f"  [green]✔[/green] Model ready → {model_path}")

    # Done
    console.print(Panel.fit(
        "[bold green]Setup complete![/bold green]\n\n"
        + (f"[cyan]echowall run[/cyan]" if mode != "sim"
           else "[cyan]echowall run --simulate[/cyan]")
        + "[dim]  ← run this now[/dim]",
        border_style="green"
    ))

    if use_ha and ha_ip:
        rprint(
            f"\n[dim]Home Assistant: EchoWall will appear automatically at {ha_ip}[/dim]\n"
            "[dim]No YAML needed. Check Settings → Devices & Services.[/dim]"
        )


def _guess_ssid() -> str:
    try:
        if platform.system() == "Darwin":
            result = subprocess.run(
                ["/System/Library/PrivateFrameworks/Apple80211.framework/"
                 "Versions/Current/Resources/airport", "-I"],
                capture_output=True, text=True, timeout=3
            )
            for line in result.stdout.splitlines():
                if "SSID" in line and "BSSID" not in line:
                    return line.split(":", 1)[1].strip()
    except Exception:
        pass
    return ""
