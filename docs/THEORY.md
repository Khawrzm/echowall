# Theory of Operation

This is the math behind ECHOWALL. If you just want to install it, see the [README](../README.md).

---

## 1. Channel State Information (CSI)

Every 802.11 OFDM packet is a comb of subcarriers. The receiver estimates
the complex channel response `H(f, t)` on each subcarrier to equalize the
signal. That estimate — normally discarded after equalization — *is* the
channel state. For the ESP32-S3 it is 52 (HT20) or 117 (HT40) complex taps,
sampled at 50–100 Hz via `esp_wifi_set_csi()`.

When a human body moves through the channel, multipath geometry changes.
The amplitude `|H|` and phase `∠H` of each subcarrier move in characteristic,
low-rank ways. Those motions are what the TCN learns from.

---

## 2. Acoustic FMCW fusion

Wi-Fi CSI is rich but spatially ambiguous. FMCW (Frequency-Modulated
Continuous Wave) chirps over an ultrasonic carrier (18–22 kHz, inaudible)
give a clean **range** estimate via beat frequency:

```
range = (c × f_beat) / (2 × slope)
```

where `c` is the speed of sound, `f_beat` is the beat frequency at the mic,
and `slope` is the chirp slope in Hz/s. Fusing the two modalities removes
~70% of false positives seen in CSI-only ablations.

---

## 3. Preprocessing

1. **Phase sanitization.** Sen-Tan-Roy linear-fit correction per packet.
2. **Subcarrier selection.** Drop pilot and guard subcarriers.
3. **Amplitude denoise.** Hampel filter (window 7, σ=3) per subcarrier.
4. **Doppler tensor.** STFT over 2-second windows; keep ±5 Hz around DC.
5. **Acoustic range bins.** Cross-correlate received chirp with template.

Output per 200 ms tick: `[subcarriers × doppler_bins]` tensor + `[acoustic_bins]` vector.

---

## 4. Inference model — SRAM-optimized INT8 TCN

The v0.2.0 model is an **SRAM-optimized INT8 Temporal Convolutional Network
(TCN)** — not the previously documented 3M-parameter Transformer. The
architecture change was driven by the ESP32-S3's 384 KB internal SRAM
constraint.

### Architecture

```
  [CSI tensor 640-dim]     [Acoustic vector]
         │                        │
   TCN block × 3             Linear × 2
   (dilated Conv1D,               │
    dilation = 1,2,4)             │
         │                        │
         └──────── concat ────────┘
                       │
              Linear (128 → 64)
                       │
          ┌────────────┴────────────┐
          │                         │
  Presence/Count head        Posture/Vitals head
    (INT8 softmax)             (INT8 regression)
```

### Memory footprint (INT8)

| Layer | Weights | Bytes |
|---|---|---|
| TCN encoder (640→64) | 640 × 64 | 40,960 |
| Hidden (64→64) | 64 × 64 | 4,096 |
| Output (64→8) | 64 × 8 | 512 |
| Biases (INT32) | (64+64+8) × 4 | 544 |
| **Total** | | **~46 KB** |

Inference on ESP32-S3 (Xtensa LX7 @ 240 MHz): **~85 ms** (11.7 Hz).
No float on the inference hot path — all operations in INT8/INT16.

---

## 5. Training Dataset & Deterministic Baseline

### Hardware setup

The v0.1 baseline was evaluated on data collected from a single ESP32-S3
node in a 60 m² apartment (2-bedroom, concrete walls, 2.4 GHz HT20 channel).

| Parameter | Value |
|---|---|
| Environment | 60 m² apartment, 2-bedroom |
| Wall material | Reinforced concrete |
| Wi-Fi band | 2.4 GHz, HT20, channel 6 |
| Subcarriers | 52 active (HT20) |
| Sampling rate | 50 Hz |
| ESP32-S3 firmware | v0.2.0 (`echowall-firmware`) |
| Data collection | 4 × 2-hour sessions |
| Annotators | 1 (self-annotated via timestamp log) |

### Class distribution (real-hardware dataset)

| Class | Label | Real samples | Duration |
|---|---|---|---|
| Empty room | 0 | 1,440 | 28.8 min |
| Standing | 1 | 860 | 17.2 min |
| Sitting | 2 | 920 | 18.4 min |
| Fall event | 3 | 180 | 3.6 min (simulated falls) |

### Deterministic offline seed (v0.1-baseline)

Because real hardware data is not redistributable (privacy), the public
repository ships a **deterministic synthetic seed** instead:

- **Generator:** `scripts/generate_csi_dataset.py --seed 42 --n-per-class 50`
- **Seed:** `numpy.random.default_rng(seed=42)`
- **Physics bias:** Lower subcarrier weights (`encoder[:20, :]`) are scaled
  by `2.5×` to reflect the known physics of presence detection (torso
  reflection dominates lower subcarriers in 2.4 GHz HT20).
- **Quantization:** INT8, matching ESP32-S3 TCN input format.
- **Radical Honesty:** This seed produces non-trivial benchmark results but
  does **not** reproduce the real-hardware accuracy numbers in the README.
  To reproduce those numbers, collect real CSI data with an ESP32-S3 and
  run `echowall benchmark --dataset /path/to/real.csv --real`.

### Reproducing the seed

```bash
# From repo root:
python scripts/generate_csi_dataset.py --seed 42 --n-per-class 50
# Writes: tests/data/sample_csi_fall.csv
#         tests/data/sample_csi_fall.meta.json

# Or via CI (mobile-friendly):
# Actions → Generate Synthetic CSI Dataset (HITL) → Run workflow → merge PR
```

---

## 6. Federated Learning — Zero-Telemetry FedAvg via ESP-NOW

Multiple ESP32-S3 nodes improve the shared model without transmitting raw
CSI or inference results. Only masked weight deltas leave each node.

### Protocol

1. **Local training:** Each node runs SGD on its own CSI data, producing
   updated weights `w_new`.
2. **Delta computation:** `Δw = w_new − w_base` (INT8, wraps intentionally).
3. **Privacy masking:** `Δw_masked = Δw XOR LFSR(seed)` where
   `seed = MAC[4:5] XOR round_id` — a unique stream per node per round.
4. **Chunked ESP-NOW broadcast:** `Δw_masked` is split into 208 chunks of
   220 bytes each and broadcast at MAC layer (no router, no IP stack).
   Inter-frame gap: 10 ms → ~2.1 s total per round.
5. **FedAvg aggregation (receiving nodes):**

```
w_global[i] = clamp8( w_base[i] + (1/N) × Σ_j unmask(Δw_j[i]) )
```

INT16 accumulation prevents overflow: max sum across 4 peers =
4 × 127 = 508, which fits in INT16 before the final divide-and-clamp.

### What is transmitted

| Data | Transmitted? |
|---|---|
| Raw CSI subcarrier values | ❌ Never |
| Acoustic chirp recordings | ❌ Never |
| Inference outputs (presence/posture) | ❌ Never |
| Masked INT8 weight deltas (Δw) | ✅ Yes — to LAN peers only via ESP-NOW |

---

## 7. Known limitations

- **Cross-environment generalization.** A model trained in one room loses
  ~20% F1 in a different floor plan. Use `echowall calibrate` on deployment.
- **Through-wall vitals.** Single-AP BPM through concrete is research-grade.
  Do not treat as a medical measurement.
- **Fall detection is 81% accurate (Beta).** Not certified for life-safety
  emergency use. See README disclaimer.
- **Crowd > 4 people** degrades the count head significantly.
- **Synthetic benchmark ≠ real-hardware accuracy.** The bundled seed dataset
  (numpy seed=42) will not reproduce the README accuracy figures. Hardware
  CSI collection is required for production validation.

---

## 8. References

See [REFERENCES.md](REFERENCES.md). Start with Halperin 2011 and Schulz nexmon_csi.
