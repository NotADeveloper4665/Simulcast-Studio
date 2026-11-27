# Simulcast ONNX Background Removal

Native Rust libobs video filter. It uses the existing Simulcast libobs ABI,
not a renamed or invented API. Supported model contract: U2Netp float32
RGB NCHW [1,3,320,320], first output sigmoid probabilities [1,1,320,320].
Other ONNX segmentation architectures require separate preprocessing and
output adapters. Supply your own appropriately licensed model.

## Build and install (Linux)
Install Rust stable (1.88 or later), libclang, and the development headers
and library from the exact Simulcast/libobs build being targeted.
Install ONNX Runtime 1.22 (matching the selected execution provider).
```sh
export OBS_INCLUDE_DIR="$(pkg-config --variable=includedir libobs)/obs"
# If pkg-config already returns the obs directory, use it directly.
export OBS_LIB_DIR="$(pkg-config --variable=libdir libobs)"
export ORT_DYLIB_PATH=/absolute/path/to/libonnxruntime.so
cargo build --release
# NVIDIA, if the matching CUDA ORT distribution and drivers are present:
cargo build --release --features cuda
```
Copy the release library to the plugin binary directory of the actual
Simulcast installation (rename libsimulcast_background_removal.so to
simulcast-background-removal.so if its loader requires this). The effect is
embedded, so no external shader or data directory is required. Windows
uses the matching obs import library and onnxruntime.dll; macOS uses
libobs.dylib and libonnxruntime.dylib. Build with directml or coreml
respectively if the runtime supports them. No GPU is required for CPU.
The environment variable must be available to the launched Simulcast process.

Add **AI Background Removal (ONNX)** under Effect Filters on a webcam;
select the model and adjust mask confidence and edge feathering. This is
an SDR RGBA render filter; webcam YUV conversion is handled by libobs.
It keeps the original source visible while loading or if no fresh mask
is available (one-second expiry). Do not rely on it for privacy redaction.

## Architecture
Capture and resize on graphics context; alternating staging surfaces
read the previous submission. Copy only 320x320 RGB to a capacity-one
queue, evicting stale queued work. Inference and model loading occur on
a dedicated thread; render only tries to acquire settings/result locks.
The mask is uploaded as R8 and blended with premultiplied source alpha.
Model or dimension changes invalidate prior masks. The worker is joined
during destruction before graphics resources are released. Staging readback
can still synchronize with the GPU; measure on real hardware.
Edge feather is a soft probability threshold, not a spatial Gaussian blur.

## Validation
CI checks against a packaged OBS 30.x development ABI and runs a generated
ONNX graph through the CPU provider; this does not establish live webcam
or GPU performance. Test against the exact Simulcast build and its
loader before distributing. GPU execution providers need compatible
runtime binaries and drivers; session load/inference fall back to CPU.
