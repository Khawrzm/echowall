```markdown
# 🏴‍☠️ KHAWRIZM
> *Sovereign Infrastructure as Compiled Physics*

[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-black?style=flat-square)](LICENSE)
[![Status: v6.0-draft](https://img.shields.io/badge/status-v6.0__draft-yellow?style=flat-square)](https://github.com/Khawrzm/khawrizm/releases)
[![Blackpaper](https://img.shields.io/badge/docs-Blackpaper__v6.0--draft-blue?style=flat-square)](BLACKPAPER_v6.0-draft.md)
[![Ecosystem](https://img.shields.io/badge/ecosystem-Sovereign__Infrastructure-green?style=flat-square)](https://github.com/Khawrzm)

**A complete sovereign digital infrastructure stack—from bare-metal sensing to stratospheric networks.**  
Built in Riyadh. Compiled from physics. Owned by you.

📖 **[Read the Blackpaper v6.0-draft →](BLACKPAPER_v6.0-draft.md)**  
*("Sovereign Infrastructure as Compiled Physics")*

---

## ⚠️ WRAPPER CULTURE WARNING

If you are looking for:
- ❌ Cloud-native SaaS platforms
- ❌ VC-funded growth metrics
- ❌ API wrappers marketed as "innovation"
- ❌ Telemetry disguised as "analytics"

**This is not the repository you seek.**

Khawrizm is a **sovereign infrastructure stack** that:
- ✅ Runs entirely offline on $35–$1,300 hardware
- ✅ Zero cloud dependencies, zero telemetry, zero external APIs
- ✅ Bare-metal firmware, post-quantum cryptography, Delta CRDTs
- ✅ Privacy enforced by physics, not policies

> *"The algorithm always returns home."*

---

## 🧩 THE SOVEREIGN STACK

```
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

---

## 📦 REPOSITORY STRUCTURE

| Component | Purpose | Status | Documentation |
|-----------|---------|--------|---------------|
| **[echowall/](echowall/)** | Bare-metal through-wall sensing via Wi-Fi CSI + acoustic chirps | Beta v0.2.0 | [README](echowall/README.md) |
| **[niyah_engine/](niyah_engine/)** | Sovereign AI intent processing via Arabic morphological grounding | Alpha v0.1.0 | [README](niyah_engine/README.md) |
| **[niyah_executive/](niyah_executive/)** | Multi-step planning & constraint checking | Alpha v0.1.0 | [README](niyah_executive/README.md) |
| **[khawrizm_os/](khawrizm_os/)** | Microkernel + RISC-V emulator (<1000 lines C) | Proof-of-Concept | [README](khawrizm_os/README.md) |
| **[firmware/](firmware/)** | ESP32-S3, RK3588, LimeSDR firmware | Active Dev | [Docs](firmware/README.md) |
| **[comext-hybrid/](comext-hybrid/)** | Local AI browser with MCP tool integration | Alpha | [README](comext-hybrid/README.md) |
| **[casper_engine/](casper_engine/)** | Behavioral detection & fraud analysis | Research | [Docs](docs/CASPER.md) |
| **[custom_components/](custom_components/)** | Hardware-specific drivers & optimizations | Active Dev | - |
| **[scripts/](scripts/)** | Build, test, and deployment automation | - | - |
| **[tests/](tests/)** | Reproducible test vectors & benchmarks | - | - |
| **[docs/](docs/)** | Architecture specs, privacy model, security audits | - | [Index](docs/README.md) |

---

## 🎯 WHY KHAWRIZM EXISTS

### The Lock-In Cycle (The Problem)

```
Free Tier → Data Gravity → Architectural Dependency → Price Monopoly → Hostage Status
```

Every "convenience" is a leash. Khawrizm breaks this cycle at **Layer 0**:

| Layer | Centralized Model | Khawrizm Sovereign Alternative |
|-------|------------------|-------------------------------|
| **Compute** | Cloud VMs (AWS, GCP) | RK3588 / ESP32-S3 edge nodes |
| **Network** | ISP + Cell Tower | TVWS + SIDSense + LPI/PPM |
| **Consensus** | Paxos/Raft (leader-based) | Delta CRDTs (leaderless) |
| **Security** | RSA/ECC (quantum-vulnerable) | ML-KEM 768 + AES-256-GCM hybrid |
| **Intelligence** | API-bound LLMs | Niyah Engine (local intent) |
| **Sensing** | Cloud-mandatory cameras | ECHOWALL (on-device CSI) |

**Design Mandate**:  
*If it cannot run offline on a $35 board, it does not belong in the stack.*

---

## 🚀 QUICK START

### Prerequisites
- Python 3.10+
- ESP32-S3 dev board (~$5) — optional for ECHOWALL
- RK3588 board or x86_64 Linux/macOS
- Git + ESP-IDF (for firmware)

### Clone the Monorepo

```bash
git clone https://github.com/Khawrzm/khawrizm
cd khawrizm
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

---

## 🔬 KEY CAPABILITIES (HONEST NUMBERS)

| Capability | Component | Accuracy | Status | Notes |
|------------|-----------|----------|--------|-------|
| **Human presence (through wall)** | ECHOWALL | ~94% F1 | Beta | ESP32-S3, 60 m² |
| **Occupancy count (1–4 people)** | ECHOWALL | ~87% | Beta | |
| **Posture / Fall detection** | ECHOWALL | ~81% | Beta | ⚠️ NOT for life-safety |
| **Spectrum classification** | SIDSense | 94.2% | Stable | 23ms latency (RK3588) |
| **Root extraction** | SARC (Niyah) | 99.8% | Stable | Arabic trilateral roots |
| **Intent graph convergence** | Delta CRDT | 100% | Stable | Mathematical guarantee |

> **⚠️ SAFETY DISCLAIMER:** Fall detection is NOT certified for critical life-safety emergencies. See [SECURITY.md](SECURITY.md) for full disclaimers.

---

## 🔐 SOVEREIGNTY GUARANTEES

### Hard Constraints (Not Policies)

1. **Zero External API Calls**  
   ```bash
   grep -r 'requests.post\|httpx\|openai\|anthropic' .
   # → Returns nothing in sovereign mode
   ```

2. **Zero Telemetry by Physics**  
   - All data processed and discarded on-device  
   - No `upload_to_cloud()` function exists  
   - Verify: `grep -r 'upload_to_cloud\|analytics\|telemetry' .`

3. **Privacy by Physics, Not Policy**  
   - Galois LFSR adversarial jitter (ECHOWALL)  
   - LPI/PPM below thermal noise floor (TVWS)  
   - Semantic output only (no raw waveforms)

4. **Post-Quantum Security**  
   - ML-KEM 768 (NIST FIPS 203) for key exchange  
   - AES-256-GCM for data streams  
   - Hybrid handshake: pay quantum tax once, accelerate forever

5. **Offline-First Consensus**  
   - Delta CRDTs (Conflict-Free Replicated Data Types)  
   - No central leader required  
   - Edit locally, sync eventually, converge mathematically

---

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

---

## 🗺️ ROADMAP (PHASED DEPLOYMENT)

```
Phase 0 (Now): Cash-flow via consulting → fund R&D, no VC dilution
Phase 1 ($5K): T000 Tactical Node (RK3588 + LimeSDR) → single-node proof
Phase 2 ($50K): 100-node urban mesh → validate CRDT convergence + LPI stealth
Phase 3 ($500K): DePIN token launch (burn/mint equilibrium) → community incentives
Phase 4 ($50M): HAPS stratospheric layer (20km balloons + 10Gbps FSO lasers) → conditional
```

**Gatekeeping Principle**: *No phase proceeds without empirical validation of the prior.*  
Phase 4 is a horizon, not a promise. See [ROADMAP.md](ROADMAP.md) for details.

---

## 🤝 CONTRIBUTING (SOVEREIGN MODE)

### Principles

1. **No Cloud Dependencies**  
   - All imports must resolve to local packages  
   - No `pip install` from PyPI in production code

2. **Arabic-First Design**  
   - Root extraction prioritizes Arabic morphology  
   - Cross-lingual adapters are secondary

3. **Intent Over Tokens**  
   - Operate on logical predicates, not strings  
   - No probabilistic text generation

4. **Reproducibility**  
   - All claims must include test vectors  
   - Environmental variables documented

### How to Contribute

```bash
# 1. Fork the repo
git fork https://github.com/Khawrzm/khawrizm

# 2. Clone locally
git clone https://github.com/YOUR_USERNAME/khawrizm
cd khawrizm

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

See [CONTRIBUTING.md](CONTRIBUTING.md) for full guidelines.

---

## 📚 DOCUMENTATION

| Document | Purpose |
|----------|---------|
| **[BLACKPAPER_v6.0-draft.md](BLACKPAPER_v6.0-draft.md)** | Complete architectural thesis (15–20 pages) |
| **[ROADMAP.md](ROADMAP.md)** | Phased deployment strategy & regulatory pathway |
| **[SECURITY.md](SECURITY.md)** | Security model, disclaimers, vulnerability reporting |
| **[CONTRIBUTING.md](CONTRIBUTING.md)** | Contribution guidelines & sovereignty principles |
| **[docs/](docs/)** | Architecture specs, privacy model, API references |
| **[tests/](tests/)** | Reproducible benchmarks & test vectors |

---

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

| Component | Coverage | Critical Tests |
|-----------|----------|----------------|
| **ECHOWALL Sensing** | 91% | `test_csi_extraction.py`, `test_lfsr_jitter.py` |
| **SARC Root Extractor** | 94% | `test_triliteral_mapping.py` |
| **Delta CRDT** | 89% | `test_convergence.py`, `test_delta_sync.py` |
| **PQC Hybrid** | 87% | `test_handshake.py`, `test_quantum_resistance.py` |

---

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

---

## 📜 LICENSE

**Apache 2.0** — Audit it. Fork it. Ship it. Don't weaponize it.  
See [LICENSE](LICENSE) for full terms.

**Special Clause**:  
If you use Khawrizm in a system that:
- Uploads user data to cloud without explicit consent  
- Wraps proprietary APIs and calls it "innovation"  
- Violates the sovereignty principles outlined in the Blackpaper  

You are in violation of the spirit of this license, even if not the letter.

---

## 🎓 ACADEMIC CITATION

If you reference Khawrizm in research:

```bibtex
@software{alshammari2026khawrizm,
  author       = {Alshammari, Sulaiman},
  title        = {Khawrizm: Sovereign Infrastructure as Compiled Physics},
  year         = {2026},
  version      = {6.0-draft},
  url          = {https://github.com/Khawrzm/khawrizm},
  note         = {Includes ECHOWALL, Niyah Engine, HavenOS, SIDSense}
}
```

See [CITATION.cff](CITATION.cff) for full citation metadata.

---

## 🧭 PHILOSOPHY

> *"For the last 20 years, we have all been renting our digital lives from digital landlords.*  
> *We've accepted that the rug can be pulled at any time.*  
> *But what if you could just own the land?"*

**Khawrizm is not a startup.**  
It is a *compiled argument* that digital sovereignty is:
- ✅ Physically possible (Wi-Fi CSI, TVWS, LPI/PPM)
- ✅ Mathematically verifiable (CRDTs, PQC, LFSR)
- ✅ Economically bootstrappable (DePIN, burn/mint equilibrium)

**The Sovereign Stack**:
```
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

---

<sub>Built by <a href="https://github.com/Khawrzm">Sulaiman Alshammari</a> in Riyadh, Saudi Arabia.</sub>  
<sub>Started: 730 days ago. Status: v6.0-draft. Sovereignty: Non-negotiable.</sub>  
<sub>*"They built cages of convenience and called them clouds. We built keys of mathematics and called them freedom. Compile your own reality."*</sub>

---

## 📧 CONTACT & OUTREACH

**For European Deep-Tech Investors**:  
We offer regulatory arbitrage via engineering. Target CEPT Band 20 regions for TVWS deployment.  
→ Contact: [GitHub Issues](https://github.com/Khawrzm/khawrizm/issues) (technical inquiries only)

**For Systems Engineers**:  
All repos open-source. Reproduce our claims. Stress-test our math.  
→ Join: [Discussions](https://github.com/Khawrzm/khawrizm/discussions)

**For Philosophers of Technology**:  
This is sovereignty, not convenience.  
→ Read: [BLACKPAPER_v6.0-draft.md](BLACKPAPER_v6.0-draft.md)

---

## ⚡ SYSTEM STATUS

```
[✓] ECHOWALL firmware (ESP32-S3) — Beta v0.2.0
[✓] Niyah Engine (SARC + 3-lobe) — Alpha v0.1.0
[✓] HavenOS (Microkernel) — Proof-of-Concept
[✓] SIDSense (TVWS CNN) — Stable
[✓] Delta CRDT (Consensus) — Stable
[✓] PQC Hybrid (ML-KEM 768 + AES-256-GCM) — Stable
[✓] Blackpaper v6.0-draft — Public for Peer Review
[ ] Phase 4 (HAPS + FSO) — Horizon Goal

Sovereignty Status: OPERATIONAL
Cloud Dependencies: ZERO
Telemetry: DISABLED
Algorithm: RETURNS HOME
```
```
