# 🏴‍☠️ ECHOWALL - Privacy by Physics

> *You cannot hack physics. You cannot subpoena a signal that never leaves the room.*

[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-black?style=flat-square)](LICENSE)
[![Status: v0.3.0-alpha](https://img.shields.io/badge/status-v0.3.0__alpha-yellow?style=flat-square)](https://github.com/Khawrzm/echowall/releases)
[![Made in Riyadh](https://img.shields.io/badge/Made_in-Riyadh_🇸🇦-006c35?style=flat-square)](https://khawrizm.com)
[![Ecosystem](https://img.shields.io/badge/ecosystem-Sovereign__Stack-green?style=flat-square)](https://github.com/Khawrzm)

**Turn your existing Wi-Fi router into a passive edge radar. Detect human presence through walls. Zero cameras. Zero cloud telemetry.**

Built in Riyadh. Compiled from physics. Owned by you.

---

## 📡 Privacy-by-Physics: The Mathematical Core

Unlike centralized AI systems that rely on visual data and cloud telemetry, **EchoWall** relies entirely on the physics of RF signals. We process Wi-Fi Channel State Information (CSI) directly at the edge, using local firmware. Zero cameras. Zero cloud APIs.

### 1. Channel Frequency Response (CFR)

When a Wi-Fi packet travels between a transmitter and EchoWall's ESP32 receiver, it captures the multi-path environment. The CSI matrix $H(f,t)$ at time $t$ for subcarrier $f$ is represented as:

$$
H(f,t) = \sum_{i=1}^{L} \alpha_i(t) e^{-j 2 \pi f \tau_i(t)} + N(f,t)
$$

Where:
- $\alpha_i(t)$ = amplitude attenuation of path $i$
- $\tau_i(t)$ = time delay of path $i$ (including human body reflections)
- $N(f,t)$ = ambient noise
- $L$ = number of multi-paths

### 2. Phase Sanitization (Ring-0 Execution)

Raw CSI phase data is corrupted by Carrier Frequency Offset (CFO) and Sampling Frequency Offset (SFO). Before the local **Niyah Engine** evaluates intent, EchoWall sanitizes the phase deterministically:

$$
\tilde{\theta}_k = \theta_k - \frac{\theta_n - \theta_1}{n - 1} (k - 1) - \theta_1
$$

This linear transformation removes hardware-induced phase shifts, leaving only the pure physical disruption caused by human movement (e.g., breathing, walking).

### 3. The Sovereign Anomaly (Doppler Extraction)

By extracting the Doppler shift from sanitized subcarriers across time $t$, EchoWall constructs a real-time **"Human Presence Matrix"** entirely in the RAM of a $5 chip:

$$
\Delta f_d = \frac{v \cos(\theta)}{\lambda}
$$

Where:
- $v$ = velocity of human movement
- $\theta$ = angle between movement direction and signal path
- $\lambda$ = wavelength of Wi-Fi signal (2.4 GHz or 5 GHz)

**Conclusion:** The algorithm has returned home. 🏴‍☠️

📖 **[Read the Blackpaper v6.0-draft →](BLACKPAPER_v6.0-draft.md)**
*(“Sovereign Infrastructure as Compiled Physics”)*

-----

## ⚠️ WRAPPER CULTURE WARNING

If you are looking for:

- ❌ Cloud-native SaaS platforms
- ❌ VC-funded growth metrics
- ❌ API wrappers marketed as “innovation”
- ❌ Telemetry disguised as “analytics”

**This is not the repository you seek.**

Khawrizm is a **sovereign infrastructure stack** that:

- ✅ Runs entirely offline on $35–$1,300 hardware
- ✅ Zero cloud dependencies, zero telemetry, zero external APIs
- ✅ Bare-metal firmware, post-quantum cryptography, Delta CRDTs
- ✅ Privacy enforced by physics, not policies

> *“The algorithm always returns home.”*

-----

## 🧩 THE SOVEREIGN STACK

```text
┌─────────────────────────────────────────────────────────────┐
│                    KHAWRIZM ECOSYSTEM                        │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ↓                     ↓                     ↓
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   ECHOWALL   │    │    NIYAH     │    │  HavenOS     │
│ Edge Sensing │    │   Engine     │    │ (Microkernel)│
│   ($5 chip)  │    │ (Intent AI)  │    │  (<1000 LOC) │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ↓                  ↓                  ↓
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ SIDSense     │  │ Delta CRDTs  │  │ PQC Hybrid   │
│ (TVWS AI)    │  │ (Consensus)  │  │ (Security)   │
└──────────────┘  └──────────────┘  └──────────────┘
                           │
                           ↓
              ┌────────────────────┐
              │  Stratospheric     │
              │  HAPS + FSO Lasers │
              │  (Phase 4 Goal)    │
              └────────────────────┘
```

-----

## 📦 REPOSITORY STRUCTURE

|Component               |Purpose                                                          |Status          |Documentation                      |
|------------------------|-----------------------------------------------------------------|----------------|-----------------------------------|
|**<apps/>**             |iOS/Android companion apps                                       |Active Dev      |-                                  |
|**<echowall/>**         |Bare-metal through-wall sensing via Wi-Fi CSI + acoustic chirps  |Beta v0.2.0     |[README](echowall/README.md)       |
|**<niyah_engine/>**     |Sovereign AI intent processing via Arabic morphological grounding|Alpha v0.1.0    |[README](niyah_engine/README.md)   |
|**<niyah_executive/>**  |Multi-step planning & constraint checking                        |Alpha v0.1.0    |[README](niyah_executive/README.md)|
|**<khawrizm_os/>**      |Microkernel + RISC-V emulator (<1000 lines C)                    |Proof-of-Concept|[README](khawrizm_os/README.md)    |
|**<firmware/>**         |ESP32-S3, RK3588, LimeSDR firmware                               |Active Dev      |[Docs](firmware/README.md)         |
|**<cometx-hybrid/>**    |Local AI browser with MCP tool integration                       |Alpha           |[README](cometx-hybrid/README.md)  |
|**<casper_engine/>**    |Behavioral detection & fraud analysis                            |Research        |[Docs](docs/CASPER.md)             |
|**<custom_components/>**|Hardware-specific drivers & optimizations                        |Active Dev      |-                                  |
|**<scripts/>**          |Build, test, and deployment automation                           |-               |-                                  |
|**<tests/>**            |Reproducible test vectors & benchmarks                           |-               |-                                  |
|**<docs/>**             |Architecture specs, privacy model, security audits               |-               |[Index](docs/README.md)            |

-----

## 🎯 WHY KHAWRIZM EXISTS

### The Lock-In Cycle (The Problem)

```text
Free Tier → Data Gravity → Architectural Dependency → Price Monopoly → Hostage Status
```

Every “convenience” is a leash. Khawrizm breaks this cycle at **Layer 0**:

|Layer           |Centralized Model           |Khawrizm Sovereign Alternative |
|----------------|----------------------------|-------------------------------|
|**Compute**     |Cloud VMs (AWS, GCP)        |RK3588 / ESP32-S3 edge nodes   |
|**Network**     |ISP + Cell Tower            |TVWS + SIDSense + LPI/PPM      |
|**Consensus**   |Paxos/Raft (leader-based)   |Delta CRDTs (leaderless)       |
|**Security**    |RSA/ECC (quantum-vulnerable)|ML-KEM 768 + AES-256-GCM hybrid|
|**Intelligence**|API-bound LLMs              |Niyah Engine (local intent)    |
|**Sensing**     |Cloud-mandatory cameras     |ECHOWALL (on-device CSI)       |

**Design Mandate**:
*If it cannot run offline on a $35 board, it does not belong in the stack.*

-----

## 🚀 QUICK START

### Prerequisites

- Python 3.10+
- ESP32-S3 dev board (~$5) — optional for ECHOWALL
- RK3588 board or x86_64 Linux/macOS
- Git + ESP-IDF (for firmware)

### Clone the Monorepo

```bash
git clone https://github.com/Khawrzm/echowall
cd echowall
```

### Install Core Dependencies

```bash
# Install Python packages (zero cloud dependencies)
pip install -e .

# Verify sovereign mode
khawrizm check --offline
# → "All systems operational. Zero external API calls detected."
```

### Run ECHOWALL (Through-Wall Sensing)

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py build flash monitor
# The board is now a passive radar. No router changes. No cloud account.
```

### Initialize Niyah Engine (Sovereign AI)

```python
from niyah import NiyahEngine

engine = NiyahEngine(
    config_path="config/sovereign.yaml",
    root_db="data/arabic_roots.json",
    enable_telemetry=False
)

intent = engine.process("ابني نظام استشعار")
# → Extracts: ب-ن-ي (BUILD) + ش-ع-ر (SENSE)
```

-----

## 🔬 KEY CAPABILITIES (HONEST NUMBERS)

|Capability                       |Component   |Accuracy|Status|Notes                  |
|---------------------------------|------------|--------|------|-----------------------|
|**Human presence (through wall)**|ECHOWALL    |~94% F1 |Beta  |ESP32-S3, 60 m²        |
|**Occupancy count (1–4 people)** |ECHOWALL    |~87%    |Beta  |                       |
|**Posture / Fall detection**     |ECHOWALL    |~81%    |Beta  |⚠️ NOT for life-safety  |
|**Spectrum classification**      |SIDSense    |94.2%   |Stable|23ms latency (RK3588)  |
|**Root extraction**              |SARC (Niyah)|99.8%   |Stable|Arabic trilateral roots|
|**Intent graph convergence**     |Delta CRDT  |100%    |Stable|Mathematical guarantee |


> **⚠️ SAFETY DISCLAIMER:** Fall detection is NOT certified for critical life-safety emergencies. See <SECURITY.md> for full disclaimers.

-----

## 🔐 SOVEREIGNTY GUARANTEES

### Hard Constraints (Not Policies)

**1. Zero External API Calls**

```bash
grep -r 'requests.post\|httpx\|openai\|anthropic' .
# → Returns nothing in sovereign mode
```

**2. Zero Telemetry by Physics**

- All data processed and discarded on-device
- No `upload_to_cloud()` function exists
- Verify: `grep -r 'upload_to_cloud\|analytics\|telemetry' .`

**3. Privacy by Physics, Not Policy**

- Galois LFSR adversarial jitter (ECHOWALL)
- LPI/PPM below thermal noise floor (TVWS)
- Semantic output only (no raw waveforms)

**4. Post-Quantum Security**

- ML-KEM 768 (NIST FIPS 203) for key exchange
- AES-256-GCM for data streams
- Hybrid handshake: pay quantum tax once, accelerate forever

**5. Offline-First Consensus**

- Delta CRDTs (Conflict-Free Replicated Data Types)
- No central leader required
- Edit locally, sync eventually, converge mathematically

-----

## 🌍 TARGET USE CASES

### Industrial & HRI (Europe)

- **Human-Robot Collaboration**: Invisible safety zones without cameras
- **GDPR-Compliant Automation**: Zero PII collection by architecture
- **Secure Facility Monitoring**: No exploitable video streams

### Elder Care (Privacy-First)

- Fall detection without cloud-mandatory devices
- Local inference, no subscription fees
- Semantic alerts only (`{"fall_detected": true}`)

### Sovereign Communities

- Mesh networking via TV white spaces
- Offline-first collaboration tools
- Community-owned infrastructure (DePIN)

### Research & Academia

- Reproducible CSI sensing test vectors
- Open post-quantum cryptography implementations
- Arabic NLP via morphological grounding

-----

## 🗺️ ROADMAP (PHASED DEPLOYMENT)

```text
Phase 0 (Now): Cash-flow via consulting → fund R&D, no VC dilution
Phase 1 ($5K): T000 Tactical Node (RK3588 + LimeSDR) → single-node proof
Phase 2 ($50K): 100-node urban mesh → validate CRDT convergence + LPI stealth
Phase 3 ($500K): DePIN token launch (burn/mint equilibrium) → community incentives
Phase 4 ($50M): HAPS stratospheric layer (20km balloons + 10Gbps FSO lasers) → conditional
```

**Gatekeeping Principle**: *No phase proceeds without empirical validation of the prior.*
Phase 4 is a horizon, not a promise. See <ROADMAP.md> for details.

-----

## 🤝 CONTRIBUTING (SOVEREIGN MODE)

### Principles

1. **No Cloud Dependencies** — All imports must resolve to local packages. No `pip install` from PyPI in production code.
1. **Arabic-First Design** — Root extraction prioritizes Arabic morphology. Cross-lingual adapters are secondary.
1. **Intent Over Tokens** — Operate on logical predicates, not strings. No probabilistic text generation.
1. **Reproducibility** — All claims must include test vectors. Environmental variables documented.

### How to Contribute

```bash
# 1. Fork on GitHub: https://github.com/Khawrzm/echowall/fork

# 2. Clone locally
git clone https://github.com/YOUR_USERNAME/echowall
cd echowall

# 3. Create feature branch
git checkout -b feat/add-persian-root-extraction

# 4. Run pre-commit hooks
pre-commit install
pre-commit run --all-files

# 5. Submit PR with:
#    - Test vectors
#    - Zero external API verification
#    - Test coverage ≥ 85%
```

See <CONTRIBUTING.md> for full guidelines.

-----

## 📚 DOCUMENTATION

|Document                      |Purpose                                             |
|------------------------------|----------------------------------------------------|
|**<BLACKPAPER_v6.0-draft.md>**|Complete architectural thesis (15–20 pages)         |
|**<ROADMAP.md>**              |Phased deployment strategy & regulatory pathway     |
|**<SECURITY.md>**             |Security model, disclaimers, vulnerability reporting|
|**<CONTRIBUTING.md>**         |Contribution guidelines & sovereignty principles    |
|**<docs/>**                   |Architecture specs, privacy model, API references   |
|**<tests/>**                  |Reproducible benchmarks & test vectors              |

-----

## 🧪 TESTING & REPRODUCIBILITY

### Run Full Test Suite

```bash
# Unit tests (no external dependencies)
pytest tests/unit/ -v

# Integration tests (requires hardware)
pytest tests/integration/ -v --echowall-port=/dev/ttyUSB0

# Reproduce Blackpaper claims
pytest tests/blackpaper_v6/ -v

# Benchmark performance
khawrizm benchmark --full
```

### Test Coverage

|Component              |Coverage|Critical Tests                                   |
|-----------------------|--------|-------------------------------------------------|
|**ECHOWALL Sensing**   |91%     |`test_csi_extraction.py`, `test_lfsr_jitter.py`  |
|**SARC Root Extractor**|94%     |`test_triliteral_mapping.py`                     |
|**Delta CRDT**         |89%     |`test_convergence.py`, `test_delta_sync.py`      |
|**PQC Hybrid**         |87%     |`test_handshake.py`, `test_quantum_resistance.py`|

-----

## 🔍 INTEGRITY VERIFICATION

```bash
# Verify zero external API calls across entire codebase
grep -r 'requests.post\|httpx.post\|openai\|anthropic\|google.generativeai' . \
  --exclude-dir=.git --exclude-dir=node_modules
# → Should return nothing in sovereign mode

# Verify Blackpaper integrity
sha256sum BLACKPAPER_v6.0-draft.md
# Expected: [TO BE GENERATED UPON FINALIZATION]

# Run full sovereignty audit
khawrizm audit --full
# → Checks: no telemetry, no cloud deps, hardware compatibility
```

-----

## 📜 LICENSE

**Apache 2.0** — Audit it. Fork it. Ship it. Don’t weaponize it.
See <LICENSE> for full terms.

**Special Clause**:
If you use Khawrizm in a system that:

- Uploads user data to cloud without explicit consent
- Wraps proprietary APIs and calls it “innovation”
- Violates the sovereignty principles outlined in the Blackpaper

You are in violation of the spirit of this license, even if not the letter.

-----

## 🎓 ACADEMIC CITATION

```bibtex
@software{alshammari2026khawrizm,
  author       = {Alshammari, Sulaiman},
  title        = {Khawrizm: Sovereign Infrastructure as Compiled Physics},
  year         = {2026},
  version      = {6.0-draft},
  url          = {https://github.com/Khawrzm/echowall},
  note         = {Includes ECHOWALL, Niyah Engine, HavenOS, SIDSense}
}
```

See <CITATION.cff> for full citation metadata.

-----

## 🧭 PHILOSOPHY

> *“For the last 20 years, we have all been renting our digital lives from digital landlords.*
> *We’ve accepted that the rug can be pulled at any time.*
> *But what if you could just own the land?”*

**Khawrizm is not a startup.**
It is a *compiled argument* that digital sovereignty is:

- ✅ Physically possible (Wi-Fi CSI, TVWS, LPI/PPM)
- ✅ Mathematically verifiable (CRDTs, PQC, LFSR)
- ✅ Economically bootstrappable (DePIN, burn/mint equilibrium)

**The Sovereign Stack**:

```text
[Physical Layer]      ESP32-S3, RK3588, LimeSDR
       ↓
[Sensing Layer]       ECHOWALL (CSI + acoustic)
       ↓
[Intent Layer]        Niyah Engine (SARC + 3-lobe)
       ↓
[Consensus Layer]     Delta CRDTs (leaderless)
       ↓
[Network Layer]       TVWS + SIDSense + LPI/PPM
       ↓
[Stratospheric]       HAPS + FSO Lasers (Phase 4)
```

Every layer operates locally.
Every layer respects sovereignty.
Every layer returns home.

-----

## ⚡ SYSTEM STATUS

```text
[✓] ECHOWALL firmware (ESP32-S3)    — Beta v0.2.0
[✓] Niyah Engine (SARC + 3-lobe)   — Alpha v0.1.0
[✓] HavenOS (Microkernel)           — Proof-of-Concept
[✓] SIDSense (TVWS CNN)             — Stable
[✓] Delta CRDT (Consensus)          — Stable
[✓] PQC Hybrid (ML-KEM 768)         — Stable
[✓] Blackpaper v6.0-draft           — Public for Peer Review
[ ] Phase 4 (HAPS + FSO)            — Horizon Goal

Sovereignty Status:   OPERATIONAL
Cloud Dependencies:   ZERO
Telemetry:            DISABLED
Algorithm:            RETURNS HOME
```

-----

## 📧 CONTACT & OUTREACH

**For European Deep-Tech Investors**:
Regulatory arbitrage via engineering. Target CEPT Band 20 regions for TVWS deployment.
→ [GitHub Issues](https://github.com/Khawrzm/echowall/issues) (technical inquiries only)

**For Systems Engineers**:
All repos open-source. Reproduce our claims. Stress-test our math.
→ [Discussions](https://github.com/Khawrzm/echowall/discussions)

**For Philosophers of Technology**:
This is sovereignty, not convenience.
→ <BLACKPAPER_v6.0-draft.md>

-----

<sub>Built by <a href="https://github.com/Khawrzm">Sulaiman Alshammari</a> in Riyadh, Saudi Arabia.</sub>
<sub>Started: 730 days ago. Status: v6.0-draft. Sovereignty: Non-negotiable.</sub>
<sub><em>“They built cages of convenience and called them clouds. We built keys of mathematics and called them freedom. Compile your own reality.”</em></sub>
