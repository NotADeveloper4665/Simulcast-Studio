# Contributing to Simulcast Studio

Simulcast Studio is maintained independently of OBS Studio and OBS Studio Rust.

## Development workflow

- Base Simulcast-specific changes on `simulcast-appimage`.
- Keep pull requests focused; describe the problem, proposed change, and verification.
- Preserve existing libobs APIs, plugin compatibility, settings, and scene collections.
- Include relevant build and regression-test results. Clearly mark anything not tested.
- Do not include credentials, streaming keys or unredacted private logs.
- The maintainer approves merges and release publication.

See the [project build and validation guide](https://github.com/NotADeveloper4665/Simulcast-Studio/blob/simulcast-appimage/docs/simulcast-studio.md). For inherited code style and technical practices, consult [the original contribution guidelines](CONTRIBUTING.upstream.md) and [CODESTYLE.md](CODESTYLE.md).

Upstream copyright notices, licensing and attribution must be preserved. Simulcast-specific reports belong in [this repository](https://github.com/NotADeveloper4665/Simulcast-Studio/issues); upstream issues should be reported upstream only when independently reproduced there.
