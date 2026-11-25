use crossbeam_channel::{Receiver, Sender};
use ort::session::Session;
use ort::value::Tensor;
use std::sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}};
pub const N: usize = 320;
pub struct Job { pub rgb: Vec<u8>, pub model: String, pub generation: u64 }
pub struct Mask { pub alpha: Vec<f32>, pub generation: u64 }
fn load(path: &str) -> Result<Session, Box<dyn std::error::Error>> {
    let builder = Session::builder()?;
    let mut providers = Vec::new();
    #[cfg(feature="cuda")]
    providers.push(ort::execution_providers::CUDAExecutionProvider::default().build());
    #[cfg(feature="directml")]
    providers.push(ort::execution_providers::DirectMLExecutionProvider::default().build());
    #[cfg(feature="coreml")]
    providers.push(ort::execution_providers::CoreMLExecutionProvider::default().build());
    providers.push(ort::execution_providers::CPUExecutionProvider::default().build());
    Ok(builder.with_execution_providers(providers)?.commit_from_file(path)?)
}
fn infer(session: &mut Session, rgb: &[u8]) -> Result<Vec<f32>, Box<dyn std::error::Error>> {
    let mut chw = vec![0f32; 3*N*N];
    let max = rgb.iter().copied().max().unwrap_or(0).max(1) as f32;
    for i in 0..N*N { for c in 0..3 {
        chw[c*N*N+i] = (rgb[i*3+c] as f32/max - [0.485,0.456,0.406][c]) / [0.229,0.224,0.225][c];
    }}
    let input = Tensor::from_array(([1usize,3,N,N], chw.into_boxed_slice()))?;
    let outputs = session.run(ort::inputs![input])?;
    let (shape, data) = outputs[0].try_extract_tensor::<f32>()?;
    if &shape[..] != &[1,1,N as i64,N as i64] || data.iter().any(|v| !v.is_finite()) { return Err("unexpected mask".into()); }
    let lo = data.iter().copied().fold(f32::INFINITY,f32::min);
    let hi = data.iter().copied().fold(f32::NEG_INFINITY,f32::max);
    Ok(data.iter().map(|v| if hi-lo > 1e-6 { (v-lo)/(hi-lo) } else { v.clamp(0.,1.) }).collect())
}
pub fn run(rx: Receiver<Job>, result: Arc<Mutex<Option<Mask>>>, stop: Arc<AtomicBool>) {
    let mut path = String::new();
    let mut session = None;
    while !stop.load(Ordering::Acquire) {
        let Ok(job) = rx.recv_timeout(std::time::Duration::from_millis(50)) else { continue };
        if job.model != path { path = job.model.clone(); session = match load(&path) { Ok(s)=>Some(s), Err(e)=>{eprintln!("background model: {e}");None} }; }
        if let Some(s) = session.as_mut() {
            match infer(s,&job.rgb) {
                Ok(alpha) => { if let Ok(mut slot)=result.lock() { *slot=Some(Mask{alpha,generation:job.generation}); } }
                Err(e)=>eprintln!("background inference: {e}")
            }
        }
    }
}
