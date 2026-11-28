// SPDX-License-Identifier: GPL-2.0-or-later
// Model and reference contract: MediaPipe Selfie Segmentation (landscape),
// Google LLC (Apache-2.0), as used by royshil/obs-backgroundremoval.
use crossbeam_channel::Receiver;
use ort::{session::Session, value::TensorRef};
use std::{sync::{Arc, Mutex, OnceLock, atomic::{AtomicBool, Ordering}}, time::{Duration, Instant}};
pub const W: usize = 256;
pub const H: usize = 144;
pub const MAX_MASK_AGE: Duration = Duration::from_millis(350);

fn fresh(captured: Instant, now: Instant) -> bool {
    now.saturating_duration_since(captured) < MAX_MASK_AGE
}
const MODEL: &[u8] = include_bytes!(env!("SIMULCAST_MODEL_PATH"));
pub struct Job { pub rgb: Vec<u8>, pub model: String, pub generation: u64, pub captured: Instant }
pub struct Mask { pub alpha: Vec<u8>, pub generation: u64, pub captured: Instant }

fn load(path: &str, gpu: bool) -> Result<Session, Box<dyn std::error::Error>> {
    static INIT: OnceLock<Result<(), String>> = OnceLock::new();
    let init = INIT.get_or_init(|| {
        let runtime = std::env::var("ORT_DYLIB_PATH").ok()
            .or_else(|| option_env!("SIMULCAST_ORT_PATH").map(str::to_owned));
        let result = match runtime {
            Some(path) => ort::init_from(path).commit(),
            None => ort::init().commit(),
        };
        result.map(|_| ()).map_err(|e| e.to_string())
    });
    if let Err(e) = init { return Err(e.clone().into()); }
    let builder = Session::builder()?.with_intra_threads(2)?;
    let mut providers = Vec::new();
    if gpu {
        #[cfg(all(feature="cuda", any(target_os="linux",target_os="windows")))]
        providers.push(ort::execution_providers::CUDAExecutionProvider::default().build());
        #[cfg(all(feature="directml", target_os="windows"))]
        providers.push(ort::execution_providers::DirectMLExecutionProvider::default().build());
        #[cfg(all(feature="coreml", target_os="macos"))]
        providers.push(ort::execution_providers::CoreMLExecutionProvider::default().build());
    }
    providers.push(ort::execution_providers::CPUExecutionProvider::default().build());
    let builder = builder.with_execution_providers(providers)?;
    Ok(if path.is_empty() { builder.commit_from_memory(MODEL)? } else { builder.commit_from_file(path)? })
}
fn preprocess_into(rgb: &[u8], input: &mut ndarray::Array4<f32>) {
    for (i, pixel) in rgb.chunks_exact(3).enumerate() {
        for c in 0..3 { input[[0,c,i/W,i%W]] = pixel[c] as f32 / 255.0; }
    }
}
#[cfg(test)]
fn preprocess(rgb: &[u8]) -> ndarray::Array4<f32> {
    let mut input = ndarray::Array4::zeros((1,3,H,W));
    preprocess_into(rgb, &mut input);
    input
}
fn quantize(data: &[f32]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    if data.len()!=W*H || data.iter().any(|v| !v.is_finite()) { return Err("invalid foreground mask".into()); }
    // Absolute model probability; never min/max normalize an empty foreground.
    Ok(data.iter().map(|v| (v.clamp(0.,1.)*255.).round() as u8).collect())
}
fn infer(session: &mut Session, rgb: &[u8], scratch: &mut ndarray::Array4<f32>) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    if rgb.len()!=W*H*3 { return Err("invalid RGB frame".into()); }
    preprocess_into(rgb, scratch);
    let input = TensorRef::from_array_view(scratch.view())?;
    let outputs = session.run(ort::inputs![input])?;
    if outputs.len()==0 { return Err("no output".into()); }
    let (shape, data) = outputs[0].try_extract_tensor::<f32>()?;
    if &shape[..] != &[1,1,H as i64,W as i64] { return Err("expected [1,1,144,256] mask".into()); }
    quantize(data)
}
pub fn run(rx: Receiver<Job>, result: Arc<Mutex<Option<Mask>>>, stop: Arc<AtomicBool>) {
    let mut path: Option<String> = None;
    let mut session = None;
    let mut gpu = true;
    let mut scratch = ndarray::Array4::zeros((1,3,H,W));
    while !stop.load(Ordering::Acquire) {
        let job = match rx.recv_timeout(Duration::from_millis(50)) {
            Ok(j)=>j, Err(crossbeam_channel::RecvTimeoutError::Timeout)=>continue, Err(_)=>break
        };
        if stop.load(Ordering::Acquire) { break; }
        if !fresh(job.captured, Instant::now()) { continue; }
        if path.as_deref() != Some(job.model.as_str()) {
            path = Some(job.model.clone()); gpu = true;
            session = match std::panic::catch_unwind(|| load(&job.model,true).or_else(|_|load(&job.model,false))) {
                Ok(Ok(s))=>Some(s),
                Ok(Err(e))=>{eprintln!("Simulcast background model: {e}");None},
                Err(_)=>{eprintln!("Simulcast ONNX runtime initialization failed");None}
            };
        }
        // Model loading may take longer than the frame freshness budget.
        if stop.load(Ordering::Acquire) { break; }
        if !fresh(job.captured, Instant::now()) { continue; }
        if let Some(s) = session.as_mut() {
            match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| infer(s,&job.rgb,&mut scratch))) {
                Ok(Ok(alpha)) => { if let Ok(mut slot)=result.lock() { *slot=Some(Mask{alpha,generation:job.generation,captured:job.captured}); } }
                _ => {
                    if let Ok(mut slot)=result.lock() { *slot=Some(Mask{alpha:Vec::new(),generation:job.generation,captured:job.captured}); }
                    eprintln!("Simulcast background inference failed");
                    session = if gpu { gpu=false; std::panic::catch_unwind(||load(&job.model,false)).ok().and_then(Result::ok) } else { None };
                }
            }
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn freshness_boundary() {
        let captured = Instant::now();
        assert!(fresh(captured, captured));
        assert!(fresh(captured, captured + MAX_MASK_AGE - Duration::from_millis(1)));
        assert!(!fresh(captured, captured + MAX_MASK_AGE));
    }
    #[test] fn rgb_contract() {
        let mut rgb=vec![0;W*H*3]; rgb[0]=255; rgb[1]=128;
        let a=preprocess(&rgb);
        assert_eq!(a.shape(), &[1,3,H,W]);
        assert_eq!(a[[0,0,0,0]],1.);
        assert!((a[[0,1,0,0]]-128./255.).abs()<1e-6);
    }
    #[test] fn absolute_probability() {
        let mut a=vec![0.25;W*H]; a[1]=0.75;
        let q=quantize(&a).unwrap(); assert_eq!(q[0],64);assert_eq!(q[1],191);
        a[2]=f32::NAN;assert!(quantize(&a).is_err());
        assert!(quantize(&[]).is_err());
    }
    #[test] fn pinned_model_cpu_inference() {
        let mut s=load("",false).expect("bundled MediaPipe model");
        assert_eq!(infer(&mut s,&vec![128;W*H*3], &mut ndarray::Array4::zeros((1,3,H,W))).unwrap().len(),W*H);
    }
}
