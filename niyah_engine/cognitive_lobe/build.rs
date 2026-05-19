//! build.rs — Compile and statically link the C11 Casper Engine.
//!
//! Invoked automatically by `cargo build`. Compiles:
//!   casper_engine/src/tensor.c
//!   casper_engine/src/sarf.c
//!   casper_engine/src/casper_api.c
//!
//! into a static library and links it into the `cognitive_lobe` crate.
//!
//! ## Path resolution
//! This build script lives at:
//!   niyah_engine/cognitive_lobe/build.rs
//!
//! CARGO_MANIFEST_DIR at build time:
//!   <repo_root>/niyah_engine/cognitive_lobe
//!
//! Therefore casper_engine is at:
//!   <repo_root>/casper_engine
//!   = CARGO_MANIFEST_DIR/../../casper_engine
//!
//! ## Target-specific flags
//! On AArch64 targets (RK3588, Cortex-A76) we add:
//!   -march=armv8.2-a+fp16+simd  (enables NEON + FP16)
//!   -DHAVE_NEON=1
//!
//! On all other targets (host x86_64 for CI/tests) we use:
//!   -DHAVE_NEON=0
//! which activates the portable C11 fallback path in tensor.c.

use std::env;
use std::path::PathBuf;

fn main() {
    // -----------------------------------------------------------------------
    // Locate casper_engine source directory.
    // -----------------------------------------------------------------------
    let manifest_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set"),
    );
    // cognitive_lobe -> niyah_engine -> repo_root -> casper_engine
    let casper_src = manifest_dir
        .join("..")
        .join("..")
        .join("casper_engine")
        .join("src");

    // -----------------------------------------------------------------------
    // Detect build target architecture.
    // -----------------------------------------------------------------------
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let is_aarch64 = target_arch == "aarch64";

    // -----------------------------------------------------------------------
    // Build Casper Engine C sources.
    // -----------------------------------------------------------------------
    let mut build = cc::Build::new();

    build
        // C11 standard — no extensions.
        .std("c11")
        // Full optimization — mandatory for NEON throughput.
        .opt_level(3)
        // Strict warnings.
        .warnings(true)
        .extra_warnings(true)
        .flag_if_supported("-Werror=implicit-function-declaration")
        .flag_if_supported("-Werror=incompatible-pointer-types")
        // Freestanding / embedded — no libc assumptions beyond stdint/stddef.
        .flag_if_supported("-ffreestanding")
        // Dead-code elimination.
        .flag_if_supported("-ffunction-sections")
        .flag_if_supported("-fdata-sections")
        // Include path for cross-file headers.
        .include(&casper_src);

    // AArch64-specific: enable ARMv8.2-A with NEON + FP16.
    if is_aarch64 {
        build
            .flag("-march=armv8.2-a+fp16+simd")
            .define("HAVE_NEON", "1");
    } else {
        // Host (x86_64) build: portable C11 fallback, ASAN/UBSAN in debug.
        build.define("HAVE_NEON", "0");
        if env::var("PROFILE").as_deref() == Ok("debug") {
            build
                .flag_if_supported("-fsanitize=address,undefined")
                .flag_if_supported("-fno-omit-frame-pointer");
        }
    }

    // Compile sources.
    build
        .file(casper_src.join("tensor.c"))
        .file(casper_src.join("sarf.c"))
        .file(casper_src.join("casper_api.c"))
        // Output library name: libcasper.a
        .compile("casper");

    // -----------------------------------------------------------------------
    // Linker instructions.
    // -----------------------------------------------------------------------
    // cc::Build::compile() already emits cargo:rustc-link-lib=static=casper,
    // but we emit it explicitly for clarity and IDE tooling.
    println!("cargo:rustc-link-lib=static=casper");

    // -----------------------------------------------------------------------
    // Re-run triggers: rebuild if any C source or header changes.
    // -----------------------------------------------------------------------
    println!(
        "cargo:rerun-if-changed={}",
        casper_src.join("tensor.c").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        casper_src.join("tensor.h").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        casper_src.join("sarf.c").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        casper_src.join("sarf.h").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        casper_src.join("casper_api.c").display()
    );
}
