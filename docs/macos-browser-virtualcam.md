# macOS browser and virtual camera development

The macOS browser/virtual-camera branch is based on the published alpha source plus documentation. It does not merge the separate Fedora compatibility branch or change streaming, recording, or canvas routing.

Browser Source uses the existing pinned obs-rust/obs-browser module and CEF 8078 Apple Silicon dependencies (SHA-256 2ddd78dd796af956728984747aaba4d859658354f00c6035be975b9f322f95cb from CMakePresets.json). CI checks the browser module, Chromium framework, four branded helper apps, code signatures, arm64 executable, and DMG. Browser rendering, Interact, audio and both canvases need real Mac testing.

The macOS 13+ camera uses the existing Core Media I/O system extension; the legacy DAL is not a substitute on current macOS. Simulcast uses its own bundle and camera device/source/sink IDs. The branded modern path will not uninstall the original OBS legacy DAL. The extension is enabled in distributed development packages only when signing, notarization and both profiles are available. Never disable SIP or system-extension security to install this package.

## Apple credentials (maintainer setup)

A Developer ID Application certificate and Apple Developer team are required for trusted direct distribution. Configure repository Actions secrets, not source files or chat:

- MACOS_SIGNING_IDENTITY (Developer ID Application identity including Team ID)
- MACOS_SIGNING_CERT (base64 PKCS12 certificate including private key)
- MACOS_SIGNING_CERT_PASSWORD
- MACOS_KEYCHAIN_PASSWORD (optional)
- MACOS_NOTARIZATION_USERNAME (Apple ID)
- MACOS_NOTARIZATION_PASSWORD (app-specific password)
- MACOS_SIGNING_PROVISIONING_PROFILE (base64 Developer ID profile for io.github.notadeveloper4665.SimulcastStudio with system-extension install capability)
- MACOS_CAMERA_PROVISIONING_PROFILE (base64 Developer ID profile for io.github.notadeveloper4665.SimulcastStudio.mac-camera-extension)

Both profiles must authorize the same team and app group `TEAMID.io.github.notadeveloper4665.SimulcastStudio`. The workflow validates the profile identities, installs them separately, assigns each to its target, and requires Apple notarization acceptance and DMG stapling before uploading a notarized artifact. Missing prerequisites mean browser-only, not a falsely advertised functional virtual camera. No secrets have been configured by this change.

## Testing and distribution

Download the development DMG from the macOS browser and virtual camera Actions workflow. Check BUILD-INFO.txt for actual signing and camera status. A signed, notarized build with the extension must be installed in /Applications; start Virtual Camera, approve the extension in macOS Settings if prompted, and select Simulcast Studio Virtual Camera in a video conferencing application. Verify video, start/stop, horizontal program and coexistence with official OBS. No runtime activation is claimed from a bundle smoke test. The alpha release DMG remains unchanged until this candidate passes and is promoted.

Apple references: https://developer.apple.com/documentation/coremediaio/creating-a-camera-extension-with-core-media-i-o and https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution/
