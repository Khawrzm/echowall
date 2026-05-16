# 🔍 ECHOWALL

> **See through walls using the Wi-Fi you already own.**  
> No cameras. No special hardware. No cloud.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.10%2B-blue)](https://python.org)
[![Platform](https://img.shields.io/badge/hardware-ESP32--S3%20%7C%20RPi%20%7C%20Any-green)](#)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#)

---

## What is ECHOWALL?

Every Wi-Fi router in existence emits radio waves that pass through walls and bounce off human bodies. These reflections encode rich physical information — **Channel State Information (CSI)** — that most firmware discards. ECHOWALL harvests it.

Combined with ultrasonic acoustic chirps from any speaker, ECHOWALL turns any building into a **passive 3D radar** that detects:

- 👤 Human presence (through walls, 0-15 meters)
- 🔢 Occupancy count estimation
- 🧍 Posture classification (standing / sitting / fallen)
- 💓 Breathing rate & heart rate (micro-Doppler)
- 🚶 Intrusion & abnormal motion detection
- 🗺️ Through-wall 2.5D spatial mapping

**ECHOWALL is the first open-source system combining:**
1. Wi-Fi CSI passive radar
2. Acoustic chirp fusion (phone speakers, no extra hardware)
3. Federated edge learning (privacy-by-design, runs 100% locally)
4. Adversarial CSI jitter (Privacy-by-Physics™)

---

## 🧠 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     ECHOWALL STACK                          │
├──────────────┬──────────────────────────┬───────────────────┤
│  SENSOR LAYER│     INFERENCE LAYER      │  APPLICATION LAYER│
│              │                          │                   │
│ Wi-Fi CSI    │  CSI Preprocessor        │  REST / MQTT API  │
│ (ESP32-S3,   │  (Amplitude + Phase)     │  Arabic-first SDK │
│  RPi CM4,    │         +                │  Home Assistant   │
│  iwlwifi)    │  Acoustic Chirp Fusion   │  Integration      │
│              │  (FMCW-style ranging)    │                   │
│ Ultrasonic   │         +                │  Dashboard UI     │
│ Chirp (any   │  EchoNet Model           │  (Svelte PWA)     │
│  speaker)    │  (Transformer-based)     │                   │
│              │         +                │  Alerts Engine    │
│ Microphone   │  Federated Learning      │  (fall, intrusion,│
│ Array        │  (local model, no cloud) │   vital signs)    │
└──────────────┴──────────────────────────┴───────────────────┘
```

---

## 🚀 Quick Start

### Option A: Raspberry Pi 4 / CM4 (recommended)

```bash
git clone https://github.com/Khawrzm/echowall
cd echowall
pip install -e ".[rpi]"
echowall init --mode rpi
echowall run
```

### Option B: ESP32-S3 Firmware

```bash
cd firmware/esp32-s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Option C: Simulation (no hardware)

```bash
pip install -e ".[sim]"
echowall run --simulate --scene living_room
```

---

## 🔒 Privacy Architecture

ECHOWALL was designed with **Privacy-by-Physics™**:

1. **No raw CSI leaves the device** — all processing is on-edge
2. **Adversarial Jitter** — a hardware-seeded random perturbation is added to outgoing CSI streams, making them useless to eavesdroppers while the local model (which knows the seed) can reverse it
3. **Federated Learning** — only model weight deltas (not data) are shared between nodes
4. **Output is semantic only** — the API returns `{"presence": true, "count": 2, "posture": "seated"}`, never raw signal data

---

## 📦 Hardware Support Matrix

| Platform | CSI Extraction | Acoustic | Status |
|---|---|---|---|
| ESP32-S3 (custom FW) | ✅ Native | ✅ I2S speaker | ✅ Stable |
| Raspberry Pi 4 + Nexmon | ✅ nexmon_csi | ✅ 3.5mm / USB | ✅ Stable |
| Intel Wi-Fi 6 (iwlwifi) | ✅ CSI Tool | ✅ System audio | 🔄 Beta |
| Android (rooted) | 🔄 ADB bridge | ✅ Native speaker | 🔄 Beta |
| macOS (Virtual) | ❌ Simulation only | ✅ | ✅ Sim |

---

## 📂 Project Structure

```
echowall/
├── core/               # CSI processing & fusion engine
│   ├── csi/            # CSI capture, cleaning, feature extraction
│   ├── acoustic/       # FMCW chirp generation & echo processing
│   └── fusion/         # RF + Acoustic data fusion
├── models/             # EchoNet neural network
│   ├── echonet/        # Transformer-based presence model
│   └── federated/      # Federated learning coordinator
├── firmware/           # Embedded firmware
│   └── esp32-s3/       # ESP-IDF project for CSI extraction
├── api/                # REST + MQTT server
├── sdk/                # Client SDKs (Python, JS, Arabic docs)
├── dashboard/          # Svelte PWA dashboard
├── privacy/            # Adversarial jitter & Privacy-by-Physics
├── sim/                # Simulation environment
├── tests/              # Unit + integration tests
└── docs/               # Full documentation
```

---

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). We welcome hardware ports, model improvements, and language support (Arabic docs are first-class).

---

## 📜 License

Apache 2.0 — free for research, commercial, and humanitarian use.

---

> *Built with the belief that safety technology should be open, sovereign, and available to everyone.*  
> — ECHOWALL Project, 2026
