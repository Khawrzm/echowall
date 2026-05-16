# How ECHOWALL Works

## 1. The Physics

Wi-Fi signals (2.4 GHz / 5 GHz) penetrate walls and interact with any object in the environment. When a human moves — even slightly, from breathing — the multipath pattern of the signal changes. This manifests as tiny variations in the **Channel State Information (CSI)**: the complex-valued frequency response across all OFDM subcarriers.

Think of it like sonar, but using Wi-Fi instead of sound.

## 2. CSI Extraction

Standard Wi-Fi drivers discard CSI after using it for link adaptation. ECHOWALL patches or replaces the driver to expose raw CSI:

- **ESP32-S3**: Custom ESP-IDF firmware exposes CSI via serial
- **Raspberry Pi**: [nexmon_csi](https://github.com/seemoo-lab/nexmon_csi) patches the Broadcom driver
- **Intel Wi-Fi 6 (AX200/AX210)**: [linux-80211n-csitool](https://github.com/dhalperi/linux-80211n-csitool) or iwlwifi fork

## 3. Acoustic Fusion

CSI alone has limited spatial resolution (~1m). ECHOWALL augments it with **inaudible FMCW chirps** (20-22 kHz) emitted from any speaker. By cross-correlating the emitted chirp with its microphone echo, we generate a high-resolution 1D range profile (resolution ~1.5 cm). Fusing this with CSI improves spatial localization significantly.

## 4. EchoNet Inference

`EchoNetBackbone` is a Transformer encoder that processes a sliding window of 10 fused frames (~1 second). It outputs:
- Occupancy count (0, 1, 2, 3 people)
- Posture class (standing, sitting, fallen, moving)
- Confidence score

Breathing rate is extracted separately via bandpass filtering of the CSI amplitude at 0.1–0.5 Hz.

## 5. Privacy-by-Physics™

The key insight: **CSI is only useful if you can correlate it with a baseline**. ECHOWALL adds seeded Gaussian noise that decorrelates any intercepted CSI from the room's true state — but the local model, knowing the seed, can subtract it exactly. This is not encryption — it's a physical deniability layer.

## 6. Output

The API returns only semantic data:
```json
{"presence": true, "count": 2, "posture": "sitting", "confidence": 0.87}
```
No raw CSI, no waveforms, no spatial maps are exposed externally.
