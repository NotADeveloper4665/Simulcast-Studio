# OBS plugin compatibility

Keep the existing OBS ABI and multistream output routing. This branch changes plugin discovery, diagnostics and the frontend's read-only streaming-active query. It does not redefine the singular streaming-output handle or redirect plugin controls to a different destination.

## Linux installation

Use compatible, trusted Linux binaries. Current layout: ~/.local/share/obs-studio/plugins/<name>/<name>.so with resources in <name>/data. Legacy layout: ~/.config/obs-studio/plugins/<name>/bin/64bit/<name>.so with resources in <name>/data. Restart and inspect Tools → Plugin Manager and the log.

Optional separate plugin directory: SIMULCAST_OBS_PLUGINS_PATH="$HOME/.local/share/simulcast-plugins" simulcast-studio (current layout). Explicit legacy environment paths are OBS_LEGACY_PLUGINS_PATH and OBS_LEGACY_PLUGINS_DATA_PATH; older OBS_PLUGINS_PATH plus OBS_PLUGINS_DATA_PATH are accepted as a pair.

Do not copy system libobs or bundled OBS core modules into Simulcast. Plugins must match architecture and compatible Qt/FFmpeg dependencies. Flatpak extensions and macOS bundles need separate validation.

## Gates

Fedora CI compiles a filter and harness against the stock distribution OBS SDK, tests against stock libobs, then runs the exact same binaries against Simulcast's installed libobs. Symbol comparison is diagnostic, not proof of complete ABI compatibility.

Before merging: verify actual independent horizontal and vertical outputs (two each), start/stop, single-service fallback, dual recording, and Studio Mode. Test unmodified Move Transition, Advanced Scene Switcher, Source Record, Shaderfilter, and WebSocket. Verify Qt dependencies and frontend behavior. No release is published by the branch workflow.
