#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case)]
mod ffi { include!(concat!(env!("OUT_DIR"), "/obs.rs")); }
mod inference;
mod filter;
use ffi::*;
use std::ffi::c_char;
#[no_mangle]
pub extern "C" fn obs_module_set_pointer(_: *mut obs_module_t) {}
#[no_mangle]
pub extern "C" fn obs_module_ver() -> u32 { LIBOBS_API_VER }
#[no_mangle]
pub extern "C" fn obs_module_name() -> *const c_char { c"Simulcast Background Removal".as_ptr() }
#[no_mangle]
pub extern "C" fn obs_module_load() -> bool { unsafe { filter::register(); } true }
