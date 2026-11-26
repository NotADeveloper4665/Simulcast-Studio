use crate::{ffi::*, inference::{self, Job, Mask, W, H}};
use crossbeam_channel::{bounded, Receiver, Sender, TrySendError};
use std::{cell::UnsafeCell, ffi::{c_void, c_char, CStr}, ptr, sync::{Arc, Mutex, atomic::{AtomicBool, AtomicU32, Ordering}}, thread::JoinHandle, time::{Duration, Instant}};

#[derive(Clone)]
struct Settings { model: String, threshold: f32, feather: f32, revision: u64 }
struct Filter {
    source: *mut obs_source_t,
    dirty: AtomicBool,
    width: AtomicU32,
    height: AtomicU32,
    settings: Mutex<Settings>,
    // All graphics resources are accessed only under the libobs graphics context.
    render: UnsafeCell<Render>,
    tx: Sender<Job>,
    evict: Receiver<Job>,
    result: Arc<Mutex<Option<Mask>>>,
    stop: Arc<AtomicBool>,
    worker: Option<JoinHandle<()>>,
}
struct Render {
    capture: *mut gs_texrender_t,
    resize: *mut gs_texrender_t,
    stages: [*mut gs_stagesurf_t; 2],
    staged: [Option<Instant>; 2],
    write: usize,
    mask: *mut gs_texture_t,
    effect: *mut gs_effect_t,
    dimensions: (u32,u32),
    revision: u64,
    generation: u64,
    last_mask: Option<Instant>,
    frame_valid: bool,
}
impl Render {
    unsafe fn new() -> Self {
        let effect = gs_effect_create(concat!(include_str!("../data/background-removal.effect"), "\0").as_ptr().cast(), c"background-removal.effect".as_ptr(), ptr::null_mut());
        Self {
            capture: gs_texrender_create(GS_RGBA, GS_ZS_NONE),
            resize: gs_texrender_create(GS_RGBA, GS_ZS_NONE),
            stages: [gs_stagesurface_create(W as u32,H as u32,GS_RGBA),gs_stagesurface_create(W as u32,H as u32,GS_RGBA)],
            staged: [None,None], write: 0,
            mask: gs_texture_create(W as u32,H as u32,GS_R8,1,ptr::null_mut(),GS_DYNAMIC),
            effect, dimensions:(0,0), revision:0, generation:0, last_mask:None, frame_valid:false,
        }
    }
    fn valid(&self) -> bool {
        !self.capture.is_null() && !self.resize.is_null() && !self.mask.is_null() && !self.effect.is_null() && self.stages.iter().all(|s| !s.is_null())
    }
    unsafe fn free(&mut self) {
        if !self.capture.is_null() { gs_texrender_destroy(self.capture); }
        if !self.resize.is_null() { gs_texrender_destroy(self.resize); }
        for s in self.stages { if !s.is_null() { gs_stagesurface_destroy(s); } }
        if !self.mask.is_null() { gs_texture_destroy(self.mask); }
        if !self.effect.is_null() { gs_effect_destroy(self.effect); }
    }
}
unsafe fn read_settings(s: *mut obs_data_t, revision: u64) -> Settings {
    let path = obs_data_get_string(s,c"model".as_ptr());
    Settings {
        model: if path.is_null() { String::new() } else { CStr::from_ptr(path).to_string_lossy().into_owned() },
        threshold: obs_data_get_double(s,c"threshold".as_ptr()).clamp(0.,1.) as f32,
        feather: obs_data_get_double(s,c"feather".as_ptr()).clamp(0.,0.5) as f32,
        revision,
    }
}
unsafe extern "C" fn name(_: *mut c_void) -> *const c_char { c"AI Background Removal (Simulcast)".as_ptr() }
unsafe extern "C" fn defaults(s: *mut obs_data_t) {
    obs_data_set_default_string(s,c"model".as_ptr(),c"".as_ptr());
    obs_data_set_default_double(s,c"threshold".as_ptr(),0.5);
    obs_data_set_default_double(s,c"feather".as_ptr(),0.1);
}
unsafe extern "C" fn properties(_: *mut c_void) -> *mut obs_properties_t {
    let p = obs_properties_create();
    obs_properties_add_path(p,c"model".as_ptr(),c"Optional MediaPipe ONNX override (256 x 144)".as_ptr(),OBS_PATH_FILE,c"ONNX (*.onnx)".as_ptr(),ptr::null());
    obs_properties_add_float_slider(p,c"threshold".as_ptr(),c"Mask confidence".as_ptr(),0.,1.,0.01);
    obs_properties_add_float_slider(p,c"feather".as_ptr(),c"Edge feather".as_ptr(),0.,0.5,0.01);
    p
}
unsafe extern "C" fn create(s: *mut obs_data_t, source: *mut obs_source_t) -> *mut c_void {
    let (tx,rx) = bounded(1);
    let evict = rx.clone();
    let result = Arc::new(Mutex::new(None));
    let stop = Arc::new(AtomicBool::new(false));
    let (out,quit) = (result.clone(),stop.clone());
    let worker = match std::thread::Builder::new().name("simulcast-segmentation".into()).spawn(move || inference::run(rx,out,quit)) {
        Ok(t)=>t, Err(_)=>return ptr::null_mut()
    };
    obs_enter_graphics();
    let render = Render::new();
    
    obs_leave_graphics();
    // Retain the filter even if graphics initialization fails; draw nothing.
    Box::into_raw(Box::new(Filter { source, dirty:AtomicBool::new(true), width:AtomicU32::new(0),height:AtomicU32::new(0), settings:Mutex::new(read_settings(s,0)),render:UnsafeCell::new(render),tx,evict,result,stop,worker:Some(worker) })).cast()
}
unsafe extern "C" fn update(data: *mut c_void,s: *mut obs_data_t) {
    if data.is_null() { return; }
    let f = &*data.cast::<Filter>();
    if let Ok(mut old)=f.settings.lock() {
        let mut next=read_settings(s,old.revision);
        if next.model != old.model { next.revision=old.revision.wrapping_add(1); }
        *old=next;
    }
}
unsafe extern "C" fn destroy(data: *mut c_void) {
    if data.is_null() { return; }
    let mut f=Box::from_raw(data.cast::<Filter>());
    f.stop.store(true,Ordering::Release);
    if let Some(t)=f.worker.take() { let _=t.join(); }
    obs_enter_graphics();
    f.render.get_mut().free();
    obs_leave_graphics();
}
fn latest(f: &Filter, job: Job) {
    if let Err(TrySendError::Full(job)) = f.tx.try_send(job) {
        let _ = f.evict.try_recv();
        let _ = f.tx.try_send(job);
    }
}
unsafe fn draw(tex:*mut gs_texture_t,w:u32,h:u32) {
    let e=obs_get_base_effect(OBS_EFFECT_DEFAULT);
    if e.is_null() { return; }
    let p=gs_effect_get_param_by_name(e,c"image".as_ptr());
    gs_effect_set_texture(p,tex);
    while gs_effect_loop(e,c"Draw".as_ptr()) { gs_draw_sprite(tex,0,w,h); }
}
unsafe extern "C" fn render(data:*mut c_void,_:*mut gs_effect_t) {
    if data.is_null() { return; }
    let f=&*data.cast::<Filter>();
    let Ok(settings)=f.settings.try_lock().map(|s|s.clone()) else { return; };
    let r=&mut *f.render.get();
    let target=obs_filter_get_target(f.source);
    if target.is_null() || !r.valid() { return; }
    let parent=obs_filter_get_parent(f.source);
    if parent.is_null() || !obs_source_showing(parent) { return; }
    let w=obs_source_get_base_width(target);
    let h=obs_source_get_base_height(target);
    if w==0 || h==0 { return; }
    if r.dimensions!=(w,h) || r.revision!=settings.revision {
        r.dimensions=(w,h);r.revision=settings.revision;r.generation=r.generation.wrapping_add(1);
        r.staged=[None,None];r.last_mask=None;r.frame_valid=false;
        f.dirty.store(true,Ordering::Relaxed);
        let _=f.evict.try_recv();
    }

    // OBS can render one source for preview, program and multiple canvases
    // during a single video tick. Reuse the capture and staged mask work.
    if f.dirty.swap(false,Ordering::Relaxed) {
    r.frame_valid=false;
    gs_texrender_reset(r.capture);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
    if !gs_texrender_begin(r.capture,w,h) {
        gs_blend_state_pop();return;
    }
    simulcast_clear_transparent();
    gs_ortho(0.,w as f32,0.,h as f32,-100.,100.);
    let flags=obs_source_get_output_flags(target);
    if target==parent && (flags & (OBS_SOURCE_CUSTOM_DRAW|OBS_SOURCE_ASYNC))==0 {
        obs_source_default_render(target);
    } else { obs_source_video_render(target); }
    gs_texrender_end(r.capture);
    gs_blend_state_pop();
    let tex=gs_texrender_get_texture(r.capture);
    if tex.is_null() { return; }
    r.frame_valid=true;

    // Read the previous staging surface, not the texture just submitted.
    let previous=1-r.write;
    if let Some(captured)=r.staged[previous].take() {
        let mut pixels=ptr::null_mut();
        let mut stride=0;
        if gs_stagesurface_map(r.stages[previous],&mut pixels,&mut stride) {
            if !pixels.is_null() && stride >= (W*4) as u32 {
                let mut rgb=Vec::with_capacity(W*H*3);
                for y in 0..H {
                    let row=std::slice::from_raw_parts(pixels.add(y*stride as usize),W*4);
                    for px in row.chunks_exact(4) { rgb.extend_from_slice(&px[..3]); }
                }
                latest(f,Job{rgb,model:settings.model.clone(),generation:r.generation,captured});
            }
            gs_stagesurface_unmap(r.stages[previous]);
        }
    }
    gs_texrender_reset(r.resize);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
    if gs_texrender_begin(r.resize,W as u32,H as u32) {
        gs_ortho(0.,W as f32,0.,H as f32,-100.,100.);
        draw(tex,W as u32,H as u32);
        gs_texrender_end(r.resize);
        let small=gs_texrender_get_texture(r.resize);
        if !small.is_null() {
            gs_stage_texture(r.stages[r.write],small);
            r.staged[r.write]=Some(Instant::now());
            r.write=1-r.write;
        }
    }
    gs_blend_state_pop();

    if let Ok(mut slot)=f.result.try_lock() {
        if let Some(m)=slot.take() {
            if m.generation==r.generation {
                if m.captured.elapsed()<Duration::from_millis(350) && m.alpha.len()==W*H {
                    gs_texture_set_image(r.mask,m.alpha.as_ptr(),W as u32,false);
                    r.last_mask=Some(m.captured);
                } else { r.last_mask=None; }
            }
        }
    }
    }
    if !r.frame_valid { return; }
    let tex=gs_texrender_get_texture(r.capture);
    if tex.is_null() { return; }
    let ready=r.last_mask.is_some_and(|t|t.elapsed()<Duration::from_millis(350));
    if !ready { return; }
    let e=r.effect;
    gs_effect_set_texture(gs_effect_get_param_by_name(e,c"image".as_ptr()),tex);
    gs_effect_set_texture(gs_effect_get_param_by_name(e,c"mask".as_ptr()),r.mask);
    gs_effect_set_float(gs_effect_get_param_by_name(e,c"threshold".as_ptr()),settings.threshold);
    gs_effect_set_float(gs_effect_get_param_by_name(e,c"feather".as_ptr()),settings.feather);
    gs_effect_set_float(gs_effect_get_param_by_name(e,c"enabled".as_ptr()),if ready {1.} else {0.});
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    while gs_effect_loop(e,c"Draw".as_ptr()) { gs_draw_sprite(tex,0,w,h); }
    gs_blend_state_pop();
}
unsafe extern "C" fn tick(data:*mut c_void,_:f32) {
    if data.is_null() { return; }
    let f=&*data.cast::<Filter>();
    f.dirty.store(true,Ordering::Relaxed);
    let target=obs_filter_get_target(f.source);
    let (w,h)=if target.is_null() {(0,0)} else {(obs_source_get_base_width(target),obs_source_get_base_height(target))};
    f.width.store(w,Ordering::Relaxed);
    f.height.store(h,Ordering::Relaxed);
}
unsafe extern "C" fn width(data:*mut c_void)->u32 {
    if data.is_null() {0} else {(&*data.cast::<Filter>()).width.load(Ordering::Relaxed)}
}
unsafe extern "C" fn height(data:*mut c_void)->u32 {
    if data.is_null() {0} else {(&*data.cast::<Filter>()).height.load(Ordering::Relaxed)}
}
pub unsafe fn register() {
    let mut info:obs_source_info=std::mem::zeroed();
    info.id=c"simulcast_onnx_background_removal".as_ptr();
    info.type_=OBS_SOURCE_TYPE_FILTER;
    info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
    info.get_name=Some(name);info.create=Some(create);info.destroy=Some(destroy);
    info.get_defaults=Some(defaults);info.get_properties=Some(properties);info.update=Some(update);
    info.video_render=Some(render);info.video_tick=Some(tick);info.get_width=Some(width);info.get_height=Some(height);
    obs_register_source_s(&info,std::mem::size_of::<obs_source_info>());
}
