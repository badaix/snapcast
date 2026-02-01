# Santcasp

Prebuilt [snapclient and snapserver](https://github.com/snapcast/snapcast) binaries for Linux, macOS and Windows.

> This is a fork of [Snapcast](https://github.com/snapcast/snapcast) by [Johannes Pohl](https://github.com/badaix).
> All credit for the original software goes to the upstream project and its [contributors](https://github.com/snapcast/snapcast/graphs/contributors).

## What is this?

Santcasp provides ready-to-use **snapclient** and **snapserver** packages so you don't have to build from source. The upstream project distributes packages via its own CI/release process; this fork offers additional per-distro builds and a Windows binary.

Client and server are packaged separately — install only what you need.

For documentation on how snapclient works, configuration, and audio backends, see the [upstream README](https://github.com/snapcast/snapcast#readme).

## Downloads

Grab the latest builds from the [Releases](https://github.com/lollonet/santcasp/releases) page.

### Available platforms

| Platform | Arch | Client | Server |
|----------|------|--------|--------|
| Ubuntu 24.04 | amd64 | `.deb`, `.tar.gz` | `.deb`, `.tar.gz` |
| Debian 12 (bookworm) | amd64 | `.deb`, `.tar.gz` | `.deb`, `.tar.gz` |
| Debian 13 (trixie) | amd64 | `.deb`, `.tar.gz` | `.deb`, `.tar.gz` |
| macOS | arm64 | `.tar.gz` | `.tar.gz` |
| Windows | x64 | `.zip` | — * |

\* Snapserver does not compile on Windows ([upstream limitation](https://github.com/snapcast/snapcast/issues/1380)).

### Install (.deb)

```bash
# Client
sudo dpkg -i santcasp_<version>_<distro>_amd64.deb
sudo apt-get install -f

# Server
sudo dpkg -i santcasp-server_<version>_<distro>_amd64.deb
sudo apt-get install -f
```

### Install (tar.gz / zip)

Extract and run the binary directly. On macOS, the bundled `libs/` directory must stay next to the binary. On Windows, keep all `.dll` files in the same directory as `snapclient.exe`.

## Versions

- **v0.34.0** — stable release, matches [upstream v0.34.0](https://github.com/snapcast/snapcast/releases/tag/v0.34.0)
- **v0.35.0-dev** — pre-release from the upstream `develop` branch

## Build info

- Boost 1.90.0
- Linux: built per-distro in Docker containers for correct library linking
- macOS: arm64 (Apple Silicon), CoreAudio backend, bundled Homebrew dylibs
- Windows: native MSVC 2022 build, vcpkg dependencies, WASAPI backend, SSL disabled (client only — snapserver is [not supported on Windows](https://github.com/snapcast/snapcast/issues/1380))

## Upstream

This project is a fork of **Snapcast** — a multiroom client-server audio player where all clients are time synchronized with the server to play perfectly synced audio.

- Upstream repo: https://github.com/snapcast/snapcast
- Author: [Johannes Pohl](https://github.com/badaix)
- Upstream releases: https://github.com/snapcast/snapcast/releases

## License

GPLv3+ — same as upstream. See [LICENSE](LICENSE).
