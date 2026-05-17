# ECHOWALL Roadmap

Last updated: May 2026

We ship what works. Items move out of this list only when there is a reproducible benchmark and a passing CI run.

---

## v0.1 — Present (shipped)

- [x] ESP32-S3 CSI capture firmware
- [x] Raspberry Pi 4 + nexmon_csi capture path
- [x] Acoustic FMCW chirp generator + echo extractor
- [x] EchoNet v1 transformer (presence + count)
- [x] REST + MQTT + WebSocket API
- [x] Pure-simulation mode for hardware-less hacking

---

## v0.2 — Next 90 days

- [ ] **Home Assistant integration** — native add-on, MQTT discovery
- [ ] **Pretrained EchoNet checkpoint** released on HuggingFace
- [ ] **Reproducible benchmarks** on a public dataset (Widar 3.0 + ours)
- [ ] **Federated learning** coordinator (skeleton already landed in `federated/`)
- [ ] **iOS companion app** — sonar-only, no jailbreak, TestFlight
- [ ] Improved posture model (target: 90%+ on stand/sit/fall)
- [ ] CLI: `echowall calibrate` walk-through for new rooms
- [ ] Docker image (`ghcr.io/khawrzm/echowall`)

---

## v0.3 — Q4 2026

- [ ] Multi-node fusion (3+ routers triangulating one apartment)
- [ ] Heart-rate model trained with paired PPG ground truth
- [ ] OpenWrt package for consumer routers
- [ ] Android (non-rooted) bridge over Wi-Fi Aware
- [ ] WebAssembly model runtime (browser demo)
- [ ] Public dataset release (anonymized, opt-in contributors)

---

## v1.0 — 2027

The "it just works" release. Criteria:

- One command install on Pi, ESP32, and x86 Linux
- Pretrained models for at least 3 building archetypes (apartment, villa, office)
- < 5% false-positive rate over 30 days in a real home
- Independent third-party privacy audit published
- Stable plugin API for community models

---

## Research track (no committed dates)

Ideas we are exploring. Help wanted.

- 2.5D spatial occupancy maps from a single AP
- Cross-environment domain adaptation (your model survives moving house)
- Gesture vocabulary (~20 gestures, controller-free smart home)
- Search & rescue: detecting trapped survivors through rubble
- Acoustic-only fallback for buildings with no Wi-Fi
- Adversarial robustness: surviving deliberate Wi-Fi jamming

---

## How to influence this roadmap

Open an Issue with the `roadmap` label. The most upvoted items move up. PRs move things faster than upvotes.
