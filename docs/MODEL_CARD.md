# EchoNet v1 — Model Card

> On-device transformer for through-wall presence sensing.
> No cloud. No telemetry. Runs at 12 Hz on a Raspberry Pi 4.

---

## What this model does

EchoNet v1 takes fused Wi-Fi CSI + acoustic FMCW data and outputs:

| Output | Description |
|---|---|
| `presence` | Is at least one human present? (boolean) |
| `count` | How many people? (integer, 0–4) |
| `posture` | standing / sitting / fallen / unknown |
| `breathing_rate` | Breaths per minute (float, null if unavailable) |
| `heart_rate` | BPM estimate (float, null if unavailable) |
| `confidence` | Model certainty 0.0–1.0 |

---

## Performance (v0.1 baseline)

All numbers from internal benchmarks: 60 m² apartment, single drywall, ESP32-S3 + Pi 4.
Reproduce with `echowall benchmark`.

| Capability | Status | Metric |
|---|---|---|
| Human presence (through 1 drywall) | ✅ Stable | ~94% F1 |
| Occupancy count (1–4 people) | ✅ Stable | ~87% accuracy |
| Posture (stand/sit/fall) | 🟡 Beta | ~81% accuracy |
| Breathing rate (line of sight) | 🟡 Beta | ±2 bpm |
| Breathing rate (through wall) | 🔬 Research | ±5 bpm |
| Heart rate (micro-Doppler) | 🔬 Research | ±8 bpm |

---

## Model architecture

- **Type:** Transformer encoder (lightweight)
- **Parameters:** ~3M
- **Input:** 640 features (64 CSI subcarriers × 10 time frames)
- **Hidden dim:** 128 → 64
- **Output:** 8 values (presence, count, posture×5, confidence)
- **Inference speed:** 12 Hz on Raspberry Pi 4 (CPU)
- **Format:** JSON weights (human-readable, no binary format dependency)

---

## How the seed model works

When no trained weights exist on disk, EchoWall seeds an initial model locally
from simulation data — **zero internet required, zero cloud touched**.

The seed model uses biased initialization informed by the known physics of
CSI sensing (presence = energy increase in lower subcarriers, fall = rapid
vertical displacement signature). It is **not randomly initialized** — it is
physics-informed and will produce useful results out of the box.

Run `echowall calibrate` in your real environment to adapt it to your space.

---

## Training data (v0.1)

| Dataset | Environments | Subjects | Hours |
|---|---|---|---|
| Internal (Riyadh apartment) | 1 | 3 | 48 h |
| Simulation (EchoWall sim engine) | 5 scenes | synthetic | ∞ |
| Widar 3.0 (external, pre-training) | 3 | 16 | 120 h |

A public dataset release is planned for v0.3 (see ROADMAP).

---

## Limitations

- Accuracy numbers are from a specific environment — your results may vary.
- Concrete walls reduce presence F1 to ~78%.
- More than 4 people degrades count accuracy significantly.
- Heart rate is research-grade, not medical-grade. Do not use for diagnosis.
- The seed model is physics-informed but not trained on real CSI — run `echowall calibrate` for best results.

---

## Privacy guarantees

- Raw CSI is **never stored or transmitted**.
- Model output is semantic only: `{"presence": true, "count": 2}`.
- No user data leaves the device. Ever.
- Adversarial jitter (Privacy-by-Physics) prevents CSI reconstruction by eavesdroppers.

---

## License

Apache 2.0. Same as the project.

---

*Model card follows [Mitchell et al. (2019)](https://arxiv.org/abs/1810.03993) format.*
