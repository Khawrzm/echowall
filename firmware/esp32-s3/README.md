# ECHOWALL ESP32-S3 Firmware

Extracts Wi-Fi CSI from ESP32-S3 and streams it over serial (JSON) to the host ECHOWALL pipeline.

## Requirements
- ESP32-S3 DevKit (any variant with Wi-Fi 802.11n)
- ESP-IDF v5.1+
- USB-C cable

## Build & Flash

```bash
# Install ESP-IDF first: https://docs.espressif.com/projects/esp-idf/en/latest/
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Serial Protocol

Outputs one JSON object per CSI frame at 921600 baud:

```json
{
  "ts": 1234567890.123,
  "rssi": -65,
  "channel": 6,
  "csi_real": [0.12, -0.34, ...],
  "csi_imag": [0.56, 0.78, ...]
}
```

Array length = `N_ANTENNAS × N_SUBCARRIERS` = 3 × 64 = 192 elements each.

## Configuration

Edit `main/config.h`:
```c
#define ECHOWALL_WIFI_SSID     "your_network"
#define ECHOWALL_WIFI_PASS     "your_password"
#define ECHOWALL_CSI_ANTENNAS  3
#define ECHOWALL_BAUD_RATE     921600
```
