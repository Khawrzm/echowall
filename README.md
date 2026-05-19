# ECHOWALL

> **Bare-metal through-wall presence sensing on a $5 ESP32-S3.**
> No cameras. No cloud. No router dependency. Physics-enforced privacy.

[![Tests](https://github.com/Khawrzm/echowall/actions/workflows/test.yml/badge.svg)](https://github.com/Khawrzm/echowall/actions/workflows/test.yml)
[![ESP32 Build](https://github.com/Khawrzm/echowall/actions/workflows/esp32-build.yml/badge.svg)](https://github.com/Khawrzm/echowall/actions/workflows/esp32-build.yml)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)
[![Hardware](https://img.shields.io/badge/hardware-ESP32--S3_%245-green.svg)](#hardware)
[![Status](https://img.shields.io/badge/status-beta-orange.svg)](#status)

> ⚠️ **SAFETY DISCLAIMER:** Fall detection accuracy is **81% (Beta)**.
> This system is **NOT certified for critical life-safety emergencies**.
> Do not use as the sole detection mechanism for medical or emergency response.

---

## What ECHOWALL actually is

A bare-metal firmware for the **ESP32-S3 microcontroller (~$5)** that extracts
Channel State Information (CSI) from 802.11 Wi-Fi frames, fuses it with
ultrasonic FMCW chirps, runs an **SRAM-optimized INT8 Temporal Convolutional
Network (TCN)** fully on-device, and outputs structured presence/posture events
over serial — with zero cloud dependency, zero router reconfiguration, and
zero data exfiltration by physics.

```bash
# Flash the firmware
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py build flash monitor
# The board is now a passive radar. No router changes. No cloud account.
```

---

## Why not commercial alternatives?

Commercial through-wall sensing (Xandar Kardian, Origin Wireless):
- **$2,000+ per unit** — hardware you don't own, locked to their ecosystem.
- **Cloud-mandatory** — your bedroom motion data is uploaded to their servers.
- **Closed-source** — you cannot audit what is collected or inferred.

ECHOWALL is the opposite:
- **$5 hardware** (ESP32-S3). One chip. No subscription.
- **Zero telemetry** — `grep -r 'upload_to_cloud\|requests.post\|http' echowall/` returns nothing in the sensing stack.
- **Apache 2.0** — every line is auditable.

---

## Honest accuracy numbers (v0.2.0 Beta)

| Capability | Status | Accuracy | Notes |
|---|---|---|---|
| Human presence (through 1 drywall) | ✅ Stable | ~94% F1 | ESP32-S3, 60 m² apartment |
| Occupancy count (1–4 people) | ✅ Stable | ~87% | |
| **Posture / Fall detection** | 🟡 **Beta** | **~81%** | ⚠️ NOT for life-safety use |
| Breathing rate (line of sight) | 🟡 Beta | ±2 bpm | |
| Breathing rate (through wall) | 🔬 Research | ±5 bpm | Not a medical device |
| Heart rate (micro-Doppler) | 🔬 Research | ±8 bpm | Not a medical device |

*Reproduce: `echowall benchmark` — runs fully offline on recorded CSI replay.*

---

## How it works

```
  ESP32-S3 (STA mode)  ←── 802.11 frames ───  any AP in range
       │
       ├─ CSI extraction (52–117 subcarriers, 50–100 Hz)
       ├─ Ultrasonic FMCW chirp (I2S DAC → 18–22 kHz → mic)
       ├─ Galois LFSR adversarial jitter (Privacy-by-Physics)
       └─ INT8 TCN inference → {presence, count, posture, confidence}
                                      │
                              Serial / REST / MQTT
                         (semantic output only — no raw CSI)
```

---

## Privacy-by-Physics protocol

Three hard guarantees — not policies, not promises:

1. **On-device processing only.** Raw CSI is processed and discarded in the
   same FreeRTOS task iteration. There is no `upload_to_cloud()` function
   anywhere in this repository. Verify: `grep -r 'upload_to_cloud' .`

2. **Adversarial RF jitter.** A hardware-seeded Galois LFSR injects
   deterministic perturbations into outgoing CSI streams before any
   transmission. A passive eavesdropper receives noise; the local model
   (which holds the seed) receives signal. This is **Privacy-by-Physics** —
   the privacy guarantee is enforced by the mathematics of the LFSR, not by
   a server-side policy that can be changed. Spec: [`docs/PRIVACY.md`](docs/PRIVACY.md).

3. **Semantic output only.** The API surface returns
   `{"presence": true, "count": 2, "posture": "seated"}`. It never returns
   raw waveforms, subcarrier amplitudes, or any signal reconstructible into
   a meaningful representation of the environment.

---

## Hardware

The **only required hardware** is an ESP32-S3 development board (~$5).
No router reconfiguration. No Raspberry Pi required for the core sensing loop.

| Platform | Role | Notes |
|---|---|---|
| **ESP32-S3** | Primary — standalone sensing + inference | $5, recommended |
| Raspberry Pi 4 / CM4 | Optional host for Python dashboard | Best accuracy with nexmon_csi |
| Intel AX200/AX210 | Optional — Linux laptop CSI source | Beta |

---

## Federated Learning mesh (v0.2.0)

Multiple ESP32-S3 nodes can now exchange **masked INT8 weight deltas** via
**ESP-NOW** (MAC-layer P2P, no router required) and perform on-device
**FedAvg aggregation**. Raw CSI never leaves any node. Only `Δw` (weight
deltas), masked with a per-round Galois LFSR stream, are transmitted.

See [`firmware/esp32-s3/components/fl_mesh/`](firmware/esp32-s3/components/fl_mesh/)
and [`docs/THEORY.md`](docs/THEORY.md).

---

## Quick start

```bash
git clone https://github.com/Khawrzm/echowall
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py build flash monitor
```

For the Python analytics host:
```bash
pip install echowall
echowall run
```

---

## Status

ECHOWALL is **v0.2.0 — Beta**. The ESP32-S3 bare-metal stack is the primary
platform. The Python host is a companion analytics layer, not a dependency.

---

## License

Apache 2.0. Audit it. Fork it. Ship it. Don't weaponize it. See [`LICENSE`](LICENSE).

---

<sub>Built by <a href="https://github.com/Khawrzm">Sulaiman Alshammari</a> in Riyadh.
The router in your living room is already a radar — ECHOWALL just lets you read what it sees,
on your own hardware, with your own keys, under your own roof.</sub>
