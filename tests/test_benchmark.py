"""Offline benchmark unit tests — Issue #1 acceptance criteria.

These tests verify the benchmark pipeline runs end-to-end on the
bundled synthetic dataset without any network calls or hardware.

Radical Honesty note: tests use deterministic synthetic data
(numpy seed=42), NOT real RF measurements.
"""
from __future__ import annotations

import pathlib
import json
import csv

import pytest


DATA_DIR = pathlib.Path(__file__).parent / "data"
MODEL_CACHE = pathlib.Path.home() / ".echowall" / "models"


def test_model_loader_seeds_offline(tmp_path, monkeypatch):
    """get_model_path() must succeed 100% offline (no network)."""
    # Redirect cache to a temp dir so we don't pollute the real cache.
    import echowall.model_loader as ml
    monkeypatch.setattr(ml, "_CACHE_DIR", tmp_path)

    # Block all network access to prove offline fallback works.
    import urllib.request

    def _no_network(url, **kwargs):  # noqa: ARG001
        raise OSError("Network blocked in test")

    monkeypatch.setattr(urllib.request, "urlopen", _no_network)

    path = ml.get_model_path("echonet-v1")
    assert path.exists(), "Seeded model file must exist after get_model_path()"

    data = json.loads(path.read_text())
    assert "weights" in data
    assert "architecture" in data
    assert data["architecture"]["input_dim"] == 640


def test_model_loader_sha256_integrity(tmp_path, monkeypatch):
    """SHA-256 sidecar must be written and must match file contents."""
    import echowall.model_loader as ml
    import hashlib
    monkeypatch.setattr(ml, "_CACHE_DIR", tmp_path)

    import urllib.request
    monkeypatch.setattr(urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(OSError("blocked")))

    path = ml.get_model_path("echonet-v1")
    meta_path = path.with_suffix(".meta.json")
    assert meta_path.exists(), "Sidecar .meta.json must be written"

    meta = json.loads(meta_path.read_text())
    assert "sha256" in meta

    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    assert actual == meta["sha256"], "Sidecar SHA-256 must match file contents"


def test_download_weights_cli(tmp_path, monkeypatch):
    """echowall download-weights must exit 0 and report a valid model path."""
    import echowall.model_loader as ml
    monkeypatch.setattr(ml, "_CACHE_DIR", tmp_path)

    import urllib.request
    monkeypatch.setattr(urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(OSError("blocked")))

    from typer.testing import CliRunner
    from echowall.cli import app

    runner = CliRunner()
    result = runner.invoke(app, ["download-weights"])
    assert result.exit_code == 0, f"download-weights exited {result.exit_code}:\n{result.output}"
    assert "Model ready" in result.output


def test_benchmark_synthetic_dataset_exists():
    """Bundled synthetic dataset must exist for offline benchmark to work.

    If this test fails, run the generate-dataset workflow and merge the
    resulting PR to populate tests/data/sample_csi_fall.csv.
    """
    csv_path = DATA_DIR / "sample_csi_fall.csv"
    if not csv_path.exists():
        pytest.skip(
            "tests/data/sample_csi_fall.csv not yet generated. "
            "Run: Actions → Generate Synthetic CSI Dataset (HITL) → merge PR."
        )

    with csv_path.open(newline="") as fh:
        reader = csv.reader(fh)
        header = next(reader)
        rows = list(reader)

    assert header[0] == "label", "First column must be 'label'"
    assert len(rows) >= 8, "Dataset must have at least 8 rows (2 per class)"
    assert len(header) == 641, "Must have 640 features + 1 label column"
