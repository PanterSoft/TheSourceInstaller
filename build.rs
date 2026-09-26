// Records the target triple so `tsi self-update` can ask for the release asset
// built for exactly this target. `cfg(target_arch)` alone can't tell armv6 from
// armv7, and both ship as separate release binaries.
fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    println!("cargo:rustc-env=TSI_BUILD_TARGET={target}");
    println!("cargo:rerun-if-changed=build.rs");
}
