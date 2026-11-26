# Simulcast AI Background Removal

Right-click webcam → Filters → Effect Filters → AI Background Removal (Simulcast). The Fedora compatibility RPM bundles the filter and pinned 256×144 MediaPipe landscape ONNX model used by royshil/obs-backgroundremoval. No model download at runtime. CPU inference is the default; CUDA, DirectML and CoreML features are optional build targets, not validated runtime providers. Threshold and soft edge controls are included. The filter outputs nothing during initialization or if the mask is unavailable/stale; a previously valid mask can still be spatially inaccurate during motion. This is not guaranteed privacy redaction. Disabling the filter shows the original. No changes to host streaming, recording or canvas routing.

This is a first Rust adaptation using the upstream Apache-2.0 model and reference input contract, not a direct port of the latest GPU shader network. It uses an asynchronous bounded queue and GPU staging readback; full upstream blur, GPU shader inference, mask refinement and low-light modes are not included. Test actual webcam and dual-canvas performance on target hardware before release.

Build with exact Simulcast headers and library (OBS_INCLUDE_DIR, OBS_CONFIG_DIR, OBS_LIB_DIR), the SHA-verified model at SIMULCAST_MODEL_PATH, and ONNX Runtime >=1.22 at SIMULCAST_ORT_PATH or ORT_DYLIB_PATH. See NOTICE and Apache-2.0.txt. Do not fall back to old generated OBS ABI bindings.

Capture and readback are performed at most once per OBS video tick, even when preview and multiple canvases render the same source. All views reuse the same current frame and mask; the bounded inference queue remains unchanged.
