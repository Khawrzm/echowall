# EchoWall Companion — iOS App

> Native SwiftUI presence dashboard. LAN-only. Zero cloud. No jailbreak required.

---

## Requirements

- Xcode 15.2+
- iOS 17.0+ deployment target
- Swift 5.9+
- An EchoWall node running on your local network (`echowall run` on the Pi or ESP32-S3)

---

## Xcode Project Setup

This folder contains Swift source files only — no `.xcodeproj` is committed to keep
the repo clean. Create the Xcode project once:

```
1. Open Xcode → File → New → Project
2. Choose: iOS → App
3. Product Name:       EchoWallCompanion
4. Bundle Identifier:  com.echowall.companion   (or your own)
5. Interface:          SwiftUI
6. Language:           Swift
7. Minimum Deployment: iOS 17.0
8. Uncheck: Include Tests (optional)
9. Save into:          apps/ios/EchoWallCompanion/
10. Replace the generated ContentView.swift with the one in this folder.
11. Add LocalRadarClient.swift and RadarState.swift to the target.
```

### Required Entitlements

Add to your app's `Info.plist`:

```xml
<!-- Allow outbound connections to EchoWall node on LAN -->
<key>NSLocalNetworkUsageDescription</key>
<string>EchoWall connects to your local EchoWall sensor node to display presence and posture data. No data leaves your home network.</string>

<!-- Allow plain HTTP/WS on LAN (EchoWall runs on ws://, not wss://) -->
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSAllowsLocalNetworking</key>
    <true/>
</dict>
```

---

## Architecture

```
ContentView.swift          — SwiftUI root view, dark radar dashboard
LocalRadarClient.swift     — WebSocket client (Network.framework)
RadarState.swift           — ObservableObject state model
```

Data flow:
```
EchoWall node (ws://echowall.local:8765/ws)
    └─ JSON: {"presence": true, "count": 2, "posture": "standing", "confidence": 0.94}
              ↓
         LocalRadarClient (NWConnection / URLSessionWebSocketTask)
              ↓
         RadarState (@Published properties)
              ↓
         ContentView (SwiftUI, redraws on state change)
```

Only semantic JSON is received. Raw CSI never leaves the EchoWall node.
This app is a display terminal, not a sensor.

---

## Default Connection

The app defaults to `ws://echowall.local:8765/ws` (mDNS hostname).
If your node doesn't advertise mDNS, enter the IP directly in Settings.

---

## Privacy

- No network calls outside your LAN.
- No analytics SDK.
- No crash reporting to any external server.
- No data stored beyond the current session.
- `grep -r 'URLSession\|Network' EchoWallCompanion/` — every outbound
  call targets `echowall.local` or a user-configured LAN IP.
