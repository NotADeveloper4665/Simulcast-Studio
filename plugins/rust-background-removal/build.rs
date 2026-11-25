fn main() {
    let include = std::env::var("OBS_INCLUDE_DIR").expect("Set OBS_INCLUDE_DIR to installed libobs headers");
    let lib = std::env::var("OBS_LIB_DIR").expect("Set OBS_LIB_DIR to libobs library directory");
    println!("cargo:rustc-link-search=native={lib}");
    println!("cargo:rustc-link-lib=obs");
    println!("cargo:rerun-if-env-changed=OBS_INCLUDE_DIR");
    println!("cargo:rerun-if-env-changed=OBS_LIB_DIR");
    bindgen::Builder::default()
        .header_contents("wrapper.h", "#include <obs-module.h>\nenum { SIMULCAST_LIBOBS_API_VER = LIBOBS_API_VER };")
        .clang_arg(format!("-I{include}"))
        .prepend_enum_name(false)
        .allowlist_function("obs_.*|gs_.*")
        .allowlist_type("obs_.*|gs_.*|vec4")
        .allowlist_var("OBS_.*|GS_.*|SIMULCAST_.*")
        .generate().expect("libobs bindings")
        .write_to_file(std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("obs.rs")).unwrap();
}
