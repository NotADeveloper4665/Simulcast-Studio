fn main() {
    let include = std::env::var("OBS_INCLUDE_DIR").expect("OBS_INCLUDE_DIR");
    let config = std::env::var("OBS_CONFIG_DIR").expect("OBS_CONFIG_DIR");
    let lib = std::env::var("OBS_LIB_DIR").expect("OBS_LIB_DIR");
    for key in ["OBS_INCLUDE_DIR", "OBS_CONFIG_DIR", "OBS_LIB_DIR"] { println!("cargo:rerun-if-env-changed={key}"); }
    println!("cargo:rustc-link-search=native={lib}");
    println!("cargo:rustc-link-lib=dylib=obs");
    println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN/../..");
    bindgen::Builder::default()
        .header_contents("wrapper.h", "#include <obs-module.h>\nenum { SIMULCAST_LIBOBS_API_VER = LIBOBS_API_VER };")
        .clang_args([format!("-I{include}"), format!("-I{config}")])
        .prepend_enum_name(false)
        .allowlist_function("obs_.*|gs_.*|bfree")
        .allowlist_type("obs_.*|gs_.*|vec[234]")
        .allowlist_var("OBS_.*|GS_.*|SIMULCAST_.*")
        .layout_tests(false)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate().expect("generate exact host bindings")
        .write_to_file(std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("obs.rs")).unwrap();
}
