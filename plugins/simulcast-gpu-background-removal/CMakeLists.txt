# Build Rust cdylib against this exact libobs; no external OBS SDK or runtime.
find_program(SIMULCAST_CARGO cargo REQUIRED)
set(_cargo_dir "${CMAKE_CURRENT_BINARY_DIR}/cargo")
set(_module "${CMAKE_CURRENT_BINARY_DIR}/simulcast-gpu-background-removal.so")
file(GLOB_RECURSE _rust_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rs" "${CMAKE_CURRENT_SOURCE_DIR}/data/*.effect")
add_custom_command(OUTPUT "${_module}"
  COMMAND "${CMAKE_COMMAND}" -E env
    "OBS_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/libobs"
    "OBS_CONFIG_DIR=${CMAKE_BINARY_DIR}/config"
    "OBS_LIB_DIR=$<TARGET_FILE_DIR:libobs>"
    "CARGO_TARGET_DIR=${_cargo_dir}"
    "${SIMULCAST_CARGO}" build --release --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_cargo_dir}/release/libsimulcast_gpu_background_removal.so" "${_module}"
  DEPENDS libobs ${_rust_sources} Cargo.toml build.rs
  VERBATIM)
add_custom_target(simulcast-gpu-background-removal ALL DEPENDS "${_module}")
install(FILES "${_module}" DESTINATION "${OBS_PLUGIN_DESTINATION}/core" COMPONENT Runtime)
install(FILES README.md GPL-3.0-or-later.txt Apache-2.0.txt DESTINATION "${OBS_DATA_DESTINATION}/obs-modules/core/simulcast-gpu-background-removal" COMPONENT Runtime)
