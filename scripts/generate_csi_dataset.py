#!/usr/bin/env python3
"""Generate deterministic synthetic CSI dataset for EchoWall benchmarking.

Usage:
    python scripts/generate_csi_dataset.py [--seed 42] [--n-per-class 50]

Outputs:
    tests/data/sample_csi_fall.csv
    tests/data/sample_csi_fall.meta.json
"""
import argparse
import csv
import hashlib
import json
import pathlib

import numpy as np

DEFAULT_SEED = 42
DEFAULT_N_PER_CLASS = 50
N_SUBCARRIERS = 64
N_TIME_FRAMES = 10
N_FEATURES = N_SUBCARRIERS * N_TIME_FRAMES  # 640

CLASS_NAMES = {0: "empty", 1: "standing", 2: "sitting", 3: "fall"}


def generate(seed: int = DEFAULT_SEED, n_per_class: int = DEFAULT_N_PER_CLASS):
    rng = np.random.default_rng(seed=seed)
    labels = np.repeat([0, 1, 2, 3], n_per_class)
    rows = []

    for label in labels:
        base = rng.standard_normal(N_FEATURES).astype(np.float32)

        if label == 0:    # empty — low energy, flat spectrum
            csi = base * 0.3
        elif label == 1:  # standing — lower subcarriers elevated (torso reflection)
            csi = base * 0.8
            csi[:20 * N_TIME_FRAMES] *= 2.5
        elif label == 2:  # sitting — mid-band elevated (seated torso)
            csi = base * 0.7
            csi[20 * N_TIME_FRAMES:40 * N_TIME_FRAMES] *= 1.8
        else:             # fall — transient spike then rapid decay
            csi = base * 1.2
            csi[:5  * N_TIME_FRAMES] *= 4.0
            csi[5   * N_TIME_FRAMES:] *= 0.4

        # Quantize to INT8 [-128, 127] — matches ESP32-S3 TCN input format
        scale = max(abs(csi.max()), abs(csi.min())) / 127.0 or 1.0
        csi_int8 = np.clip(csi / scale, -128, 127).astype(np.int8)
        rows.append([int(label)] + csi_int8.tolist())

    return rows


def write(rows, seed, n_per_class):
    out_path = pathlib.Path("tests/data/sample_csi_fall.csv")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    header = ["label"] + [f"f{i}" for i in range(N_FEATURES)]
    with out_path.open("w", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow(header)
        writer.writerows(rows)

    sha256 = hashlib.sha256(out_path.read_bytes()).hexdigest()

    meta = {
        "dataset":      "sample_csi_fall",
        "seed":         seed,
        "rows":         len(rows),
        "n_per_class":  n_per_class,
        "features":     N_FEATURES,
        "classes":      CLASS_NAMES,
        "quantization": "INT8",
        "sha256":       sha256,
        "generated_by": "scripts/generate_csi_dataset.py",
        "note": "Deterministic synthetic simulation data. NOT real RF measurements.",
    }
    meta_path = out_path.with_suffix(".meta.json")
    meta_path.write_text(json.dumps(meta, indent=2))

    print(f"dataset : {out_path}")
    print(f"rows    : {len(rows)}  ({n_per_class} per class x 4 classes)")
    print(f"features: {N_FEATURES}  ({N_SUBCARRIERS} subcarriers x {N_TIME_FRAMES} frames)")
    print(f"seed    : {seed}")
    print(f"sha256  : {sha256}")
    print(f"size    : {out_path.stat().st_size:,} bytes")
    print(f"meta    : {meta_path}")
    return sha256


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate synthetic CSI dataset.")
    parser.add_argument("--seed",         type=int, default=DEFAULT_SEED)
    parser.add_argument("--n-per-class",  type=int, default=DEFAULT_N_PER_CLASS)
    args = parser.parse_args()
    rows = generate(seed=args.seed, n_per_class=args.n_per_class)
    write(rows, seed=args.seed, n_per_class=args.n_per_class)
