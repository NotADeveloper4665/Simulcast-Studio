<p align="center">
  <img src="docs/images/simulcast.svg" alt="Simulcast Studio logo" width="128">
</p>

# Simulcast Studio

Simulcast Studio is a Linux-focused recording and livestreaming application based on [OBS Studio Rust](https://github.com/obs-rust/obs-studio) and [OBS Studio](https://github.com/obsproject/obs-studio). It is designed for creators who want independent horizontal and vertical layouts in one application.

**Project status:** Active development. Fedora 44 x86-64 is the current packaging target. The active Simulcast development branch is [`simulcast-appimage`](https://github.com/NotADeveloper4665/Simulcast-Studio/tree/simulcast-appimage); the inherited upstream source is retained separately on `master`.

## Current development milestone

- Independent horizontal and portrait canvases, with an interactive vertical preview and source layout controls.
- Multiple stream destinations, including separate horizontal and vertical YouTube RTMPS keys.
- Dual video recording with separate destination folders, independent H.264/AAC encoders and MKV output.
- Hardware H.264 encoder detection and software fallback.
- Master volume controls for outgoing and recorded audio.
- Fedora RPM and AppImage packaging.
- Latest source change: dual recording paths in **Settings → Output**, with a single **Record Both / Stop Both** control beneath normal recording. This change is committed but the updated application package has not yet been verified.

## Development builds

See [GitHub Actions](https://github.com/NotADeveloper4665/Simulcast-Studio/actions) and [Releases](https://github.com/NotADeveloper4665/Simulcast-Studio/releases) for available artifacts. A source commit is not evidence that a new RPM has been built or published.

For an existing Fedora 44 RPM:

```bash
sudo dnf install ./simulcast-studio-*.fc44.x86_64.rpm
simulcast-studio
```

For source builds, clone the development branch:

```bash
git clone --branch simulcast-appimage --recurse-submodules https://github.com/NotADeveloper4665/Simulcast-Studio.git
cd Simulcast-Studio
```

See the [Fedora build instructions and dual-stream setup](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/simulcast-appimage/docs/simulcast-studio.md) for dependencies, AppImage/RPM packaging and validation.

## Roadmap

- Validate simultaneous horizontal and vertical streaming and recording on target hardware.
- Improve recording and streaming configuration, diagnostics and hardware encoder coverage.
- Evaluate a native Rust ONNX webcam background-removal filter independently before integrating it into the application.

## Upstream and license

Simulcast Studio preserves upstream source history, copyright notices and license obligations. The upstream OBS APIs, plugin identifiers and existing configuration paths remain intact for compatibility.

- [OBS Studio Rust](https://github.com/obs-rust/obs-studio)
- [OBS Studio](https://github.com/obsproject/obs-studio)
- [Inherited Rust fork README](README.obs-rust.md)
- [Original OBS README](README.upstream.md)
- [GPL-2.0-or-later license](COPYING)

Report Simulcast-specific bugs and feature requests through [this repository's issues](https://github.com/NotADeveloper4665/Simulcast-Studio/issues).
