// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from royshil/obs-backgroundremoval at d177d33197099dbeb5ccd430923de39fb941ea19
#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case, dead_code)]
mod ffi { include!(concat!(env!("OUT_DIR"), "/obs.rs")); }
mod graph;
mod weights;
mod pipeline;
use ffi::*;
use std::{cell::UnsafeCell, ffi::{c_void,c_char}, ptr, sync::atomic::{AtomicPtr,AtomicU32,AtomicBool,Ordering::Relaxed}};
static MODULE: AtomicPtr<obs_module_t> = AtomicPtr::new(ptr::null_mut());
struct Filter {
    source: *mut obs_source_t,
    pipeline: UnsafeCell<Option<pipeline::Pipeline>>,
    attempted: UnsafeCell<(u32,u32)>,
    w: AtomicU32, h: AtomicU32,
    blur: AtomicBool, strength: AtomicU32, focus: AtomicU32,
    dirty: AtomicBool,
}
fn boundary<T>(fallback:T, f:impl FnOnce()->T)->T {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)).unwrap_or(fallback)
}
#[no_mangle] pub extern "C" fn obs_module_set_pointer(m:*mut obs_module_t){MODULE.store(m,Relaxed);}
#[no_mangle] pub extern "C" fn obs_current_module()->*mut obs_module_t{MODULE.load(Relaxed)}
#[no_mangle] pub extern "C" fn obs_module_ver()->u32{SIMULCAST_LIBOBS_API_VER as u32}
#[no_mangle] pub extern "C" fn obs_module_name()->*const c_char{c"Simulcast Background Removal".as_ptr()}
#[no_mangle] pub extern "C" fn obs_module_description()->*const c_char{c"GPU MediaPipe segmentation in Rust".as_ptr()}
unsafe extern "C" fn name(_: *mut c_void)->*const c_char{c"AI Background Removal (GPU)".as_ptr()}
unsafe extern "C" fn defaults(s:*mut obs_data_t){
    obs_data_set_default_bool(s,c"blur".as_ptr(),false);
    obs_data_set_default_double(s,c"strength".as_ptr(),4.0);
    obs_data_set_default_double(s,c"focus".as_ptr(),-100.0);
}
unsafe extern "C" fn properties(_: *mut c_void)->*mut obs_properties_t{
    let p=obs_properties_create();
    obs_properties_add_bool(p,c"blur".as_ptr(),c"Blur background instead of removing it".as_ptr());
    obs_properties_add_float_slider(p,c"strength".as_ptr(),c"Blur strength".as_ptr(),0.,20.,0.1);
    obs_properties_add_float_slider(p,c"focus".as_ptr(),c"Blur focus ratio (%)".as_ptr(),-100.,100.,1.);
    p
}
unsafe extern "C" fn update(data:*mut c_void,s:*mut obs_data_t){
    if data.is_null()||s.is_null(){return;}
    let f=&*data.cast::<Filter>();
    f.blur.store(obs_data_get_bool(s,c"blur".as_ptr()),Relaxed);
    let v=obs_data_get_double(s,c"strength".as_ptr());
    f.strength.store((if v.is_finite(){v.clamp(0.,20.) as f32}else{4.}).to_bits(),Relaxed);
    let v=obs_data_get_double(s,c"focus".as_ptr());
    f.focus.store((if v.is_finite(){((v.clamp(-100.,100.)+100.)/200.) as f32}else{0.}).to_bits(),Relaxed);
}
unsafe extern "C" fn create(s:*mut obs_data_t,source:*mut obs_source_t)->*mut c_void{
    boundary(ptr::null_mut(),||{
        if source.is_null(){return ptr::null_mut();}
        let p=Box::into_raw(Box::new(Filter{source,pipeline:UnsafeCell::new(None),attempted:UnsafeCell::new((0,0)),w:AtomicU32::new(0),h:AtomicU32::new(0),blur:AtomicBool::new(false),strength:AtomicU32::new(4f32.to_bits()),focus:AtomicU32::new(0),dirty:AtomicBool::new(true)})).cast();
        update(p,s);p
    })
}
unsafe extern "C" fn destroy(data:*mut c_void){
    if data.is_null(){return;}
    boundary((),||{let mut f=Box::from_raw(data.cast::<Filter>());obs_enter_graphics(); f.pipeline.get_mut().take();obs_leave_graphics();});
}
unsafe extern "C" fn width(data:*mut c_void)->u32{if data.is_null(){0}else{(*data.cast::<Filter>()).w.load(Relaxed)}}
unsafe extern "C" fn height(data:*mut c_void)->u32{if data.is_null(){0}else{(*data.cast::<Filter>()).h.load(Relaxed)}}
unsafe extern "C" fn tick(data:*mut c_void,_:f32){
    if data.is_null(){return;}
    boundary((),||{
        let f=&*data.cast::<Filter>();
        let target=obs_filter_get_target(f.source);
        let parent=obs_filter_get_parent(f.source);
        if target.is_null()||parent.is_null()||!obs_source_showing(parent){return;}
        let size=(obs_source_get_base_width(target),obs_source_get_base_height(target));
        f.dirty.store(true,Relaxed);
        if *f.attempted.get()==size{return;}
        *f.attempted.get()=size;
        obs_enter_graphics();
        *f.pipeline.get()=None;
        if size.0>0&&size.1>0{
            match pipeline::Pipeline::new(size.0,size.1){
                Ok(p)=>*f.pipeline.get()=Some(p),Err(e)=>eprintln!("Simulcast background removal: {e}")
            }
        }
        obs_leave_graphics();
        let valid=(*f.pipeline.get()).is_some();
        f.w.store(if valid{size.0}else{0},Relaxed);f.h.store(if valid{size.1}else{0},Relaxed);
    });
}
unsafe extern "C" fn render(data:*mut c_void,_:*mut gs_effect_t){
    if data.is_null(){return;}
    boundary((),||{
        let f=&*data.cast::<Filter>();
        let parent=obs_filter_get_parent(f.source);
        if parent.is_null()||!obs_source_showing(parent){return;}
        let target=obs_filter_get_target(f.source);
        if target.is_null(){return;}
        if let Some(p)=&mut *f.pipeline.get(){
            if (obs_source_get_base_width(target),obs_source_get_base_height(target))!=(p.w,p.h){return;}
            let blur=f.blur.load(Relaxed);
            let strength=f32::from_bits(f.strength.load(Relaxed));
            let focus=f32::from_bits(f.focus.load(Relaxed));
            if f.dirty.swap(false,Relaxed){
                if let Err(e)=p.prepare(f.source,target,blur,strength,focus){
                    eprintln!("Simulcast background removal: {e}");p.ready=false;return;
                }
            }
            if p.ready {p.draw();}
        }
    });
}
#[no_mangle] pub extern "C" fn obs_module_load()->bool{
    boundary(false,||unsafe{
        let mut info:obs_source_info=std::mem::zeroed();
        info.id=c"simulcast_gpu_background_removal".as_ptr();
        info.type_=OBS_SOURCE_TYPE_FILTER;
        info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW|OBS_SOURCE_SRGB;
        info.get_name=Some(name);info.create=Some(create);info.destroy=Some(destroy);
        info.get_defaults=Some(defaults);info.get_properties=Some(properties);info.update=Some(update);
        info.get_width=Some(width);info.get_height=Some(height);info.video_tick=Some(tick);info.video_render=Some(render);
        obs_register_source_s(&info,std::mem::size_of::<obs_source_info>());true
    })
}
