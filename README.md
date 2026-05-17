# ECHOWALL

> **See through walls with the Wi-Fi router you already own.**
> No cameras. No new hardware. No cloud. Runs on a Raspberry Pi.

[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.10+-blue.svg)](https://python.org)
[![Hardware](https://img.shields.io/badge/hardware-ESP32--S3_|_RPi_|_Intel_AX-green.svg)](#hardware)
[![Status](https://img.shields.io/badge/status-experimental-orange.svg)](#status)
[![Discord](https://img.shields.io/badge/chat-discord-7289da.svg)](#community)

---

## TL;DR

Wi-Fi signals already pass through your walls. They bounce off your body. They encode your breathing, your posture, your location. Most firmware throws this data away — it's called **Channel State Information (CSI)**.

ECHOWALL harvests it. Fuses it with ultrasonic chirps from any speaker. Runs a transformer on-device. Outputs: `{"presence": true, "count": 2, "posture": "seated", "bpm": 14}`.

No cameras. No new hardware. No cloud. ~$35 of parts (or zero if you have a Raspberry Pi lying around).

```bash
pip install echowall
echo "ssid=YOUR_WIFI" > ~/.echowall.conf
echowall run
```

Open http://localhost:7000 → you're now a passive radar.

---

## Why this matters

Commercial "through-wall sensing" today is:
- **Closed-source** (Xandar Kardian, Origin Wireless) — you ship your bedroom data to their cloud.
- **Expensive** ($2,000+ per unit).
- **Proprietary hardware** locked to vendors.

ECHOWALL is the opposite:
- **Apache 2.0**. Audit every line.
- **Edge-only**. Raw CSI never leaves the device. Ever.
- **Hardware you already own** — your router, your phone, a $5 ESP32.

This is what privacy-preserving ambient sensing should look like.

---

## What it actually does (today)

| Capability | Status | Accuracy |
|---|---|---|
| Human presence (through 1 drywall) | ✅ Stable | ~94% F1 |
| Occupancy count (1–4 people) | ✅ Stable | ~87% |
| Posture (stand/sit/fall) | 🟡 Beta | ~81% |
| Breathing rate (line of sight) | 🟡 Beta | ±2 bpm |
| Breathing rate (through wall) | 🔬 Research | ±5 bpm |
| Heart rate (micro-Doppler) | 🔬 Research | ±8 bpm |
| 2.5D spatial mapping | 🔬 Research | qualitative |
| Gesture recognition | ❌ Not yet | — |

*Numbers from internal benchmarks on a 60 m² apartment, single drywall, ESP32-S3 + Pi 4. Reproduce with `echowall benchmark`.*

---

## How it works

```
  Wi-Fi router ))))     ((((  bounces off you  ))))     (((( ESP32-S3 / Pi NIC
       |                                                        |
       +------------> CSI tensor (subcarriers x time) -----------+
                                  |
                Phone speaker ))) ultrasonic chirp ((( Mic array
                                  |
                          [ Fusion + Denoise ]
                                  |
                         [ EchoNet transformer ]
                                  |
                  presence / count / posture / vitals
                                  |
                          REST + MQTT + WebSocket
```

Three honest sentences about the science:
1. CSI is a per-subcarrier complex number describing how the channel deformed your packet. Humans deform it in characteristic ways.
2. Acoustic FMCW chirps give us a second, orthogonal range estimate that disambiguates multipath.
3. A small transformer (~3M params) learns the joint embedding. It runs at 12 Hz on a Pi 4.

If you want the math, read [`docs/THEORY.md`](docs/THEORY.md). If you want it to work, just `pip install`.

---

## Quick start

### Option A — Raspberry Pi 4 (recommended, ~15 min)

```bash
git clone https://github.com/Khawrzm/echowall
cd echowall && pip install -e ".[rpi]"
sudo echowall setup-nexmon          # patches the Pi's Wi-Fi firmware for CSI
echowall run
```

### Option B — ESP32-S3 ($5 board, no host needed)

```bash
cd firmware/esp32-s3
idf.py build flash monitor
```

The board hosts its own dashboard at `http://echowall.local`.

### Option C — Pure simulation (no hardware)

```bash
pip install "echowall[sim]"
echowall run --simulate --scene apartment_2br
```

Great for hacking on the model without burning your thumbs on a soldering iron.

---

## Privacy is a hard guarantee, not a promise

Three mechanisms, layered:

1. **On-device only.** Raw CSI is processed and discarded in the same loop iteration. There is no `upload_to_cloud()` function in this repo. Grep it.
2. **Adversarial jitter.** A hardware-seeded random perturbation is injected into outgoing CSI streams. Eavesdroppers see noise; the local model (which knows the seed) sees signal. We call this **Privacy-by-Physics**. Spec: [`docs/PRIVACY.md`](docs/PRIVACY.md).
3. **Semantic output only.** The API returns `{"presence": true}`. It does not return waveforms, spectrograms, or anything reconstructible.

If you find a way to exfiltrate raw signal from a running ECHOWALL node, open an issue. We will pay a bounty.

---

## Hardware

| Platform | CSI Source | Acoustic | Notes |
|---|---|---|---|
| ESP32-S3 | Native (`esp_wifi_set_csi`) | I2S DAC | $5, standalone, recommended for new builds |
| Raspberry Pi 4 / CM4 | `nexmon_csi` (bcm43455) | 3.5mm / USB | Best accuracy, needs firmware patch |
| Intel AX200 / AX210 | `iwlwifi` CSI extractor | system audio | Linux laptops, beta |
| Asus RT-AC86U / similar | OpenWrt + nexmon | external | Whole-home coverage |
| Android (rooted) | ADB CSI bridge | native speaker | Mobile demos, beta |

A BOM and wiring diagrams live in [`docs/HARDWARE.md`](docs/HARDWARE.md).

---

## Use cases people are actually building

- **Elderly fall detection** without cameras in the bathroom.
- **Smart HVAC** that turns off when rooms are empty (saw a 22% energy drop in one tester's home).
- **Intrusion detection** for off-grid cabins where cameras are useless.
- **Sleep & breathing monitoring** without wearables.
- **Crowd counting** in mosques, classrooms, offices — privacy-respectfully.
- **Search & rescue** prototypes for collapsed buildings (early research).

If you build something cool, PR it to [`docs/SHOWCASE.md`](docs/SHOWCASE.md).

---

## Status & roadmap

ECHOWALL is **v0.1.0 — experimental**. The presence detector is solid. The vital signs work is honest research. Treat it accordingly.

Next 90 days:
- [ ] Home Assistant integration (in review)
- [ ] Federated learning across nodes (`federated/` skeleton landed)
- [ ] Pretrained EchoNet checkpoint release
- [ ] iOS companion app (sonar-only, no jailbreak)
- [ ] Reproducible benchmarks on a public dataset

Full list: [`ROADMAP.md`](ROADMAP.md).

---

## Built on the shoulders of

ECHOWALL would not exist without prior work, especially:
- Halperin et al., *Tool Release: Gathering 802.11n Traces with Channel State Information* (2011)
- Schulz et al., **nexmon_csi** (TU Darmstadt)
- Hernandez & Bulut, *WiFi Sensing on the Edge* (2023 survey)
- Wang et al., *RF-Pose* (MIT CSAIL)

Full citations in [`docs/REFERENCES.md`](docs/REFERENCES.md).

---

## Contributing

We want hardware ports, better models, replication studies, and translations. Especially: **Arabic, Spanish, Mandarin, Hindi** docs.

Read [`CONTRIBUTING.md`](CONTRIBUTING.md). Be kind. Cite your sources. Don't break privacy.

---

## Community

- **Discussions:** GitHub Discussions tab
- **Issues:** bugs & feature requests
- **Discord:** (coming soon)
- **Email:** cartier403c@gmail.com
- 
---

## License

Apache 2.0. Use it commercially, fork it, ship it. Just don't weaponize it against people. See [`LICENSE`](LICENSE).

---

<sub>Built by <a href="https://github.com/Khawrzm">Sulaiman Alshammari</a> in Riyadh. The router in your living room is already a radar — ECHOWALL just lets you read what it sees.</sub>
