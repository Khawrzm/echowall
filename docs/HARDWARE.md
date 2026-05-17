# Hardware Guide

ECHOWALL works on multiple platforms. This document tells you what to buy and how to wire it.

---

## TL;DR: which platform should I pick?

| Your situation | Use this |
|---|---|
| I want to try it tonight, I have a Pi 4 | Raspberry Pi 4 + nexmon_csi |
| I want a tiny standalone sensor | ESP32-S3 board |
| I have a Linux laptop with AX200/AX210 | Intel iwlwifi route |
| I want whole-home coverage | OpenWrt router + nexmon |
| I just want to hack on models | Simulation — no hardware |

---

## Option A — Raspberry Pi 4 / CM4 (recommended)

**BOM** (~$60 total):

| Item | Approx. price | Notes |
|---|---|---|
| Raspberry Pi 4 (2 GB) | $35 | CM4 also works |
| microSD 32 GB | $8 | UHS-I class 10 |
| USB-C 5V 3A PSU | $10 | official one is bulletproof |
| USB sound card | $5 | optional; on-board jack works |
| Small speaker + electret mic | $5 | any 8Ω driver |

**Why the Pi?** The bcm43455c0 chip exposes full 256-tap CSI via the `nexmon_csi` patch. It is currently the cleanest signal quality you can get without going to commercial test gear.

**Setup outline:**

```bash
sudo apt install raspberrypi-kernel-headers git
git clone https://github.com/seemoo-lab/nexmon_csi
# follow nexmon_csi README to build the firmware patch
echo "options bcm43455 csi_enable=1" | sudo tee /etc/modprobe.d/csi.conf
sudo reboot
echo "verify" → `iw dev wlan0 link` should still associate normally
```

Then `echowall setup-nexmon` automates the rest.

---

## Option B — ESP32-S3 (smallest, $5)

**BOM** (~$15 total):

| Item | Approx. price | Notes |
|---|---|---|
| ESP32-S3-DevKitC-1 | $7 | any S3 board with 2x antennas works |
| I2S DAC (MAX98357A) | $4 | for ultrasonic chirp |
| 8Ω mini speaker | $2 | small is fine, we use 18–22 kHz |
| INMP441 mic (I2S) | $2 | omnidirectional, MEMS |

**Why ESP32-S3?** Native CSI API (`esp_wifi_set_csi_rx_cb`), enough flash for a quantized model, and it costs less than a coffee.

**Wiring** (default pins, see `firmware/esp32-s3/main/pins.h` to remap):

```
  ESP32-S3        MAX98357A (speaker)
  ---------       -------------------
  GPIO 5    -->   BCLK
  GPIO 6    -->   LRC
  GPIO 7    -->   DIN
  3V3       -->   VIN
  GND       -->   GND

  ESP32-S3        INMP441 (mic)
  ---------       -------------
  GPIO 9    -->   SCK
  GPIO 10   -->   WS
  GPIO 11   -->   SD
  GND       -->   L/R + GND
  3V3       -->   VDD
```

Flash with `idf.py -p /dev/ttyUSB0 flash monitor`. The board hosts its own dashboard at `http://echowall.local` once it joins your Wi-Fi.

---

## Option C — Intel AX200 / AX210 laptop

Works on Linux with the `iwlwifi` CSI patch. Beta. See `docs/INTEL_AX_SETUP.md` (coming with v0.2). Expect to compile a custom kernel module. Not for the faint of heart.

---

## Option D — OpenWrt router (whole-home)

Supported router chipsets:

- Broadcom bcm4366c0 (Asus RT-AC86U) — best
- Broadcom bcm4339 (Nexus 5 era) — legacy but proven
- MediaTek MT7621 + MT7615 — beta

Flash OpenWrt 23.05+, install our `echowall-csi` package (coming Q3 2026), point your existing devices at it. The router itself remains a normal router.

---

## Acoustic considerations

- Use 18–22 kHz. Below 18 kHz some people can hear it. Above 22 kHz most consumer speakers roll off hard.
- Keep the speaker and mic at least 30 cm apart to avoid direct path saturation.
- Tile/glass rooms reflect ultrasonics like a mirror. Throw a rug or two in the test room.
- Pets: cats and dogs can hear up to ~45 kHz. Stay under 22 kHz to be safe.

---

## What does NOT work (yet)

- Wi-Fi 7 / 802.11be CSI exposure — too new, firmware patches do not exist.
- Chromecast / cheap IoT chips — most do not expose CSI at all.
- iPhone CSI — sandboxed away from us. We use mic-only on iOS.
- Mesh networks where one node hops channels — our model assumes a stable channel.

---

## Submit your build

If you bring up ECHOWALL on a platform we have not documented, PR a section here. We will credit you.
