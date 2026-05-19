# Casper Engine

C11 hybrid neuro-symbolic inference engine for the Niyah Engine Cognitive Lobe.

## What it is

A zero-dependency, zero-heap, zero-telemetry static library (`libcasper.a`) that provides:

1. **INT8 MLP inference** — 3-layer network (640→64→64→8) with ARM NEON SIMD acceleration
2. **Arabic Sarf morphology** — deterministic triliteral root extraction (2,976 forms)
3. **FFI API** — exact C-linkage surface required by `cognitive_lobe/src/casper_bridge.rs`

## Build

```bash
# Native AArch64 (on RK3588 or cross-compiled):
make

# Cross-compile from x86_64:
make CC=aarch64-linux-gnu-gcc

# Host build for unit tests (disables NEON, enables ASAN/UBSAN):
make test ARCH=host

# Output:
libcasper.a   (~50 KB stripped)
```

## Linking with Rust

In `cognitive_lobe/build.rs` (to be created at M4):

```rust
println!("cargo:rustc-link-search=native=../../casper_engine");
println!("cargo:rustc-link-lib=static=casper");
```

## Model buffer layout

The `casper_init(model_buf, model_len)` function expects:

| Offset | Size | Content |
|---|---|---|
| 0 | 40,960 | Encoder weights [640×64] INT8 |
| 40,960 | 4,096 | Hidden weights [64×64] INT8 |
| 45,056 | 512 | Output weights [64×8] INT8 |
| **Total** | **45,568** | **minimum** |

The weights from `echowall download-weights` are compatible with this layout.

## Sarf — Radical Honesty

The triliteral root extraction in `sarf.c` is a structural approximation.
It correctly handles simple triliteral verb stems after prefix/suffix stripping.
Complex derived forms (broken plurals, Form VII–X with gemination) require
a full Buckwalter morphological database. The deterministic form_index hash
is reproducible but not linguistically precise for all 2,976 forms.

## Architecture constraints

- `#include <arm_neon.h>` only when `HAVE_NEON=1` (AArch64 builds)
- No `malloc`, no `free`, no `printf`, no file I/O
- All intermediate buffers are stack-allocated
- Model weights are pointer-aliased from the caller's buffer (zero-copy)
- Re-entrant: no global mutable state after `casper_init` completes
  (single-owner architecture; concurrent calls are undefined behaviour
  by design — the sovereign architecture has one Executive)
