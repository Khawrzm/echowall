# Security Policy

## Supported versions

| Version | Status |
|---------|--------|
| 0.1.x   | ✅ actively maintained |
| < 0.1   | ❌ unsupported |

## Reporting a vulnerability

**Please do not open a public issue.**

Email: `security@khawrizm.com`

Include:
- A description of the issue
- Steps to reproduce (ideally a minimal PoC)
- Affected version / commit hash
- Your name (or a handle) if you want credit

We aim to:
- Acknowledge within **48 hours**
- Triage within **7 days**
- Patch critical issues within **30 days**
- Publicly credit you after the fix ships, if you want

## In scope

- Raw-signal exfiltration from a running node
- Breaking the adversarial-jitter privacy guarantee
- De-anonymization of federated learning updates
- Authentication bypass on the REST/MQTT API
- Remote code execution in firmware or daemon
- Local privilege escalation

## Out of scope

- Physical attacks on the device
- Social engineering of maintainers
- DoS that requires saturating Wi-Fi spectrum
- Issues in third-party dependencies (report upstream)

## Bounty

See [`docs/PRIVACY.md`](docs/PRIVACY.md#bug-bounty) for current payouts. They are real and they are paid from our own pockets, not grant money.
