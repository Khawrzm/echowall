# EchoWall — Setup Guide

> Zero assumed knowledge. If you can plug in a USB drive, you can set up EchoWall.

---

## What you need

- A Raspberry Pi 4 (or any Linux machine) — $35
- Your existing Wi-Fi router (no changes needed)
- 10 minutes

That's it. No cloud account. No subscription. No app store.

---

## One-command install

Open a terminal on your Raspberry Pi and run:

```bash
curl -sSL https://raw.githubusercontent.com/Khawrzm/echowall/main/install.sh | bash
```

This:
1. Detects your platform automatically
2. Installs everything needed
3. Seeds the AI model **offline** (no internet download)
4. Generates your config file

Then:
```bash
echowall setup   # 4-question wizard, 2 minutes
echowall run     # you're live
```

Open **http://your-pi-ip:8765** to see the live dashboard.

---

## Home Assistant (smart home)

If you use Home Assistant, EchoWall appears **automatically** as a device —
no YAML, no restarts, no configuration files.

**Option 1 — Automatic (recommended):**

Answer "yes" when `echowall setup` asks about Home Assistant.
EchoWall will find it on your network and connect automatically.

Then in Home Assistant:
`Settings → Devices & Services → EchoWall` — it's already there.

**Option 2 — HACS custom component:**

```
1. Install HACS (https://hacs.xyz) if you haven't already
2. Add custom repository: https://github.com/Khawrzm/echowall
3. Install "EchoWall" from HACS
4. Restart Home Assistant
5. Settings → Devices & Services → Add Integration → EchoWall
6. Enter your EchoWall node IP (find it with: echowall discover)
```

You get these sensors automatically:
- `binary_sensor.echowall_presence` — is someone home?
- `sensor.echowall_count` — how many people?
- `sensor.echowall_posture` — standing / sitting / fallen
- `sensor.echowall_breathing_rate` — breaths per minute

---

## No smart home? No problem.

EchoWall works standalone. Open the dashboard:

```
http://your-pi-ip:8765
```

Or from any phone on the same Wi-Fi:
```
http://echowall.local:8765
```

---

## Test without hardware (simulation)

```bash
echowall run --simulate --scene living_room
```

Shows real output with simulated data. No Pi needed. Great for trying it out first.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `echowall: command not found` | Run `pip install echowall` first |
| Can't reach dashboard | Check Pi IP with `hostname -I` |
| HA doesn't show EchoWall | Check they're on the same Wi-Fi network |
| Model says "unknown" for everything | Run `echowall calibrate` in an empty room |
| Accuracy seems low | Concrete walls reduce accuracy — try moving the Pi closer to the room center |

---

## Privacy

- EchoWall **never sends data to the internet**.
- No cameras. No audio recording.
- All processing happens on your Pi. Nothing leaves your home.
- You can verify this: `grep -r 'upload\|cloud\|http' echowall/` — zero results in the sensing code.

---

*Built in Riyadh. Runs everywhere.*
