# ECHOWALL v0.3.0 — Niyah Engine & Khawrizm OS Transition Roadmap

> **Classification:** Public Architectural Declaration  
> **Status:** Planning — Not yet in development  
> **Radical Honesty:** This document describes intended future architecture. Nothing in this file is deployed or functional today.

---

## Strategic Context

ECHOWALL v0.2.0 delivered a sovereign, offline-first passive radar pipeline on constrained hardware. The v0.3.0 transition elevates this from a **single-purpose sensor** into a **general-purpose sovereign intelligence substrate** — the **Niyah Engine**.

The Niyah Engine is not a product update. It is a architectural reclassification: ECHOWALL becomes the Sensory Lobe of a three-part cognitive system running entirely on edge hardware under **Khawrizm OS** — a purpose-built, zero-telemetry operating environment.

---

## The Three-Lobe Brain Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    NIYAH ENGINE v3.0                        │
│                  (Khawrizm OS substrate)                    │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  LOBE I      │  │  LOBE II     │  │  LOBE III        │  │
│  │  Executive   │  │  Sensory     │  │  Cognitive       │  │
│  │              │  │              │  │                  │  │
│  │ - Intent     │  │ - CSI radar  │  │ - Local LLM      │  │
│  │   resolution │  │   (ECHOWALL) │  │   inference      │  │
│  │ - Task       │  │ - Acoustic   │  │ - Pattern        │  │
│  │   scheduling │  │   FMCW       │  │   memory         │  │
│  │ - Resource   │  │ - Env state  │  │ - Context        │  │
│  │   arbitration│  │   fusion     │  │   persistence    │  │
│  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘  │
│         │                 │                    │            │
│         └─────────────────┴────────────────────┘            │
│                           │                                 │
│              ┌────────────▼────────────┐                    │
│              │   PHALANX GATE          │                    │
│              │   Zero-Trust Firewall   │                    │
│              │   (Edge enforcement)    │                    │
│              └─────────────────────────┘                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Lobe Definitions

### Lobe I — Executive

The orchestration layer. Responsible for:

- **Intent resolution:** Translating high-level operator commands into deterministic task graphs
- **Task scheduling:** Priority-based execution across Lobe II and III with hard real-time deadlines
- **Resource arbitration:** SRAM/CPU budget enforcement — no lobe may exceed its allocation
- **Watchdog:** Hardware-level reset authority if any lobe enters undefined state

Implementation target: Rust, `no_std` compatible, deployable on ESP32-S3 or RPi CM4.

### Lobe II — Sensory

The perception layer. Current ECHOWALL pipeline is the seed of this lobe:

- **CSI passive radar** (existing ECHOWALL v0.2.0 pipeline)
- **Acoustic FMCW** ranging and presence confirmation
- **Environmental state fusion:** Unified world-state tensor updated at 10 Hz
- **Adversarial jitter:** Hardware-bound privacy masking on all raw sensor output

All raw sensor data is destroyed after fusion. Only the world-state tensor propagates to Lobe III.

### Lobe III — Cognitive

The inference layer. Stateful pattern recognition and contextual reasoning:

- **Local LLM inference:** Quantized model (INT4/INT8) running fully offline — no API calls
- **Pattern memory:** Ring-buffer of world-state tensors; used for temporal anomaly detection
- **Context persistence:** Cross-session state stored encrypted on local flash only
- **Federated calibration:** FedAvg weight updates via ESP-NOW (zero IP, zero cloud)

---

## Phalanx Gate — Zero-Trust Edge Firewall

All inter-lobe communication passes through **Phalanx Gate**, a software-defined zero-trust boundary enforced at the message-bus level:

| Rule | Enforcement |
|---|---|
| No lobe may initiate outbound network I/O without Executive authorization | Hard block at OS scheduler |
| All inter-lobe messages are schema-validated and length-bounded | Compile-time contract |
| Any message failing schema validation triggers Lobe I watchdog | No silent failures |
| Raw sensor data (CSI, audio) never crosses lobe boundaries | Lobe II destroys raw data before publishing |
| All persisted state is encrypted with a hardware-derived key | ATECC608A or RP2350 OTP |

Phalanx Gate has no configuration file. Its rules are compiled into the firmware image and cannot be altered at runtime.

---

## Khawrizm OS

Khawrizm OS is the minimal operating substrate on which the Niyah Engine runs. It is not a general-purpose OS. Its design constraints are:

- **Zero telemetry by construction:** No network stack loaded unless explicitly authorized by Executive
- **Deterministic scheduler:** Fixed-priority preemptive, no dynamic memory allocation after boot
- **Sovereign boot chain:** Verified boot from OTP hash; refuses to run unsigned firmware
- **Single operator:** Designed for single-owner deployment; no multi-tenant surface
- **Audit log:** Append-only ring buffer of all Executive decisions, stored on local flash

### Target hardware (v0.3.0 planning)

| Platform | Role |
|---|---|
| ESP32-S3 (×2) | Lobe II sensors (CSI + acoustic) |
| Raspberry Pi CM4 | Lobe I Executive + Lobe III Cognitive |
| ATECC608A | Hardware key storage for Phalanx Gate |

---

## v0.3.0 Milestone Sequence

| Milestone | Description | Target |
|---|---|---|
| M1 | Executive Lobe skeleton (Rust, no_std) | TBD |
| M2 | Phalanx Gate message bus prototype | TBD |
| M3 | Lobe II ← ECHOWALL v0.2.0 integration | TBD |
| M4 | Lobe III local LLM inference (INT4) | TBD |
| M5 | Khawrizm OS verified boot chain | TBD |
| M6 | Full three-lobe integration test | TBD |

---

## What This Is Not

- **Not a cloud product.** No SaaS. No subscriptions. No telemetry.
- **Not general-purpose AI.** The Cognitive Lobe serves the sovereign operator only.
- **Not vaporware.** Every architectural claim in this document must be backed by a working test before the milestone is closed.

---

*This roadmap is a living document. All milestone dates are TBD until Lobe I execution scheduling is implemented. Radical Honesty: nothing here is built yet.*
