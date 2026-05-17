# Theory of Operation

This is the math behind ECHOWALL. If you just want to install it, see the [README](../README.md).

---

## 1. Channel State Information (CSI), in one paragraph

Every 802.11 OFDM packet is a comb of subcarriers. The receiver estimates the complex channel response `H(f, t)` on each subcarrier to equalize the signal. That estimate — normally discarded after equalization — *is* the channel state. For a bcm43455 it is a 256-tap complex vector; for ESP32-S3 it is 52 (HT20) or 117 (HT40) taps. We sample it at 50–100 Hz.

When a human body moves through the channel, multipath geometry changes. The amplitude `|H|` and phase `∠H` of each subcarrier move in characteristic, low-rank ways. Those motions are what we learn from.

---

## 2. Why fuse acoustic FMCW chirps

Wi-Fi CSI is rich but ambiguous: a hand wave near the AP and a body move at 5 m can look similar after the channel collapses.

FMCW (Frequency-Modulated Continuous Wave) chirps over an ultrasonic carrier (18–22 kHz, inaudible to most adults) give us a clean **range** estimate via beat frequency:

```
range = (c * f_beat) / (2 * slope)
```

where `c` is the speed of sound, `f_beat` is the beat frequency at the mic, and `slope` is the chirp slope in Hz/s. Combining the two modalities removes ~70% of the false positives we saw in CSI-only ablations.

---

## 3. Preprocessing

1. **Phase sanitization.** Raw CSI phase is corrupted by CFO, SFO, and PBD. We apply the Sen-Tan-Roy linear-fit correction per-packet.
2. **Subcarrier selection.** Drop pilot and guard subcarriers. Keep data subcarriers only.
3. **Amplitude denoise.** Hampel filter (window 7, n_sigmas 3) per subcarrier.
4. **Doppler tensor.** STFT over 2-second windows; keep ±5 Hz around DC — that is where human motion lives.
5. **Acoustic range bins.** Cross-correlate received chirp with template; pick top-K peaks.

Output per 200 ms tick: a `[subcarriers, doppler_bins]` tensor + an `[acoustic_bins]` vector.

---

## 4. EchoNet model

A 3M-parameter encoder, two heads.

```
          [CSI tensor]              [Acoustic vector]
               |                            |
         Conv1D x 3                   Linear x 2
               |                            |
               +-------- concat ------------+
                            |
                    4-layer Transformer
                       (dim=128, heads=4)
                            |
          +-----------------+------------------+
          |                                    |
   Presence/Count head                  Posture/Vitals head
      (softmax)                            (regression)
```

Trained with focal loss on presence/count and Huber loss on regression heads. We use 80% labeled real data + 20% sim-2-real augmentation.

Inference: ~14 ms on a Pi 4, ~6 ms on Apple M2, ~85 ms on ESP32-S3 (quantized int8).

---

## 5. Federated learning

We ship a personal model out of the box. To improve, nodes optionally participate in federated rounds:

- Each node trains a local LoRA adapter on its own data.
- It uploads only the adapter delta (~200 KB), DP-clipped at `ε=2`.
- The coordinator averages and ships back a new global adapter.

No raw signals leave the node. We are deliberately conservative on DP because passive radar data is sensitive by nature.

---

## 6. Known limitations (read this before opening an issue)

- **Cross-environment generalization is hard.** A model trained in your living room loses ~20% F1 in a different floor plan. v1.0 will address with domain-adaptive heads.
- **Through-wall vitals are noisy.** Single-AP through-wall BPM is honest-to-god research, not a product. Treat the numbers as confidence intervals, not measurements.
- **2.4 GHz vs 5 GHz tradeoffs.** 2.4 GHz penetrates better but has more interference. We default to 5 GHz indoors.
- **Crowd > 6 people** breaks the count head. The training distribution is sparse there.

---

## 7. Want to go deeper?

The references that shaped this design are listed in [REFERENCES.md](REFERENCES.md). Start with Halperin 2011 and Schulz nexmon_csi.
