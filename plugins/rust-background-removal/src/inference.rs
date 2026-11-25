use crossbeam_channel::Receiver;
use ort::session::Session;
use ort::value::Tensor;
use std::{sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}},time::Instant};
pub const N: usize = 320;
pub struct Job { pub rgb: Vec<u8>, pub model: String, pub generation: u64, pub captured: Instant }
pub struct Mask { pub alpha: Vec<u8>, pub generation: u64, pub captured: Instant }
fn load(path: &str, gpu: bool) -> Result<Session, Box<dyn std::error::Error>> {
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
    Ok(builder.with_execution_providers(providers)?.commit_from_file(path)?)
}
fn preprocess(rgb: &[u8]) -> ndarray::Array4<f32> {
    let max = rgb.iter().copied().max().unwrap_or(0).max(1) as f32;
    ndarray::Array4::from_shape_fn((1,3,N,N), |(_,c,y,x)| {
        (rgb[(y*N+x)*3+c] as f32/max - [0.485,0.456,0.406][c]) / [0.229,0.224,0.225][c]
    })
}
fn quantize(data:&[f32]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    if data.len()!=N*N || data.iter().any(|v| !v.is_finite()) { return Err("unexpected mask".into()); }
    let lo = data.iter().copied().fold(f32::INFINITY,f32::min);
    let hi = data.iter().copied().fold(f32::NEG_INFINITY,f32::max);
    Ok(data.iter().map(|v| {
        let a=if hi-lo > 1e-6 { (v-lo)/(hi-lo) } else { v.clamp(0.,1.) };
        (a.clamp(0.,1.)*255.).round() as u8
    }).collect())
}
fn infer(session: &mut Session, rgb: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    if rgb.len()!=N*N*3 { return Err("invalid RGB frame".into()); }
    let input = Tensor::from_array(preprocess(rgb))?;
    let outputs = session.run(ort::inputs![input])?;
    if outputs.len()==0 { return Err("model has no output".into()); }
    let (shape, data) = outputs[0].try_extract_tensor::<f32>()?;
    if &shape[..] != &[1,1,N as i64,N as i64] { return Err("expected [1,1,320,320] foreground probabilities".into()); }
    quantize(data)
}
pub fn run(rx: Receiver<Job>, result: Arc<Mutex<Option<Mask>>>, stop: Arc<AtomicBool>) {
    let mut path = String::new();
    let mut session = None;
    let mut gpu = true;
    let mut reported = false;
    while !stop.load(Ordering::Acquire) {
        let job = match rx.recv_timeout(std::time::Duration::from_millis(50)) {
            Ok(j)=>j, Err(crossbeam_channel::RecvTimeoutError::Timeout)=>continue, Err(_)=>break
        };
        if stop.load(Ordering::Acquire) { break; }
        if job.model != path {
            path = job.model.clone(); gpu=true; reported=false;
            session = match std::panic::catch_unwind(|| load(&path,true).or_else(|_|load(&path,false))) {
                Ok(Ok(s))=>Some(s),
                Ok(Err(e))=>{eprintln!("background model: {e}");None},
                Err(_)=>{eprintln!("ONNX Runtime could not initialize; check ORT_DYLIB_PATH");None}
            };
        }
        if let Some(s) = session.as_mut() {
            match infer(s,&job.rgb) {
                Ok(alpha) => { reported=false; if let Ok(mut slot)=result.lock() { *slot=Some(Mask{alpha,generation:job.generation,captured:job.captured}); } }
                Err(e)=>{
                    if !reported { eprintln!("background inference: {e}");reported=true; }
                    if gpu { gpu=false; session=load(&path,false).ok(); }
                }
            }
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn black_input_is_finite() {
        let a=preprocess(&vec![0;N*N*3]);
        assert_eq!(a.shape(), &[1,3,N,N]);
        assert!(a.iter().all(|x|x.is_finite()));
    }
    #[test] fn mask_endpoints_and_invalid() {
        let mut a=vec![0.25;N*N]; a[1]=0.75;
        let q=quantize(&a).unwrap(); assert_eq!(q[0],0);assert_eq!(q[1],255);
        a[2]=f32::NAN;assert!(quantize(&a).is_err());
        assert!(quantize(&[]).is_err());
    }
    #[test] fn constant_masks_preserve_probability() {
        assert_eq!(quantize(&vec![1.;N*N]).unwrap()[0],255);
        assert_eq!(quantize(&vec![0.;N*N]).unwrap()[0],0);
    }
    #[test] fn actual_onnx_inference_if_fixture_is_set() {
        let Ok(path)=std::env::var("SIMULCAST_TEST_MODEL") else { return };
        let mut s=load(&path,false).unwrap();
        let mask=infer(&mut s,&vec![128;N*N*3]).unwrap();
        assert_eq!(mask.len(),N*N);
    }
}
