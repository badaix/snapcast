# Build Snapcast

Clone the Snapcast repository. To do this, you need git.  
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

Snapcast depends on boost 1.70 or higher. Since it depends on header only boost libs, boost does not need to be installed, but the boost include path must be set properly: download and extract the latest boost version and add the include path, e.g. calling `make` with prepended `ADD_CFLAGS`: `ADD_CFLAGS="-I/path/to/boost_1_7x_0/" make`.  
For `cmake` you must add the path to the `-DBOOST_ROOT` flag: `cmake -DBOOST_ROOT=/path/to/boost_1_7x_0`

## Linux (Native)

Install the build tools and required libs:  
For Debian derivates (e.g. Raspberry Pi OS, Debian, Ubuntu, Mint):

```sh
sudo apt-get install build-essential
sudo apt-get install libasound2-dev libpulse-dev libvorbisidec-dev libvorbis-dev libopus-dev libflac-dev libsoxr-dev alsa-utils libavahi-client-dev avahi-daemon libexpat1-dev
```

Compilation requires gcc 4.8 or higher, so it's highly recommended to use Debian (Raspbian) Jessie.

For Arch derivates:

```sh
sudo pacman -S base-devel
sudo pacman -S alsa-lib avahi libvorbis opus-dev flac libsoxr alsa-utils boost expat
```

For Fedora (and probably RHEL, CentOS, & Scientific Linux, but untested):

```sh
sudo dnf install @development-tools
sudo dnf install alsa-lib-devel avahi-devel gcc-c++ libatomic libvorbis-devel opus-devel pulseaudio-libs-devel flac-devel soxr-devel libstdc++-static expat boost-devel
```

### Build Snapclient and Snapserver

`cd` into the Snapcast src-root directory:

```sh
cd <snapcast dir>
make
```

Install Snapclient and/or Snapserver:

```sh
sudo make installserver
sudo make installclient
```

This will copy the client and/or server binary to `/usr/bin` and update init.d/systemd to start the client/server as a daemon.

### Build Snapclient

`cd` into the Snapclient src-root directory:

```sh
cd <snapcast dir>/client
make
```

Install Snapclient

```sh
sudo make install
```

This will copy the client binary to `/usr/bin` and update init.d/systemd to start the client as a daemon.

### Build Snapserver

`cd` into the Snapserver src-root directory:

```sh
cd <snapcast dir>/server
make
```

Install Snapserver

```sh
sudo make install
```

This will copy the server binary to `/usr/bin` and update init.d/systemd to start the server as a daemon.

### Debian packages

Debian packages can be made with

```sh
sudo apt-get install debhelper
cd <snapcast dir>
fakeroot make -f debian/rules binary
```

If you don't have boost installed or in your standard include paths, you can call

```sh
fakeroot make -f debian/rules CPPFLAGS="-I/path/to/boost_1_7x_0" binary
```

## FreeBSD (Native)

Install the build tools and required libs:  

```sh
sudo pkg install alsa-lib pulseaudio gmake gcc bash avahi libogg libvorbis opus flac libsoxr
```

### Build Snapserver

`cd` into the Snapserver src-root directory:

```sh
cd <snapcast dir>/server
gmake TARGET=FREEBSD
```

Install Snapserver

```sh
sudo gmake TARGET=FREEBSD install
```

This will copy the server binary to `/usr/local/bin` and the startup script to `/usr/local/etc/rc.d/snapserver`. To enable the Snapserver, add this line to `/etc/rc.conf`:  

```ini
snapserver_enable="YES"
```

For additional command line arguments, add in `/etc/rc.conf`:

```ini
snapserver_opts="<your custom options>"
```

Start and stop the server with `sudo service snapserver start` and `sudo service snapserver stop`.

## Gentoo (native)

Snapcast is available under Gentoo's [Portage](https://wiki.gentoo.org/wiki/Portage) package management system.  Portage utilises `USE` flags to determine what components are built on compilation.  The available options are...

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

Starting the client or server depends on whether you are using `systemd` or `openrc`.  To start using `openrc`:

```sh
/etc/init.d/snapclient start
/etc/init.d/snapserver start
```

To enable the serve and client to start under the default run-level:

```sh
rc-update add snapserver default
rc-update add snapclient default
```

## macOS (Native)

*Warning:* macOS support is experimental

 1. Install Xcode from the App Store
 2. Install [Homebrew](http://brew.sh)
 3. Install the required libs

```ssh
brew install flac libsoxr libvorbis boost opus
```

### Build Snapclient

`cd` into the Snapclient src-root directory:

```sh
cd <snapcast dir>/client
make TARGET=MACOS
```

Install Snapclient

```sh
sudo make install TARGET=MACOS
```

This will copy the client binary to `/usr/local/bin` and create a Launch Agent to start the client as a daemon.

### Build Snapserver

`cd` into the Snapserver src-root directory:

```sh
cd <snapcast dir>/server
make TARGET=MACOS
```

Install Snapserver

```sh
sudo make install TARGET=MACOS
```

This will copy the server binary to `/usr/local/bin` and create a Launch Agent to start the server as a daemon.

## Android (Cross compile)

Clone [Snapdroid](https://github.com/badaix/snapdroid), which includes Snapclient as submodule:
```sh
git clone https://github.com/badaix/snapdroid.git
cd snapdroid
git submodule update --init --recursive
```
and execute `./gradlew build`, which will cross compile Snapclient and bundle it into the Snapdroid App.

## OpenWrt/LEDE (Cross compile)

To cross compile for OpenWrt, please follow the [OpenWrt flavored SnapOS guide](https://github.com/badaix/snapos/blob/master/openwrt/README.md)

## Buildroot (Cross compile)

To integrate Snapcast into [Buildroot](https://buildroot.org/), please follow the [Buildroot flavored SnapOS guide](https://github.com/badaix/snapos/blob/master/buildroot-external/README.md)

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
