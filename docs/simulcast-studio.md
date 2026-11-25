# Simulcast Studio

## Build the Fedora 44 x86-64 AppImage

Enable RPM Fusion Free for x264. Use Fedora 44 with the pinned Rust toolchain,
uv, and the upstream-pinned appimagetool from build-aux/appimage/build.py.

```sh
sudo dnf install dnf5-plugins cmake ninja-build extra-cmake-modules \
  clang git rustup rpm-build x264-devel patchelf squashfs-tools pulseaudio-utils
sudo dnf builddep obs-studio
git submodule update --init plugins/obs-browser plugins/obs-websocket
cmake --preset fedora -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF -DOBS_VERSION_OVERRIDE=33.0.0 -DOBS_RUST_VERSION=0.1.0
cmake --build build_fedora --parallel 4
DESTDIR="$PWD/Simulcast.AppDir" cmake --install build_fedora --strip
uv run --no-project build-aux/appimage/assemble_fedora_appdir.py Simulcast.AppDir
ARCH=x86_64 appimagetool --no-appstream Simulcast.AppDir Simulcast-Studio-x86_64.AppImage
```

The AppImage targets Fedora 44 and uses the host's graphics and VA-API drivers.
Run it directly or use `--appimage-extract-and-run` if FUSE is unavailable.
On KDE Wayland, use Plasma's ScreenCast portal for screen and window capture.

## Build the Fedora 44 x86-64 RPM

After the native build above, run:

```sh
cpack --config build_fedora/CPackConfig.cmake -G RPM -B build_fedora/rpm -D CPACK_PACKAGE_VERSION=0.1.0 -D CPACK_RPM_PACKAGE_RELEASE=2
sudo dnf install ./build_fedora/rpm/simulcast-studio-0.1.0-2.fc44.x86_64.rpm
simulcast-studio
```

The RPM and AppImage use the same scene collections and profiles as the
previous Simulcast Studio build.

## Set up both layouts

1. Create your horizontal scenes and sources normally. Open **Docks → Dual
   Stream** to show the portrait preview alongside the normal preview. The
   horizontal feed follows OBS Program, including studio-mode
   transitions.
2. Use **Add Source** to create or reuse sources in the portrait scene.
   Select and drag, resize, rotate, or Alt-drag to crop directly in the
   inline portrait canvas. **Layout** provides numeric controls and
   **Properties** opens source settings. **Raise** and **Lower** change
   stacking. The portrait layout is saved in the scene collection.
3. In **Settings → Output**, check the hardware H.264 status beside the stream
   encoder selector. It reports encoders that the loaded OBS plugins and Linux
   driver exposed. New destinations and dual recordings prefer a detected
   hardware encoder such as NVIDIA NVENC, Intel VA-API or Quick Sync; otherwise
   they fall back to x264. The encoder selector still lets you choose another
   available encoder.
4. Set the YouTube ingest URL and separate horizontal and vertical keys in
   **Settings → Stream → YouTube Dual Stream Destinations**. Saved keys load as
   destinations in the Dual Stream dock at startup. Other services can still
   be added there. Assign destinations to either orientation and select an
   H.264 encoder and bitrate.
   Select a destination and press **Start** or **Stop**, or use **Start All**
   and **Stop All Streams** for the configured destinations. For YouTube,
   create one live event, set the vertical preview to **Encoder** and **RTMP(S)**
   in Live Control Room, then use the horizontal event key for Horizontal and
   Start both before going live in Studio. Both outputs use the same event's
   RTMPS ingest URL and keep their horizontal/vertical canvas assignment
   independent.
5. Under **Settings → Output → Dual Video Recording**, choose separate
   **Horizontal folder** and **Vertical folder** paths and apply the changes.
   In Controls, press **Record Both** under **Start Recording** to save the two
   canvases as timestamped MKV files with H.264 video and AAC audio. The same
   button becomes **Stop Both** while either recording is active; press it to finish
   both files. Each recording has its own encoder and file path. Recording
   folders are remembered between launches.
6. Extra stream destinations use OBS audio track 1 and a separate H.264 encoder
   and AAC encoder per destination. Each live destination consumes its
   configured upload bitrate. Destination settings and stream keys stay in
   memory for the current application session.
7. Use **Master Volume** in the Audio Mixer to adjust all outgoing and
   recorded audio tracks from 0% to 100%. Individual source settings stay
   where you set them, and the master setting is saved for the next launch.

## Verify the AppImage

Use an existing profile and scene collection with screen capture, a camera,
and a microphone. Verify all three work in the packaged application. Create
a portrait layout that moves or crops the same camera differently, then close
and reopen the app to verify that the layout survives a restart.

To check both video feeds without publishing, listen on two local ports with
FFmpeg in separate terminals.

```sh
ffmpeg -listen 1 -i rtmp://127.0.0.1:19351/live/horizontal -c copy horizontal.flv
ffmpeg -listen 1 -i rtmp://127.0.0.1:19352/live/vertical -c copy vertical.flv
```

Configure the matching server URLs ending in `/live` and use `horizontal`
and `vertical` as the stream keys. Start both targets. Change the OBS Program
scene and verify the vertical layout remains in place. Stop each destination
independently and confirm the other keeps receiving frames and audio. Use
`ffprobe` to verify the configured horizontal resolution and 1080 × 1920
vertical resolution. Repeat with a third listener assigned to either
orientation to verify that two destinations can use the same canvas.
