# Contributing to ECHOWALL

Thank you for your interest! ECHOWALL is an open research project — all contributions welcome.

## Areas of Focus

- **Hardware ports**: New CSI extraction targets (Qualcomm, MediaTek, AR9380)
- **Model improvements**: Better EchoNet weights, new training datasets
- **Acoustic fusion**: Improved FMCW processing, multi-microphone beamforming  
- **Privacy**: Novel adversarial perturbation schemes
- **Language support**: Arabic documentation is first-class — translations welcome
- **Home automation**: Home Assistant, openHAB, MQTT broker integrations

## Development Setup

```bash
git clone https://github.com/Khawrzm/echowall
cd echowall
pip install -e ".[dev]"
pre-commit install
```

## Running Tests

```bash
pytest tests/ -v
```

## Code Style

```bash
ruff check .
mypy echowall/
```

## Pull Request Guidelines

1. Fork the repo and create a feature branch
2. Add tests for new functionality
3. Ensure `pytest` and `ruff` pass
4. Open a PR with a clear description

## Research Ethics

ECHOWALL is designed for **safety and humanitarian applications**:
- Fall detection for elderly care
- Emergency responder situational awareness  
- Home intrusion detection (privacy-preserving)
- Building occupancy optimization

Contributions intended for surveillance or privacy violation will not be accepted.

## License

All contributions are licensed under Apache 2.0.
