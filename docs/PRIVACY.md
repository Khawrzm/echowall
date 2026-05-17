# Privacy-by-Physics

ECHOWALL is a sensor that can, in principle, observe people in their homes. We take that seriously. This document explains the threat model and the mechanisms that defend against it. If something here is wrong, weak, or marketing-flavored, open an issue.

---

## Threat model

We assume the following adversaries:

1. **Network eavesdropper.** Reads packets on the local network. Wants to reconstruct CSI or model outputs.
2. **Cloud operator.** A future hosted version could be tempted to ingest raw signal. We design so this is structurally impossible.
3. **Compromised node.** Attacker has root on the ECHOWALL device and tries to exfiltrate.
4. **Curious bystander.** Another Wi-Fi device on the same channel tries to passively learn what ECHOWALL learned.

---

## Mechanism 1 — Edge-only processing

Raw CSI is read, processed, and discarded inside the same event loop iteration. There is no buffer that survives the tick. There is no `upload()` call. There is no telemetry endpoint enabled by default.

Verify it yourself:

```bash
grep -rni "upload\|telemetry\|requests.post\|httpx.post" echowall/
```

If you find one we did not declare, that is a CVE. File it.

---

## Mechanism 2 — Adversarial jitter (Privacy-by-Physics)

Wi-Fi CSI is the *physics* of the channel. Anyone within radio range can in theory observe the same channel. To defeat a passive eavesdropper, ECHOWALL nodes inject a hardware-seeded perturbation into the outbound packets that they themselves transmit.

Properties:

- The perturbation is deterministic given a seed derived from a TPM/secure-element key.
- The local model knows the seed and inverts the perturbation.
- An eavesdropper without the seed sees a channel that looks 6–9 dB noisier and is statistically uncorrelated with body motion.

We do not claim information-theoretic privacy. We claim **practical** privacy: it makes off-the-shelf CSI tools useless against an ECHOWALL-protected channel.

Spec, ablations and known weaknesses live in [`privacy/JITTER_SPEC.md`](../privacy/JITTER_SPEC.md) (coming with v0.2).

---

## Mechanism 3 — Semantic-only output

The public API never exposes:

- Raw CSI tensors
- Doppler spectrograms
- Acoustic waveforms
- Per-subcarrier amplitudes or phases

It exposes JSON like:

```json
{ "presence": true, "count": 2, "posture": "seated", "bpm": 14, "confidence": 0.87 }
```

You cannot reconstruct a person from `{"presence": true}`. That is the point.

---

## Differential privacy in federated learning

If you opt into federated learning (off by default), your node uploads LoRA adapter deltas, not data. We DP-clip at `ε=2, δ=1e-6` per round. The privacy accountant ships with the runtime; run `echowall privacy-budget` to see your spend.

---

## What we do NOT defend against

We are not magicians. The following are out of scope:

- An attacker who physically owns your hardware can read whatever the model reads. Put your node behind a locked door.
- Side channels we do not yet know about. (If you find one, we will pay.)
- Misuse by the operator. ECHOWALL is a sensor. A bad actor with a sensor will do bad things. Use [`LICENSE`](../LICENSE) and your local laws.

---

## Bug bounty

We pay for working exploits that extract raw signal from a running ECHOWALL node or that demonstrably break the jitter scheme.

- **Critical** (raw CSI exfiltration): $1,000 + public credit
- **High** (signal reconstruction beyond semantic output): $500
- **Medium** (de-anonymization of federated updates): $250

Email `cartier403c@gmail.com` with PGP key on request. Please do not disclose publicly until we ship a patch.
---

## Auditing

We want third-party privacy audits. If you run one, we will publish the result here — good or bad.
