# Simulcast Studio — living development reference

Snapshot: 2026-11-26. Audience: maintainers and AI coding agents. Read this before making architectural, packaging or compatibility decisions. Update it when behavior changes; verify branch HEAD and CI before treating this snapshot as current. Source and tests override this summary. This file documents decisions, not a claim of complete runtime validation.

## Upstream integration planning — 2026-11-27

See [the upstream integration plan](upstream-integration-plan.md) for the pinned commit/PR ledger, dependency batches, Simulcast adaptation points, code-comment requirements and regression gates. Review pins: Simulcast master `f1aabe69c274cc17762427a183520e8479721edd`, upstream `723cd1c803bdee2e57f782a969203de768d45677`, common ancestor `fa6e0bebb4e74b721d79de85448f0c3cbdfa8759`. Upstream is 40 commits ahead / 30 behind this fork. All 40 merged PRs are classified; 74 open PRs were inventoried and triaged, with selected source/caller review.

Individual commit file lists contain 315 unique paths (the aggregate comparison caps at 300). None directly overlaps paths changed by Simulcast master, compatibility or macOS branches since the common ancestor; source/filter/frame/audio APIs still have significant behavioral integration risk. Preserve shared-capture clone groups, private portrait Program layouts, master gain, standard recording/API behavior, distinct background-removal implementations, branding and platform security. The plan is documentation only: no upstream code was imported, no new build generated, and no hardware claim made.

Upstream master push run [45962407898](https://github.com/obs-rust/obs-studio/actions/runs/45962407898) succeeded at the pinned head. Existing Simulcast compiler-repair PR [#3](https://github.com/NotADeveloper4665/Simulcast-Studio/pull/3) remains separate; its final-head run [45956743027](https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45956743027) failed. Integration must resolve baseline failures and validate each batch through GitHub Actions before release. Open PRs and upstream infrastructure optimizations are proposals, not available alpha features. Rollback of this planning documentation changes no runtime behavior.

## Upstream integration implementation: batch A — 2026-11-27

PR #4 now imports the seven portable CI fixes from its plan, preserving authorship and `cherry-pick -x` provenance: upstream #5615/#5747/#5764/#5778/#5780/#5798/#5808 map to integration commits `4d1152cef`, `81b480910`, `c9a9ccbfc`, `02008e97d`, `703cc3ee6`, `c645af157`, `c87c79c7e`. See [implementation progress](upstream-integration-plan.md#implementation-progress-batch-a) for the full mapping and test evidence. This supersedes the planning-only status above for PR #4's branch, not master or any release.

Changes: `.github/workflows/build-project.yaml` guards/reclaims root-owned Tier 2 scratch and raises the inherited Windows Tier 2 deadline to 40 minutes; `ci/queue_report.py::summary` reports observed runner/labels without calling dependency wait pure dispatch latency; `rust/tools/linux-validate/validate.sh` explicitly starts and probes the cache daemon before its existing bounded retry; `rust/tools/linux-validate/ci-runtime.py::{install_packages,install_prepared_delta,install_unavailable_pin,install_from_snapshot,conflicting_pins}` gives stock/prepared apt setup the same exact-version recovery and signed-snapshot trust; `ci/ai_rebase.py::{queue_state,main,requeue_ejected}` takes no action on unreadable GitHub inventories. Original upstream comments explain ownership, timeouts and fallback decisions. No AI-rebase enablement, fleet routing, cache trust shortcut or skipped gate was added.

Fork adaptation: existing tests failed because `ci.toml` omitted Simulcast's two existing release workflows. Register their actual triggers and the narrow release-only macOS GEN-002 rationale, with explanatory comments. Neither release workflow is modified. All canvas, source-clone, master-gain, multistream, recording, ABI and background-removal implementations remain unchanged.

Validation: regression tests on pre-import products ran 118 tests with 10 failures and 6 errors; the imported products pass 241 focused tests. A separate policy/gate suite passes 209 tests after the fork metadata adaptation (overlapping suites, not 450 unique tests). Shell syntax and whitespace checks pass. Published-head GitHub Actions validation is pending; no live apt/container test, application compile, package generation or hardware verification is claimed. Platform effect is CI tooling, including the inherited Windows runner; release/platform availability is unchanged. Batches B–E remain outstanding, as do unrelated baseline CI failures. Reverse the seven integration commits and policy adaptation to roll back this batch.

### Follow-up baseline corrections in PR #4

The first implementation [Actions run 45971550741](https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45971550741) reports the existing unused Linux-only `Instant` import and frontend formatting failures. Commit `dded03bbf` carries forward PR #3's source-only compiler fixes (`d31f485` and `7a03eded`) rather than redoing them. Commit `18adb52ee` applies the pinned clang-format 22.1.3 to 14 frontend files, changing only whitespace/configured braces; the full 6,455-file formatter check passes. Keep these baseline repairs separate from the seven upstream CI imports for review/rollback. They preserve the existing feature implementations.

The full 410-script Python self-test sweep reports additional failures outside the passing targeted suites, including AppImage, build-group and project-identity checks. Not all are diagnosed; no blanket compatibility or green-CI claim is made. Published-head validation and the remaining baseline investigation are required before merging. No generated application package or hardware result is associated with this batch.

### CI follow-up on PR #4

The hosted Fast Gate in [run 45972450070](https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45972450070) passed formatting and stopped at the scripting platform check. The scripting header now selects its printf annotation by compiler capability family (GCC-compatible compilers, excluding SWIG), with the existing MinGW format dialect. The frontend settings and widget implementation files now get their includes from their existing precompiled headers, and the widget group includes its members in alphabetical order.

Several Windows and macOS Rust jobs in [run 45956743027](https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45956743027) failed to link because the independent C audio oracle exported the same master-volume setter as the Rust implementation. The oracle now follows its existing symbol-renaming convention for this setter. The production C ABI and gain processing retain their names and behavior. The fork's title fixture now expects the truncated Simulcast label while keeping its length and termination assertions. The Fedora alpha AppImage assembly step invokes its existing script through uv, installed in the Fedora build job.

With CMake and Ninja available, 182 focused CI, policy, and configuration tests passed. The pinned formatter and a standalone C check for the title contract also passed. GitHub Actions remains the validation for the cross-platform audio link and the full application builds. This follow-up does not produce a package or establish hardware behavior. Revert the CI follow-up commit to undo these repairs independently of the upstream imports.

## 1. Identity and source lineage

OBS (2012) → OBS Studio (2014 cross-platform rewrite) → obs-rust/obs-studio → Simulcast Studio. Hybrid Rust/C/C++ with inherited Qt frontend and OBS C ABI; not a completely Rust application. Linux-first, independent community fork. Preserve upstream history, credits, notices, ABI and config identity. AI-assisted development is explicitly disclosed; generated tests are not proof of production reliability. Host GPL-2.0-or-later; component licenses differ.

## 2. Branches and distribution snapshot

| Branch | Purpose |
|---|---|
| master | Alpha releases, established dual-canvas features; README at e187b859d13e4f89fbd30c1b1295af2ebc54f378 |
| feature/obs-plugin-compatibility | Fedora-only experimental branch: ABI discovery, browser, source clones, clean recording, unified recording, two Rust AI filters; 9 commits ahead / 1 behind master at review |
| feature/macos-browser-virtualcam | Independent macOS branch based on alpha plus docs; 2 commits ahead of master at review; does not contain Fedora compatibility features |
| feature/rust-background-removal | Earlier standalone ONNX development, 16 ahead / 33 behind master; do not assume latest integration lives here |
| simulcast-appimage | Historical branch, 11 behind master and no unique commits at review |

Alpha Build 9 source cb8b1833ae056740d2e5b8935e8a6627e5a0b307; Fedora 44 RPM release 15, Fedora 44 AppImage, Ubuntu 26.04 DEB, Debian testing DEB, Apple Silicon DMG. Debian 13 stable lacks required FFmpeg 8. macOS alpha ad-hoc signed, not notarized, browser and virtual camera omitted. Physical camera input is distinct from virtual camera output; actual hardware use remains unverified in this review.
Release: https://github.com/NotADeveloper4665/Simulcast-Studio/releases/tag/simulcast-alpha-build-9
README-documented successful development source 6ca9a8ff7397ad586cc4ea31539624c797388143, run 45865538881, artifact 15139853973. Fedora 44 x86-64 only. https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45865538881
These are documented build references, not independent checks of current artifact availability. Never conflate a successful CI job with working live capture or publish untested feature claims. Check build commit, job conclusion, artifact existence, expiry, architecture and package contents before answering download questions. Preserve alpha vs dev channel separation.

## 3. Chronological Simulcast-specific history (master)

Commit IDs below are abbreviated; resolve in this repository.
- 8b37a5e: dual-canvas multistream and Fedora-compatible AppImage.
- 865d169: inline portrait preview and master output volume.
- e191a10: keep horizontal Program usable in Studio Mode.
- c9ba73b: master gain applied in production Rust audio callback.
- 6559d4f: branding and Fedora RPM; 7663d26: product version build flag; 320449e: runtime identity.
- 7d01a15: inline portrait editing using standard scene tools.
- f758584: paired recordings and YouTube guidance; 7cb012a: detect/prefer available hardware H.264 encoders.
- abaa035: Intel GPU usage and guidance moved into settings; ba2fa9d and eb685b4: YouTube dual-stream settings and change tracking.
- fc6801d: recording folders in Output, consolidate control toggle.
- cad7592 and 617609c: branding with upstream credits; 80c7dea: master Fedora RPM numbered alpha workflow; 89a742c: upstream merge preserving branding; c92fc82: integrate alpha and release workflow.
- 5989516: explicitly install pinned Rust toolchain in Fedora CI container; 9e3c3cc: AppImage, DEB, macOS alpha packaging.
- 324c4cb: dock show/hide refresh; 25999ff: unified stream settings/controls and transform editor; afcf824: lifecycle guards and shortcuts.
- 515c973: independent portrait Scenes/Sources; 3291875: duplicate/reorder scenes; 6d27bc1: prevent duplicate source-row text.
- cb8b183: portrait Studio Mode Preview/Program; e187b85: lineage, AI disclosure, channel downloads.

## 4. Canvas, streaming and recording design

Primary implementation: frontend/widgets/OBSBasic_DualStream.cpp.hpp; declarations in OBSBasic.hpp. Key functions observed on compatibility branch: InitVerticalCanvasVideo, GetVerticalScene, FindVerticalScene, RefreshVerticalScenes, SelectVerticalScene, RefreshVerticalLayers, InitDualStreamDock, UpdateVerticalPreview, CommitVerticalProgram, TransitionVertical, SetVerticalStudioMode, ClearVerticalStudioState, StartMultiStream, StopMultiStream, StartStreamDestination, MultiStreamEnabled, MultiStreamActive, MultiStreamEditable, SaveMultiStreamSettings, CloneSourceWithFilters, UpdateCleanSourceRecording, StopCleanSourceRecording, StartDualRecording, StopVerticalRecording, StopDualRecordings. Related files: OBSBasic.cpp.hpp, OBSBasicControls.cpp.hpp, OBSBasic_Recording.cpp.hpp, OBSBasic_SceneCollections.cpp.hpp, OBSBasic_SceneItems.cpp.hpp, frontend/settings/OBSBasicSettings.cpp.hpp and .hpp.

Horizontal uses standard OBS Program. Portrait has its own scenes, source order, transform, crop, lock and visibility, with persistence in scene collections. Reuse the native transform editor, context menu and arrow nudges (Shift = 10 px); do not duplicate editor logic. Top-level drag reorder exists; full nested groups remain outstanding. Dock toggle must refresh immediately without losing layout.

Portrait Studio Mode: independent Preview and Program. CommitVerticalProgram creates a private scene-layout snapshot sharing source references. Cut via Transition Vertical; stream/record Program rather than Preview. Shared underlying properties/filters can affect both views. Persist Preview/Program IDs, not the private snapshot. Leaving Studio Mode selects Program scene. No portrait fades or T-bar yet.

Multistream: Settings → Stream contains RTMP(S) destinations with orientation, server, key, H.264 encoder and bitrate. Import old YouTube keys into profile settings once; never display/log keys. Normal Start/Stop Streaming controls aggregate when enabled; single-service fallback remains intact. Extra destinations use distinct H.264/AAC encoders and audio track 1; upload demand sums across destinations. Prefer available NVENC/Intel VA-API/Quick Sync where exposed, fallback x264. Shared encoder optimization is future work. Preserve singular OBS frontend output handle semantics; compatibility branch changes read-only streaming-active query, not routing. Guard start/stop while connecting and shutdown.

Master volume: 0–100% master gain across outgoing/recorded audio tracks, independent of individual source gain; persisted. Historical commit c9ba73b moves gain into production Rust callback; preserve actual processing rather than UI-only control.

Alpha recording: normal horizontal plus Record Both (paired H.264/AAC MKV, distinct paths/encoders). Compatibility branch: single Start/Stop Recording, preserve regular OBS profile encoder/format/path, status, API and hotkeys. Settings → Video optional vertical MKV companion (off by default), vertical folder in Output; companion follows normal horizontal lifecycle including API/hotkeys and unexpected stop. Disable pause when dual recording configured to avoid mismatched files. Clean-source output is independent.

Source clone: normal OBS group referencing original source, independent group filter chain and transform, no second capture device. Original properties/filters remain shared. Serialization uses OBS scene format; nested groups not fully editable in portrait yet.
Clean-source recording: selected input rendered on ephemeral canvas excluding other overlays; original source filters apply, separate clone filters do not. Fixed starting dimensions, own H.264/AAC MKV encoder, normal track 1 audio (not isolated gameplay), session poll 250 ms, stops with stream/record session. Validate capacity with concurrent outputs.

## 5. Plugin compatibility and browser

Preserve OBS C ABI regardless of implementation language. Do not replace module loader with rust-obs-plugins; its old fallback bindings risk ABI mismatch. Optional SDK only, exact headers/bindgen, layout checks, panic containment, ownership and independent cdylib tests. No wrapper vendored.
Implementation: frontend/plugin-manager/PluginModuleLoader.hpp, PluginModuleLoader_Linux.cpp.hpp, frontend/OBSStudioAPI.cpp.hpp; fixtures ci/plugin-compat/{stock-filter.c,load-filter.c,compare-symbols.py}.
Current Linux user layout ~/.local/share/obs-studio/plugins/<name>/<name>.so + data. Legacy ~/.config/obs-studio/plugins/<name>/bin/64bit/<name>.so + data. SIMULCAST_OBS_PLUGINS_PATH for separate current layout; OBS_LEGACY_PLUGINS_PATH + OBS_LEGACY_PLUGINS_DATA_PATH; older OBS_PLUGINS_PATH + OBS_PLUGINS_DATA_PATH accepted pair. Do not copy system libobs or core modules. Architecture/Qt/FFmpeg dependencies must match. Tools → Plugin Manager and logs for diagnosis. Stock Fedora SDK fixture compiled independently and run against stock and Simulcast libobs; symbol checks diagnostic only.
Browser: enable existing obs-rust/obs-browser submodule, source ID browser_source, inherited permissions and CEF packaging. Fedora CEF 8078-r2 SHA256 dc463c360d4b8e4eb482fa78a5b92b996b053a4a8650dba3052c89390430ac9c. No duplicate source implementation: portrait enumerates registered input types. Keep sandboxing; no insecure workaround. Wayland custom docks have limitations. Validate transparent HTML, interactions, audio, both canvases, Studio Mode; runtime not proven by file checks.

## 6. Background removal: separate implementations

CPU/ONNX: plugins/rust-background-removal/{src/lib.rs,src/filter.rs,src/inference.rs,src/clear.c,build.rs}. Effect filter AI Background Removal (Simulcast). Pinned MediaPipe landscape model 256×144, onnx-community/mediapipe_selfie_segmentation_landscape revision 2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3, model SHA256 7a0adcfdb1715d3b0ff0f61486d8a181c33f4a208343a5abfccbc6540e872d24. Bundled, no runtime download. ONNX Runtime >=1.22 CPU default; optional CUDA/DirectML/CoreML are build targets, not validated runtime providers. Asynchronous bounded queue, GPU staging readback once per video tick; previews/canvases reuse frame/mask. Threshold and feather controls. Exact OBS_INCLUDE_DIR, OBS_CONFIG_DIR, OBS_LIB_DIR, SIMULCAST_MODEL_PATH, SIMULCAST_ORT_PATH or ORT_DYLIB_PATH. No stale pre-generated ABI bindings.

GPU: plugins/simulcast-gpu-background-removal/{src/lib.rs,src/graph.rs,src/pipeline.rs,src/cadence.rs,src/weights.rs,data/mediapipe.effect,data/compositor.effect,CMakeLists.txt}. AI Background Removal (GPU), adapted from royshil/obs-backgroundremoval commit d177d33197099dbeb5ccd430923de39fb941ea19. Preserved 55 weight/bias arrays as Rust data, MediaPipe network/compositor. GPU pass scheduling in Rust, OBS shaders, no CPU readback/ORT/CUDA SDK. Transparent or optional background blur with strength/focus. GPL-3.0-or-later port and Apache-2.0 weights with attribution. Separate from ONNX plugin.
Performance: cache parameter/technique lookups and same-tick inference. Default mask cadence 30 FPS, alternatives every frame/20/15; source capture, blur and composition full-rate. Refresh mask after resizing, hide/show, failure or age >125 ms. Lower cadence may lag moving edges, not intentionally buffer camera. No quantified hardware speedup or parity claim.
Both filters draw nothing when no usable mask rather than intentionally expose unfiltered source. A valid mask may still be wrong; not a privacy guarantee. Disabling shows original. Keep pipeline independent of host stream/canvas routing. Measure real end-to-end latency and GPU utilization before optimizing further; test alpha/color, shader compilation, blur, clones, simultaneous previews/outputs.

## 7. macOS browser and virtual camera

Branch feature/macos-browser-virtualcam; docs/macos-browser-virtualcam.md; .github/workflows/simulcast-macos-dev.yml; ci/simulcast_macos_profiles.py; plugins/mac-virtualcam/src/camera-extension/{CMakeLists.txt,OBSCameraProviderSource.swift,cmake/macos/Info.plist.in}; plugins/mac-virtualcam/src/obs-plugin/{CMakeLists.txt,plugin-main.mm.hpp}.
Browser: existing pinned module plus Apple Silicon CEF 8078 SHA256 2ddd78dd796af956728984747aaba4d859658354f00c6035be975b9f322f95cb. CI intended to check framework/module, four branded helpers, signatures, arm64 executable and DMG. No claim of actual workflow success from this review.
Modern virtual camera: Core Media I/O system extension on macOS 13+, distinct branded bundle and device/source/sink IDs; don't remove official OBS legacy DAL or use it as substitute. Trusted distribution requires Developer ID Application signing, notarization, separate app and extension profiles for same Team ID and app group TEAMID.io.github.notadeveloper4665.SimulcastStudio. App bundle io.github.notadeveloper4665.SimulcastStudio; extension suffix .mac-camera-extension. Actions secrets: MACOS_SIGNING_IDENTITY, MACOS_SIGNING_CERT, MACOS_SIGNING_CERT_PASSWORD, optional MACOS_KEYCHAIN_PASSWORD, MACOS_NOTARIZATION_USERNAME, MACOS_NOTARIZATION_PASSWORD, MACOS_SIGNING_PROVISIONING_PROFILE, MACOS_CAMERA_PROVISIONING_PROFILE. Never store values in repo or chat. Docs say no secrets configured by change; missing prerequisites mean browser-only, not functional virtual camera. Check BUILD-INFO.txt and notarization/stapling before claims. Never disable SIP/system extension security. Test /Applications installation, permission approval, actual video in conferencing app, start/stop, horizontal Program, coexistence with OBS. Physical webcam input requires separate permission/capture verification. Keep alpha unchanged until validated.

## 8. Build and verification

Fedora 44 x86-64: RPM Fusion Free x264, dnf builddep obs-studio, CMake/Ninja, recursive submodules, pinned Rust (compatibility workflow 1.98.1), uv for AppImage. cmake --preset fedora -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF -DOBS_VERSION_OVERRIDE=33.0.0 -DOBS_RUST_VERSION=0.1.0 ; cmake --build build_fedora --parallel 4 ; cpack --config build_fedora/CPackConfig.cmake -G RPM -B build_fedora/rpm. AppImage: DESTDIR staging, build-aux/appimage/assemble_fedora_appdir.py, pinned appimagetool; host GPU/VAAPI drivers, Fedora 44-compatible target; KDE Wayland ScreenCast portal. RPM and AppImage share inherited configuration; back up profiles/collections before switching channels.
Compatibility workflow .github/workflows/simulcast-plugin-compat.yml: Fedora 44 container on Ubuntu runner, enable ENABLE_SIMULCAST_BACKGROUND_REMOVAL, ENABLE_SIMULCAST_GPU_BACKGROUND_REMOVAL, ENABLE_BROWSER; pin/check CEF and model; cadence rustc tests, cargo test --lib and release build, package requires ORT >=1.22, install RPM, check identity/version/ldd, verify core module and CEF files, stock SDK ABI tests. Branch artifact only, not release. README reports run 45865538881 successful; not re-verified here.

Regression matrix before merging or releasing:
- Two horizontal and two vertical RTMP destinations, independent start/stop and failure, aggregate controls, fallback, studio transitions; local FFmpeg listeners and ffprobe verify resolution/audio.
- Scene collection reload, dock reopen, transforms, source order/locks, no duplicate row rendering.
- Normal/dual/clean recording paths, hotkeys/API, pause, unexpected stop, shutdown, simultaneous streaming; inspect files.
- Webcam, microphone, capture, master gain, GPU encoding and capacity; benchmark target hardware.
- ABI tests plus real Move Transition, Advanced Scene Switcher, Source Record, Shaderfilter, WebSocket. Browser interactions and rendering.
- GPU/CPU filters under multiple render views, cadence changes, resize, motion, failure, hide/show; confirm no unintended queue latency.
- Platform packaging, signatures, physical input and actual virtual output separately.

## 9. Decision protocol for future AI agents

1. Read this file and linked authoritative documentation; fetch current branch/commit before changing behavior. Never assume all development features are in alpha.
2. Identify owning subsystem and reuse upstream behavior; preserve ABI, scene persistence, stream routing and regular recording semantics.
3. Inspect diffs and tests for previous regressions before implementing. Avoid unrelated rewrites and preserve licensing.
4. Keep experimental work separate until real validation; report exact build/source and distinction between code, CI, package, and real hardware.
5. Update this file with date, commit/PR, rationale, touched files, test evidence, platform availability, unresolved risks and rollback strategy. Update public README links only after verified artifacts exist. Do not treat historical instructions in older guide sections as current when later sections supersede them.
6. Never expose credentials; never ask user to disable platform security for compatibility.

## 10. Sources and review scope

- [Master README](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/e187b859d13e4f89fbd30c1b1295af2ebc54f378/README.md)
- [Build and dual-stream guide](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/master/docs/simulcast-studio.md)
- [Compatibility decisions](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/feature/obs-plugin-compatibility/docs/obs-plugin-compatibility.md)
- [ONNX filter](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/feature/obs-plugin-compatibility/plugins/rust-background-removal/README.md)
- [GPU filter](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/feature/obs-plugin-compatibility/plugins/simulcast-gpu-background-removal/README.md)
- [macOS development](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/feature/macos-browser-virtualcam/docs/macos-browser-virtualcam.md)
- [Compatibility workflow](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/feature/obs-plugin-compatibility/.github/workflows/simulcast-plugin-compat.yml)
- [Master commits](https://github.com/NotADeveloper4665/Simulcast-Studio/commits/master/), [compatibility comparison](https://github.com/NotADeveloper4665/Simulcast-Studio/compare/master...feature/obs-plugin-compatibility), [macOS comparison](https://github.com/NotADeveloper4665/Simulcast-Studio/compare/master...feature/macos-browser-virtualcam).

Review covered README, four feature/build guides plus macOS guide, master Simulcast-specific commit subjects, branch comparisons, compatibility workflow excerpt, and dual-stream implementation function inventory. Not an exhaustive line-by-line audit of every source/diff, inherited upstream commit, PR discussion, workflow log or hardware test. Historical branch counts and download status can change. Treat remaining details as verification tasks rather than assumed facts.
