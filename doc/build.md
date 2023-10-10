# Build Snapcast

## Clone the Snapcast repository

To do this, you need git.  
For Debian derivates (e.g. Raspberry Pi OS, Debian, Ubuntu, Mint):

```sh
sudo apt-get install git
```

For Arch derivates:

```sh
sudo pacman -S git
```

For FreeBSD:

```sh
sudo pkg install git
```

Clone Snapcast:

```sh
git clone https://github.com/badaix/snapcast.git
```

## Install dependencies

Snapcast depends on boost 1.74 or higher. Since it depends on header only boost libs, boost does not need to be installed, but the boost include path must be set properly: download and extract the latest boost version and tell `cmake` the path using the `-DBOOST_ROOT` flag: `cmake -DBOOST_ROOT=/path/to/boost_1_8x_0`.

### For Debian derivates (e.g. Raspberry Pi OS, Debian, Ubuntu, Mint)

```sh
sudo apt-get install build-essential cmake
sudo apt-get install libasound2-dev libpulse-dev libvorbisidec-dev libvorbis-dev libopus-dev libflac-dev libsoxr-dev alsa-utils libavahi-client-dev avahi-daemon libexpat1-dev
```

### For Arch derivates

```sh
sudo pacman -S base-devel cmake
sudo pacman -S alsa-lib avahi libvorbis opus-dev flac libsoxr alsa-utils boost expat
```

### For Fedora (and probably RHEL, CentOS, & Scientific Linux, but untested)

```sh
sudo dnf install @development-tools cmake
sudo dnf install alsa-lib-devel avahi-devel gcc-c++ libatomic libvorbis-devel opus-devel pulseaudio-libs-devel flac-devel soxr-devel libstdc++-static expat-devel boost-devel
```

### For FreeBSD

```sh
sudo pkg install alsa-lib pulseaudio cmake gmake gcc bash avahi libogg libvorbis opus flac libsoxr pkgconfig
```

### For macOS

*Warning:* macOS support is experimental

 1. Install Xcode from the App Store
 2. Install [Homebrew](http://brew.sh)
 3. Install the required libs

```sh
brew install pkgconfig libsoxr expat flac libvorbis boost opus
```

## Build Snapclient and Snapserver

Create a `build` directory in the Snapcast src-root directory (`<snapcast dir>`) and `cd` into it:

```sh
cd <snapcast dir>
mkdir build
cd build
```

Build Snapcast. If you haven't installed boost, but downloaded and extracted the sources, you must point cmake to the boost root directoty, otherwise the part starting with `-DBOOST_ROOT=...` can be omitted.

Other flags that can be passed to `cmake`:

- `-DBUILD_CLIENT=<ON|OFF>`: build the client: yes or no
- `-DBUILD_SERVER=<ON|OFF>`: build the server: yes or no
- `-DBUILD_WITH_PULSE=<ON|OFF>`: build with pulse audio support: yes or no

```sh
cmake .. -DBOOST_ROOT=/path/to/boost_1_8x_0
cmake --build .
```

Binaries will be created in `<snapcast dir>/bin`:

```sh
<snapcast dir>/bin/snapclient
<snapcast dir>/bin/snapserver
```

## Windows (vcpkg)

Prerequisites:

 1. CMake
 2. Visual Studio 2017 or 2019 with C++

Set up [vcpkg](https://github.com/Microsoft/vcpkg)

Install dependencies

```sh
vcpkg.exe install libflac libvorbis soxr opus boost-asio --triplet x64-windows
```

Build

```sh
cd <snapcast dir>
mkdir build
cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg_dir>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Build packages

### Debian packages

Debian packages can be made with the following steps. You can switch pulse audio support on or off by passing `-DBUILD_WITH_PULSE=ON` or `OFF` in the last step:

```sh
sudo apt-get install debhelper python3
cd <snapcast dir>
ln -s extras/package/debian debian
debian/changelog_md2deb.py changelog.md  > debian/changelog
fakeroot make -f debian/rules CMAKEFLAGS="-DBOOST_ROOT=/path/to/boost/boost_1_8x_0 -DCMAKE_BUILD_TYPE:STRING=Release -DBUILD_WITH_PULSE=OFF" binary
```

The resulting debian packages are created in the parent direcory of `<snapcast dir>`

### Gentoo (native)

Snapcast is available under Gentoo's [Portage](https://wiki.gentoo.org/wiki/Portage) package management system. Portage utilises `USE` flags to determine what components are built on compilation. The available options are...

```sh
equery u snapcast
[ Legend : U - final flag setting for installation]
[        : I - package is installed with flag     ]
[ Colors : set, unset                             ]
 * Found these USE flags for media-sound/snapcast-9999:
 U I
 + - avahi       : Build with avahi support
 + + client      : Build and install Snapcast client component
 + - flac        : Build with FLAC compression support
 + + server      : Build and install Snapcast server component
 - - static-libs : Build static libs
 - - tremor      : Build with TREMOR version of vorbis
 + - vorbis      : Build with libvorbis support
```

These can be set either in the [global configuration](https://wiki.gentoo.org/wiki//etc/portage/make.conf#USE) file `/etc/portage/make.conf` or on a per-package basis (as root):

```sh
if [ ! -d "$DIRECTORY" ]; then
    mkdir /etc/portage/package.use/media-sound
fi
echo 'media-sound/snapcast client server flac
```

If for example you only wish to build the server and *not* the client then precede the server `USE` flag with `-` i.e.

```sh
echo 'media-sound/snapcast client -server
```

Once `USE` flags are configured emerge snapcast as root:

```sh
emerge -av snapcast
```

Starting the client or server depends on whether you are using `systemd` or `openrc`. To start using `openrc`:

```sh
/etc/init.d/snapclient start
/etc/init.d/snapserver start
```

To enable the serve and client to start under the default run-level:

```sh
rc-update add snapserver default
rc-update add snapclient default
```

### Android (Cross compile)

Clone [Snapdroid](https://github.com/badaix/snapdroid), which includes Snapclient as submodule:

```sh
git clone https://github.com/badaix/snapdroid.git
cd snapdroid
git submodule update --init --recursive
```

and execute `./gradlew build`, which will cross compile Snapclient and bundle it into the Snapdroid App.

### OpenWrt/LEDE (Cross compile)

To cross compile for OpenWrt, please follow the [OpenWrt flavored SnapOS guide](https://github.com/badaix/snapos/blob/master/openwrt/README.md)

### Buildroot (Cross compile)

To integrate Snapcast into [Buildroot](https://buildroot.org/), please follow the [Buildroot flavored SnapOS guide](https://github.com/badaix/snapos/blob/master/buildroot-external/README.md)
