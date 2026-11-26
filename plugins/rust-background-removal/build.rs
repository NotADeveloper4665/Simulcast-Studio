fn main() {
    let include = std::env::var("OBS_INCLUDE_DIR").expect("Exact libobs headers required");
    let lib = std::env::var("OBS_LIB_DIR").expect("Exact libobs library required");
    let config = std::env::var("OBS_CONFIG_DIR").unwrap_or_else(|_| include.clone());
    println!("cargo:rustc-link-search=native={lib}");
    println!("cargo:rustc-link-lib=obs");
    for key in ["OBS_INCLUDE_DIR", "OBS_LIB_DIR", "OBS_CONFIG_DIR", "SIMULCAST_MODEL_PATH", "SIMULCAST_ORT_PATH"] {
        println!("cargo:rerun-if-env-changed={key}");
    }
    println!("cargo:rerun-if-changed=src/clear.c");
    let model = std::env::var("SIMULCAST_MODEL_PATH").expect("Pinned MediaPipe model required");
    println!("cargo:rerun-if-changed={model}");
    cc::Build::new().file("src/clear.c").include(&include).include(&config).compile("simulcast_graphics_shim");
    bindgen::Builder::default()
        .header_contents("wrapper.h", "#include <obs-module.h>\nenum { SIMULCAST_LIBOBS_API_VER = LIBOBS_API_VER };")
        .clang_arg(format!("-I{include}")).clang_arg(format!("-I{config}"))
        .prepend_enum_name(false)
        .allowlist_function("obs_.*|gs_.*")
        .allowlist_type("obs_source_info|obs_module_t|obs_source_t|obs_data_t|obs_properties_t|gs_.*_t")
        .allowlist_var("OBS_.*|GS_.*|SIMULCAST_.*")
        // These SDK types contain SIMD/array fields and are never dereferenced
        // in Rust. C owns the concrete vec4 used for graphics clears.
        .blocklist_type("^(vec2|vec3|vec4|obs_transform_info|obs_audio_data|obs_source_frame)$")
        .generate().expect("libobs bindings")
        .write_to_file(std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("obs.rs")).unwrap();
}
