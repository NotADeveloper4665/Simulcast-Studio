#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case)]
#[allow(dead_code)]
mod ffi { include!(concat!(env!("OUT_DIR"), "/obs.rs")); }
mod inference;
mod filter;
use ffi::*;
use std::{ffi::c_char, sync::atomic::{AtomicPtr, Ordering}};
static MODULE: AtomicPtr<obs_module_t> = AtomicPtr::new(std::ptr::null_mut());
#[no_mangle]
pub extern "C" fn obs_module_set_pointer(module: *mut obs_module_t) { MODULE.store(module, Ordering::Release); }
#[no_mangle]
pub extern "C" fn obs_current_module() -> *mut obs_module_t { MODULE.load(Ordering::Acquire) }
#[no_mangle]
pub extern "C" fn obs_module_ver() -> u32 { SIMULCAST_LIBOBS_API_VER as u32 }
#[no_mangle]
pub extern "C" fn obs_module_name() -> *const c_char { c"Simulcast Background Removal".as_ptr() }
#[no_mangle]
pub extern "C" fn obs_module_description() -> *const c_char { c"Local ONNX webcam foreground segmentation".as_ptr() }
#[no_mangle]
pub extern "C" fn obs_module_load() -> bool { unsafe { filter::register(); } true }
