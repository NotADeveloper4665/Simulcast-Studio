#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case)]
#[allow(dead_code)]
mod ffi {
    // Pointer-only C types; never allocate or dereference these in Rust.
    #[repr(C)] pub struct vec2 { _opaque: [u8; 0] }
    #[repr(C)] pub struct vec3 { _opaque: [u8; 0] }
    #[repr(C)] pub struct vec4 { _opaque: [u8; 0] }
    #[repr(C)] pub struct obs_transform_info { _opaque: [u8; 0] }
    #[repr(C)] pub struct obs_audio_data { _opaque: [u8; 0] }
    #[repr(C)] pub struct obs_source_frame { _opaque: [u8; 0] }
    extern "C" { pub fn simulcast_clear_transparent(); }
    include!(concat!(env!("OUT_DIR"), "/obs.rs"));
}
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
pub extern "C" fn obs_module_description() -> *const c_char { c"Local MediaPipe portrait segmentation in Rust".as_ptr() }
#[no_mangle]
pub extern "C" fn obs_module_load() -> bool { unsafe { filter::register(); } true }
