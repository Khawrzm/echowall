# EchoWall — Integrations Guide

> Plain language. No PhD required.

---

## What EchoWall gives you

A box (Raspberry Pi or ESP32) connected to your Wi-Fi router that tells you:

- Is anyone in the room? How many?
- Are they standing, sitting, or fallen?
- Estimated breathing rate and heart rate

No cameras. No microphones recording audio. No data leaves your home.

---

## 1. One-line install

```bash
curl -sSL https://raw.githubusercontent.com/Khawrzm/echowall/main/install.sh | bash
```

That’s it. No cloud account. No signup. No API key.

---

## 2. Home Assistant (smart home)

If you use Home Assistant, EchoWall appears automatically as a device —
no YAML, no restarts.

**With a local MQTT broker (Mosquitto):**
```python
from echowall.integrations.homeassistant import HassPublisher
from echowall.core.pipeline import EchowallPipeline

pipeline = EchowallPipeline()
pub = HassPublisher(broker="192.168.1.10")  # your HA server IP
pub.start(pipeline)
pipeline.start()
```

**Without MQTT (REST fallback):**
Add this to your `configuration.yaml` in Home Assistant:
```yaml
rest:
  - resource: http://192.168.1.50:8765/ha-discovery
    scan_interval: 2
    sensor:
      - name: "EchoWall Presence"
        value_template: "{{ value_json.entities.presence }}"
      - name: "EchoWall Count"
        value_template: "{{ value_json.entities.count }}"
      - name: "EchoWall Posture"
        value_template: "{{ value_json.entities.posture }}"
```

---

## 3. Python SDK (for developers)

```python
from echowall.sdk import EchoWallClient, RulesEngine

# Connect (auto-discovers node on your LAN)
client = EchoWallClient()

# --- Simple callbacks ---
client.on_presence(lambda r: print(f"{r['count']} people detected"))
client.on_empty(lambda r: print("Room is empty"))
client.on_fall(lambda r: alert("Someone may have fallen!"))

# --- Rules engine (no cloud logic) ---
rules = RulesEngine(client)

# Turn lights on when someone enters
rules.when("presence").then(lambda r: lights.on())

# Turn AC off 5 minutes after room empties
rules.when("empty").after(seconds=300).then(lambda r: ac.off())

# Only act when confidence is high
rules.when("fall").when_confidence(above=0.85).then(lambda r: call_family())

rules.start()  # non-blocking
```

---

## 4. Any language (REST API)

EchoWall exposes a simple HTTP API on port 8765:

```bash
# Check who’s home
curl http://192.168.1.50:8765/presence

# Response:
# {
#   "presence": true,
#   "count": 2,
#   "posture": "sitting",
#   "confidence": 0.94,
#   "breathing_rate": 16.2,
#   "heart_rate": 68.0,
#   "timestamp": 1748000000.0
# }
```

Works from JavaScript, Swift, Kotlin, Rust, shell scripts — anything.

---

## 5. Real-time stream (WebSocket)

```javascript
// Works in browser or Node.js
const ws = new WebSocket("ws://192.168.1.50:8765/ws");
ws.onmessage = (e) => {
  const data = JSON.parse(e.data);
  console.log("Presence:", data.presence, "Count:", data.count);
};
```

---

## Zero cloud. Always.

All processing happens on your device.
Nothing is sent to any server.
No account required. No subscription.
You own your data.
