# OBS plugin compatibility

Keep the existing OBS ABI and multistream output routing. This branch changes plugin discovery, diagnostics and the frontend's read-only streaming-active query. It does not redefine the singular streaming-output handle or redirect plugin controls to a different destination.

## Linux installation

Use compatible, trusted Linux binaries. Current layout: ~/.local/share/obs-studio/plugins/<name>/<name>.so with resources in <name>/data. Legacy layout: ~/.config/obs-studio/plugins/<name>/bin/64bit/<name>.so with resources in <name>/data. Restart and inspect Tools → Plugin Manager and the log.

Optional separate plugin directory: SIMULCAST_OBS_PLUGINS_PATH="$HOME/.local/share/simulcast-plugins" simulcast-studio (current layout). Explicit legacy environment paths are OBS_LEGACY_PLUGINS_PATH and OBS_LEGACY_PLUGINS_DATA_PATH; older OBS_PLUGINS_PATH plus OBS_PLUGINS_DATA_PATH are accepted as a pair.

Do not copy system libobs or bundled OBS core modules into Simulcast. Plugins must match architecture and compatible Qt/FFmpeg dependencies. Flatpak extensions and macOS bundles need separate validation.

## Gates

Fedora CI compiles a filter and harness against the stock distribution OBS SDK, tests against stock libobs, then runs the exact same binaries against Simulcast's installed libobs. Symbol comparison is diagnostic, not proof of complete ABI compatibility.

Before merging: verify actual independent horizontal and vertical outputs (two each), start/stop, single-service fallback, dual recording, and Studio Mode. Test unmodified Move Transition, Advanced Scene Switcher, Source Record, Shaderfilter, and WebSocket. Verify Qt dependencies and frontend behavior. No release is published by the branch workflow.

## Built-in Browser Source

The existing pinned obs-rust/obs-browser submodule is registered as an OBS core module. The compatibility Fedora RPM now configures ENABLE_BROWSER=ON with upstream CEF 8078-r2 (SHA-256 dc463c360d4b8e4eb482fa78a5b92b996b053a4a8650dba3052c89390430ac9c, matching CMakePresets.json). It packages the browser module, helper, CEF runtime and resources using inherited CMake rules. The vertical source picker already enumerates registered input types and accepts the target portrait scene; no special source ID or duplicate browser implementation is necessary. Keep browser_source and upstream permissions intact. Do not disable Chromium sandboxing as a workaround. CI checks installed files and dynamic dependencies. Interactive rendering, local HTML, transparent overlays, audio, Interact, both canvases and portrait Studio Mode still require runtime tests; custom browser docks on Wayland are a separate limitation. AppImage, DEB and macOS release workflows are unchanged until validated.

## Rust plugin development SDK assessment

Reference: https://github.com/bennetthardwick/rust-obs-plugins (obs-wrapper 0.4.1, obs-sys 0.3.0, GPL-2.0). Its Module/Sourceable traits, source builders, registration macro and ownership conventions are useful for a Rust plugin SDK and test fixtures. It is not a host-side binary compatibility layer: standard OBS plugins must continue to load through libobs's C ABI regardless of implementation language. Do not replace or wrap the existing loader with this crate. Its bindgen script points to older OBS header locations and falls back to pre-generated bindings; that fallback can silently mismatch the current ABI. Before adopting code, review GPL compatibility and preserve notices, generate bindings strictly against the exact supported SDK, verify C/Rust struct layouts and callbacks, contain panics at FFI boundaries, and test an independently compiled Rust cdylib through the existing module loader. Keep this SDK optional and separate from multistream, encoders, output handles and canvas routing. No external wrapper code was vendored in this change.

## Source clones and clean recordings

Right-click a video source in either editor → Clone Source (Independent Filters). The clone is a normal OBS group containing a reference to the capture, with its own scene transform and group filter chain. The original capture is not opened twice. Filters added to the clone group do not change the original. Filters or properties applied directly to the underlying shared source still affect both. This uses the OBS scene serialization format; the vertical editor does not yet expose nested group expansion.

Settings → Video → Clean Source Recording selects one input and an output folder. When streaming or recording begins, an independent ephemeral canvas renders that source without other scene overlays and records H.264/AAC MKV using the existing dual-recording encoder settings. Audio is the normal mix (track 1), not isolated game audio. Original source-level filters still apply; the clone's separate group filters do not. The output has its own encoder and stops when the streaming/recording session ends. Initial implementation uses a 250 ms session poll and fixed source dimensions at start. Verify duplicate capture avoidance, filter isolation, persistence, both orientations, simultaneous outputs, stop/shutdown and GPU capacity before merging.

## Unified recording control

Controls now has a single Start Recording / Stop Recording button. It preserves the standard horizontal OBS profile recording path, encoder, format, status, hotkeys and frontend API. Settings → Video → Enable vertical video recording (off by default) starts a companion portrait MKV when the horizontal recording starts and stops it when horizontal recording stops, including frontend API and hotkey initiated recordings. The vertical destination folder remains under Settings → Output. The old separate horizontal folder field is retired; the regular Output recording path controls horizontal files. Pause is disabled while dual recording is configured to avoid mismatched files. Clean-source recording remains an independent optional output. Verify normal and dual recording, unexpected horizontal stop, output failure, shutdown, and recording alongside multistream before merging.
