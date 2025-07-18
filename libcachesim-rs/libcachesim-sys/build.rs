use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed=src/helpers.c");

    // Find libCacheSim headers and libraries
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let project_root = PathBuf::from(&manifest_dir).parent().unwrap().parent().unwrap().to_path_buf();

    let mut include_paths = Vec::new();
    let mut lib_paths = Vec::new();
    let mut found_libcachesim = false;

    // Try pkg-config first
    if let Ok(lib) = pkg_config::probe_library("libcachesim") {
        for path in &lib.include_paths {
            include_paths.push(format!("-I{}", path.display()));
        }
        for path in &lib.link_paths {
            lib_paths.push(path.clone());
        }
        found_libcachesim = true;
        println!("Found libCacheSim via pkg-config");
    } else {
        // Fallback: look in project structure
        let libcachesim_include = project_root.join("libCacheSim").join("include");
        let libcachesim_build = project_root.join("_build");

        if libcachesim_include.exists() && libcachesim_build.exists() {
            include_paths.push(format!("-I{}", libcachesim_include.display()));

            // Add build directory for generated config.h
            let config_include = libcachesim_build.join("libCacheSim").join("include");
            if config_include.exists() {
                include_paths.push(format!("-I{}", config_include.display()));
            }

            // Link to the built library
            let lib_dir = libcachesim_build;
            if lib_dir.exists() {
                println!("cargo:rustc-link-search=native={}", lib_dir.display());
                lib_paths.push(lib_dir);
            }

            found_libcachesim = true;
            println!("Found libCacheSim in project structure");
        }
    }

    if !found_libcachesim {
        eprintln!("libCacheSim not found!");
        eprintln!("Please ensure libCacheSim is built and available via:");
        eprintln!("1. pkg-config (install libcachesim-dev package), or");
        eprintln!("2. Build libCacheSim in the parent directory");
        eprintln!("   cd .. && mkdir _build && cd _build && cmake -G Ninja .. && ninja");
        std::process::exit(1);
    }

    // Compile our helper C file
    let mut cc_build = cc::Build::new();
    cc_build.file("src/helpers.c");

    // Add include paths to cc
    for include_path in &include_paths {
        if let Some(path) = include_path.strip_prefix("-I") {
            cc_build.include(path);
        }
    }

    // Add glib include paths for cc
    if let Ok(glib_lib) = pkg_config::probe_library("glib-2.0") {
        for path in &glib_lib.include_paths {
            cc_build.include(path);
            include_paths.push(format!("-I{}", path.display()));
        }
    }

    cc_build.compile("libcachesim_helpers");

    // Link to libCacheSim and its dependencies
    println!("cargo:rustc-link-lib=libCacheSim");
    println!("cargo:rustc-link-lib=glib-2.0");
    println!("cargo:rustc-link-lib=pthread");

    // Link to zstd if available (required for compressed trace support)
    if pkg_config::probe_library("libzstd").is_ok() {
        println!("cargo:rustc-link-lib=zstd");
    } else {
        // Try common library names
        println!("cargo:rustc-link-lib=zstd");
    }

    // Generate bindings using the wrapper header
    let wrapper_path = PathBuf::from(&manifest_dir).join("wrapper.h");

    let mut builder = bindgen::Builder::default()
        .header(wrapper_path.to_str().unwrap())
        .derive_debug(true)
        .derive_default(true)
        .derive_copy(true)
        .derive_eq(true)
        .derive_partialeq(true)
        .use_core()
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Allowlist the types and functions we need
        .allowlist_type("cache")
        .allowlist_type("cache_t")
        .allowlist_type("reader")
        .allowlist_type("reader_t")
        .allowlist_type("request")
        .allowlist_type("request_t")
        .allowlist_type("common_cache_params_t")
        .allowlist_type("reader_init_param_t")
        .allowlist_type("cache_stat_t")
        .allowlist_type("trace_type_e")
        .allowlist_type("req_op_e")
        .allowlist_type("obj_id_t")
        .allowlist_type("trace_format_e")
        .allowlist_type("cache_obj_t")
        .allowlist_type("sampler_t")
        .allowlist_type("admissioner")
        .allowlist_type("admissioner_t")
        // Core cache functions
        .allowlist_function("cache_struct_init")
        .allowlist_function("cache_struct_free")
        .allowlist_function("cache_get_base")
        .allowlist_function("cache_insert_base")
        .allowlist_function("cache_remove_obj_base")
        .allowlist_function("cache_can_insert_default")
        // Reader functions
        .allowlist_function("setup_reader")
        .allowlist_function("read_one_req")
        .allowlist_function("reset_reader")
        .allowlist_function("close_reader")
        .allowlist_function("get_num_of_req")
        // Helper functions from our C file
        .allowlist_function("libcachesim_new_request")
        .allowlist_function("libcachesim_free_request")
        .allowlist_function("libcachesim_copy_request")
        .allowlist_function("libcachesim_clone_request")
        .allowlist_function("libcachesim_default_common_cache_params")
        .allowlist_function("libcachesim_default_reader_init_params")
        .allowlist_function("libcachesim_set_default_reader_init_params")
        .allowlist_function("libcachesim_cache_get_occupied_byte_default")
        .allowlist_function("libcachesim_cache_get_n_obj_default")
        .allowlist_function("libcachesim_cache_get_reference_time")
        // Constants and variables
        .allowlist_var("g_trace_type_name")
        .allowlist_var("req_op_str")
        // Blocklist problematic platform-specific types
        .blocklist_type("FILE")
        .blocklist_type("fpos_t")
        .blocklist_type("__.*")
        .blocklist_type("_.*")
        .blocklist_function("_.*")
        // Blocklist reader struct since it contains FILE*
        .blocklist_type("reader")
        .opaque_type("reader")
        .opaque_type("reader_t")
        // Add layout tests for important types
        .layout_tests(false)
        // Handle platform-specific issues
        .clang_arg("-Wno-everything")
        // Generate constants as enums for better type safety
        .rustified_enum("trace_type_e")
        .rustified_enum("req_op_e")
        .rustified_enum("trace_format_e");

    // Add include paths to bindgen
    for include_path in &include_paths {
        builder = builder.clang_arg(include_path);
    }

    let bindings = builder
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    println!("Generated FFI bindings successfully");
}
