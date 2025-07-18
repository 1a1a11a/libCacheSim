use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    // Try to find libCacheSim using pkg-config first
    if let Ok(lib) = pkg_config::probe_library("libcachesim") {
        // Found via pkg-config, use those settings
        for path in &lib.include_paths {
            println!("cargo:include={}", path.display());
        }
        return;
    }

    // Fallback: look for libCacheSim in the parent directory
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let project_root = PathBuf::from(&manifest_dir).parent().unwrap().to_path_buf();

    // Check if we're in the libCacheSim project structure
    let libcachesim_include = project_root.join("libCacheSim").join("include");
    let libcachesim_build = project_root.join("_build");

    if libcachesim_include.exists() {
        println!("cargo:include={}", libcachesim_include.display());

        // Look for built library
        if libcachesim_build.exists() {
            let lib_path = libcachesim_build.join("lib");
            if lib_path.exists() {
                println!("cargo:rustc-link-search=native={}", lib_path.display());
                println!("cargo:rustc-link-lib=cachesim");
            }
        }
    } else {
        // Print helpful error message
        eprintln!("libCacheSim not found!");
        eprintln!("Please ensure libCacheSim is built and available via:");
        eprintln!("1. pkg-config (install libcachesim-dev package), or");
        eprintln!("2. Build libCacheSim in the parent directory");
        eprintln!("   cd .. && mkdir _build && cd _build && cmake -G Ninja .. && ninja");
        std::process::exit(1);
    }

    // Link required system libraries
    println!("cargo:rustc-link-lib=glib-2.0");
    println!("cargo:rustc-link-lib=pthread");

    // Optional libraries that might be needed
    // Note: zstd linking will be handled automatically by pkg-config or system detection
}
