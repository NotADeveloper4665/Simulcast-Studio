// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from royshil/obs-backgroundremoval at d177d33197099dbeb5ccd430923de39fb941ea19
use crate::{ffi::*,graph::PASSES,weights::LAYERS,cadence::Cadence};
use std::{cell::RefCell,collections::HashMap,ffi::CString,ptr,time::{Duration,Instant}};
type Result<T>=std::result::Result<T,String>;
type Tex=*mut gs_texture_t;
struct Texture(Tex);
impl Texture {
    unsafe fn target(w:u32,h:u32,format:gs_color_format)->Result<Self>{
        let t=gs_texture_create(w,h,format,1,ptr::null_mut(),GS_RENDER_TARGET);
        if t.is_null(){Err(format!("texture allocation {w}x{h}"))}else{Ok(Self(t))}
    }
    unsafe fn weights(w:u32,h:u32,data:&[u16])->Result<Self>{
        if data.len()!=w as usize*h as usize*4{return Err("weight size mismatch".into());}
        let mut p=data.as_ptr().cast::<u8>();
        let t=gs_texture_create(w,h,GS_RGBA16F,1,&mut p,0);
        if t.is_null(){Err("weight allocation".into())}else{Ok(Self(t))}
    }
}
impl Drop for Texture{fn drop(&mut self){unsafe{gs_texture_destroy(self.0)}}}
struct Effect(*mut gs_effect_t, RefCell<HashMap<&'static str,*mut gs_eparam_t>>, RefCell<HashMap<&'static str,CString>>);
impl Effect{
    unsafe fn new(code:&str,name:&str)->Result<Self>{
        let code=CString::new(code).map_err(|_|"invalid shader")?;
        let name=CString::new(name).map_err(|_|"invalid shader name")?;
        let mut error=ptr::null_mut();
        let e=gs_effect_create(code.as_ptr(),name.as_ptr(),&mut error);
        let msg=if error.is_null(){String::new()}else{let m=std::ffi::CStr::from_ptr(error).to_string_lossy().into_owned();bfree(error.cast());m};
        if e.is_null(){Err(format!("shader compilation: {msg}"))}else{Ok(Self(e,RefCell::new(HashMap::new()),RefCell::new(HashMap::new())))}
    }
    unsafe fn param(&self,name:&'static str)->*mut gs_eparam_t{
        *self.1.borrow_mut().entry(name).or_insert_with(|| {
            let n=CString::new(name).unwrap();
            gs_effect_get_param_by_name(self.0,n.as_ptr())
        })
    }
    unsafe fn texture(&self,name:&'static str,t:Tex){let p=self.param(name);if !p.is_null(){gs_effect_set_texture(p,t)}}
    unsafe fn srgb(&self,name:&'static str,t:Tex){let p=self.param(name);if !p.is_null(){gs_effect_set_texture_srgb(p,t)}}
    unsafe fn float(&self,name:&'static str,v:f32){let p=self.param(name);if !p.is_null(){gs_effect_set_float(p,v)}}
    unsafe fn int(&self,name:&'static str,v:i32){let p=self.param(name);if !p.is_null(){gs_effect_set_int(p,v)}}
    unsafe fn half_pixel(&self,w:u32,h:u32){
        let raw=[0.5/w as f32,0.5/h as f32];
        let mut v:vec2=std::mem::zeroed();
        ptr::copy_nonoverlapping(raw.as_ptr(),(&mut v as *mut vec2).cast::<f32>(),2);
        let p=self.param("halfPixel");
        if !p.is_null(){gs_effect_set_vec2(p,&v);}
    }
    unsafe fn pass(&self,name:&'static str,w:u32,h:u32,mut set:impl FnMut(&Self)){
        let mut names=self.2.borrow_mut();
        let n=names.entry(name).or_insert_with(||CString::new(name).unwrap());
        while gs_effect_loop(self.0,n.as_ptr()){set(self);gs_draw_sprite(ptr::null_mut(),0,w,h);}
    }
    unsafe fn require(&self,name:&str)->Result<()>{
        let n=CString::new(name).unwrap();
        if gs_effect_get_technique(self.0,n.as_ptr()).is_null(){Err(format!("missing technique {name}"))}else{Ok(())}
    }
}
impl Drop for Effect{fn drop(&mut self){unsafe{gs_effect_destroy(self.0)}}}
struct Guard { tex:Tex,z:*mut gs_zstencil_t,space:gs_color_space,srgb:bool }
impl Guard{
    unsafe fn new()->Self{
        let g=Self{tex:gs_get_render_target(),z:gs_get_zstencil_target(),space:gs_get_color_space(),srgb:gs_framebuffer_srgb_enabled()};
        gs_blend_state_push();gs_matrix_push();gs_projection_push();gs_viewport_push();gs_enable_framebuffer_srgb(false);g
    }
}
impl Drop for Guard{fn drop(&mut self){unsafe{
    gs_enable_framebuffer_srgb(self.srgb);gs_viewport_pop();gs_projection_pop();gs_matrix_pop();gs_blend_state_pop();gs_set_render_target_with_color_space(self.tex,self.z,self.space);
}}}
unsafe fn target(t:Tex,w:u32,h:u32){
    gs_set_render_target_with_color_space(t,ptr::null_mut(),GS_CS_SRGB);
    gs_ortho(0.,w as f32,0.,h as f32,-100.,100.);gs_set_viewport(0,0,w as i32,h as i32);
}
struct Level{down:Texture,up:Texture,w:u32,h:u32}
pub struct Pipeline {
    pub w:u32,pub h:u32,pub ready:bool,
    net:Effect,comp:Effect,source:Texture,
    tensors:Vec<Texture>,weights:Vec<(Texture,Texture)>,mask:Texture,
    levels:Vec<Level>,blur_level:Option<usize>,blur_up:bool,
    blur:bool,strength:f32,focus:f32,
    cadence:Cadence,mask_at:Option<Instant>,
}
impl Pipeline{
    pub unsafe fn new(w:u32,h:u32)->Result<Self>{
        let net=Effect::new(include_str!("../data/mediapipe.effect"),"simulcast-mediapipe")?;
        let comp=Effect::new(include_str!("../data/compositor.effect"),"simulcast-mask")?;
        net.require("Draw")?;
        for p in PASSES{net.require(p.name)?;}
        for n in ["RefineMask","Downsample","Upsample","DrawBlurred","DrawFrame","RemoveBackground"]{comp.require(n)?;}
        let source=Texture::target(w,h,GS_BGRA)?;
        let mut tensors=vec![Texture::target(256,144,GS_BGRA)?];
        for (i,p) in PASSES.iter().enumerate(){
            if p.inputs.iter().any(|&v|v>=0&&v as usize>i){return Err("invalid graph input".into());}
            tensors.push(Texture::target(p.width*p.slices,p.height,if i+1==PASSES.len(){GS_R8}else{GS_RGBA32F})?);
        }
        let mut weights=Vec::new();
        for l in LAYERS{weights.push((Texture::weights(l.width,l.height,l.weights)?,Texture::weights(l.bias_width,1,l.bias)?));}
        Ok(Self{w,h,ready:false,net,comp,source,tensors,weights,mask:Texture::target(256,144,GS_R8)?,levels:Vec::new(),blur_level:None,blur_up:false,blur:false,strength:4.,focus:0.,cadence:Cadence::default(),mask_at:None})
    }
    pub fn invalidate_mask(&mut self){self.mask_at=None;self.cadence.reset();self.ready=false;}
    pub unsafe fn prepare(&mut self,filter:*mut obs_source_t,src:*mut obs_source_t,blur:bool,strength:f32,focus:f32,mask_fps:u32)->Result<()>{
        self.ready=false;self.blur=blur;self.strength=strength;self.focus=focus;self.blur_level=None;self.blur_up=false;
        let _guard=Guard::new();gs_matrix_identity();
        target(self.source.0,self.w,self.h);
        let zero:vec4=std::mem::zeroed();gs_clear(GS_CLEAR_COLOR,&zero,0.,0);
        gs_enable_blending(true);gs_blend_function_separate(GS_BLEND_SRCALPHA,GS_BLEND_INVSRCALPHA,GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
        let parent=obs_filter_get_parent(filter);
        if src==parent&&(obs_source_get_output_flags(src)&(OBS_SOURCE_CUSTOM_DRAW|OBS_SOURCE_ASYNC))==0{obs_source_default_render(src);}else{obs_source_video_render(src);}
        gs_enable_blending(false);
        // The camera texture is always current, even if inference is rate-limited.
        let now=Instant::now();
        let due=self.cadence.due(now,mask_fps);
        if due || self.mask_at.is_none_or(|t|now.saturating_duration_since(t)>=Duration::from_millis(125)) {
        target(self.tensors[0].0,256,144);
        self.net.pass("Draw",256,144,|e|e.texture("image",self.source.0));
        for (i,p) in PASSES.iter().enumerate(){
            let w=p.width*p.slices;
            target(self.tensors[i+1].0,w,p.height);
            self.net.pass(p.name,w,p.height,|e|{
                for (j,n) in ["tensor","scale","tensorB","scaleB","tensorR"].iter().enumerate(){
                    let idx=p.inputs[j];e.texture(n,if idx<0{ptr::null_mut()}else{self.tensors[idx as usize].0});
                }
                let (wt,bt)=if p.layer<0{(ptr::null_mut(),ptr::null_mut())}else{let l=&self.weights[p.layer as usize];(l.0.0,l.1.0)};
                e.texture("weights",wt);e.texture("bias",bt);e.texture("image",ptr::null_mut());
            });
        }
        target(self.mask.0,256,144);
        self.comp.pass("RefineMask",256,144,|e|{
            e.texture("probability",self.tensors.last().unwrap().0);e.float("threshold",0.5);e.int("maskWidth",256);e.int("maskHeight",144);
        });
        self.mask_at=Some(now);
        }
        if blur&&strength>0.{
            let variance=strength*(2.*strength*strength+1.)/6.;
            let mut count=0usize;
            while count<8&&(17./18.)*(4f32.powi(count as i32)-1.)<variance{count+=1;}
            count=count.max(2);
            while self.levels.len()<count{
                let (w,h)=self.levels.last().map(|v|(v.w,v.h)).unwrap_or((self.w,self.h));
                let (w,h)=((w+1)/2,(h+1)/2);
                self.levels.push(Level{down:Texture::target(w,h,GS_BGRA)?,up:Texture::target(w,h,GS_BGRA)?,w,h});
            }
            for k in 0..count{
                let l=&self.levels[k];let above=if k==0{self.source.0}else{self.levels[k-1].down.0};
                target(l.down.0,l.w,l.h);
                self.comp.pass("Downsample",l.w,l.h,|e|{e.texture("image",above);e.half_pixel(l.w,l.h);});
            }
            let mut lower=self.levels[count-1].down.0;
            for k in (2..count).rev(){
                let s=&self.levels[k];let l=&self.levels[k-1];
                let p=4f32.powi(k as i32);
                target(l.up.0,l.w,l.h);
                self.comp.pass("Upsample",l.w,l.h,|e|{
                    e.texture("blurred",lower);e.texture("image",l.down.0);e.texture("mask",self.mask.0);
                    e.float("focusPoint",focus);e.half_pixel(s.w,s.h);e.float("strength",strength);
                    e.float("levelOffset",(17./3.)*(p-1.));e.float("levelScale",1./(17.*p));
                });
                lower=l.up.0;
            }
            self.blur_level=Some(1);self.blur_up=count>2;
        }
        self.ready=true;Ok(())
    }
    pub unsafe fn draw(&self){
        let prev=gs_framebuffer_srgb_enabled();gs_enable_framebuffer_srgb(true);
        if let Some(k)=self.blur_level{
            let l=&self.levels[k];let t=if self.blur_up{l.up.0}else{l.down.0};
            self.comp.pass("DrawBlurred",self.w,self.h,|e|{
                e.srgb("image",self.source.0);e.srgb("blurred",t);e.texture("mask",self.mask.0);
                e.float("focusPoint",self.focus);e.float("strength",self.strength);e.float("levelOffset",0.);e.float("levelScale",1./85.);
            });
        }else if self.blur{
            self.comp.pass("DrawFrame",self.w,self.h,|e|e.srgb("image",self.source.0));
        }else{
            self.comp.pass("RemoveBackground",self.w,self.h,|e|{e.srgb("image",self.source.0);e.texture("mask",self.mask.0);});
        }
        gs_enable_framebuffer_srgb(prev);
    }
}
