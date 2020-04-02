# Build Snapcast
Clone the Snapcast repository. To do this, you need git.  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install git

For Arch derivates:

    $ sudo pacman -S git

For FreeBSD:

    $ sudo pkg install git

Clone Snapcast:

    $ git clone https://github.com/badaix/snapcast.git

this creates a directory `snapcast`, in the following referred to as `<snapcast dir>`.  
Next clone the external submodules:

    $ cd <snapcast dir>/externals
    $ git submodule update --init --recursive

Snapcast depends on boost 1.70 or higher. Since it depends on header only boost libs, boost does not need to be installed, but the boost include path must be set properly: download and extract the latest boost version and add the include path, e.g. calling `make` with prepended `ADD_CFLAGS`: `ADD_CFLAGS="-I/path/to/boost_1_7x_0/" make`.  
For `cmake` you must add the path to the `-DBOOST_ROOT` flag: `cmake -DBOOST_ROOT=/path/to/boost_1_7x_0`

## Linux (Native)
Install the build tools and required libs:  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install build-essential
    $ sudo apt-get install libasound2-dev libvorbisidec-dev libvorbis-dev libopus-dev libflac-dev libsoxr-dev alsa-utils libavahi-client-dev avahi-daemon expat

Compilation requires gcc 4.8 or higher, so it's highly recommended to use Debian (Raspbian) Jessie.

For Arch derivates:

    $ sudo pacman -S base-devel
    $ sudo pacman -S alsa-lib avahi libvorbis opus-dev flac libsoxr alsa-utils boost expat

For Fedora (and probably RHEL, CentOS, & Scientific Linux, but untested):

    $ sudo dnf install @development-tools
    $ sudo dnf install alsa-lib-devel avahi-devel libvorbis-devel opus-devel flac-devel soxr-devel libstdc++-static expat

### Build Snapclient and Snapserver
`cd` into the Snapcast src-root directory:

    $ cd <snapcast dir>
    $ make

Install Snapclient and/or Snapserver:

    $ sudo make installserver
    $ sudo make installclient

This will copy the client and/or server binary to `/usr/bin` and update init.d/systemd to start the client/server as a daemon.

### Build Snapclient
`cd` into the Snapclient src-root directory:

    $ cd <snapcast dir>/client
    $ make

Install Snapclient

    $ sudo make install

This will copy the client binary to `/usr/bin` and update init.d/systemd to start the client as a daemon.

### Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ make

Install Snapserver

    $ sudo make install

This will copy the server binary to `/usr/bin` and update init.d/systemd to start the server as a daemon.

### Debian packages

Debian packages can be made with

    $ sudo apt-get install debhelper
    $ cd <snapcast dir>
    $ fakeroot make -f debian/rules binary

If you don't have boost installed or in your standard include paths, you can call
    
    $ fakeroot make -f debian/rules CPPFLAGS="-I/path/to/boost_1_7x_0" binary

## FreeBSD (Native)
Install the build tools and required libs:  

    $ sudo pkg install gmake gcc bash avahi libogg libvorbis libopus flac libsoxr

### Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ gmake TARGET=FREEBSD

Install Snapserver

    $ sudo gmake TARGET=FREEBSD install

This will copy the server binary to `/usr/local/bin` and the startup script to `/usr/local/etc/rc.d/snapserver`. To enable the Snapserver, add this line to `/etc/rc.conf`:  

    snapserver_enable="YES"

For additional command line arguments, add in `/etc/rc.conf`:

    snapserver_opts="<your custom options>"

Start and stop the server with `sudo service snapserver start` and `sudo service snapserver stop`.


## Gentoo (native)

Snapcast is available under Gentoo's [Portage](https://wiki.gentoo.org/wiki/Portage) package management system.  Portage utilises `USE` flags to determine what components are built on compilation.  The availabe options are...

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


These can be set either in the [global configuration](https://wiki.gentoo.org/wiki//etc/portage/make.conf#USE) file `/etc/portage/make.conf` or on a per-package basis (as root):

    if [ ! -d "$DIRECTORY" ]; then
      mkdir /etc/portage/package.use/media-sound
    fi
    echo 'media-sound/snapcast client server flac

If for example you only wish to build the server and *not* the client then precede the server `USE` flag with `-` i.e.

    echo 'media-sound/snapcast client -server

Once `USE` flags are configured emerge snapcast as root:

    $ emerge -av snapcast


Starting the client or server depends on whether you are using `systemd` or `openrc`.  To start using `openrc`:

    /etc/init.d/snapclient start
    /etc/init.d/snapserver start

To enable the serve and client to start under the default run-level:

    rc-update add snapserver default
    rc-update add snapclient default


## macOS (Native)

*Warning: macOS support is experimental*

 1. Install Xcode from the App Store
 2. Install [Homebrew](http://brew.sh)
 3. Install the required libs

```   
$ brew install flac libsoxr libvorbis boost opus
```

### Build Snapclient
`cd` into the Snapclient src-root directory:

    $ cd <snapcast dir>/client
    $ make TARGET=MACOS

Install Snapclient

    $ sudo make install TARGET=MACOS

This will copy the client binary to `/usr/local/bin` and create a Launch Agent to start the client as a daemon.

### Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ make TARGET=MACOS

Install Snapserver

    $ sudo make install TARGET=MACOS

This will copy the server binary to `/usr/local/bin` and create a Launch Agent to start the server as a daemon.

## Android (Cross compile)
Cross compilation for Android is done with the [Android NDK](http://developer.android.com/tools/sdk/ndk/index.html) on a Linux host machine. 

### Android NDK setup
http://developer.android.com/ndk/guides/standalone_toolchain.html
 1. Download NDK: `https://dl.google.com/android/repository/android-ndk-r17b-linux-x86_64.zip`
 2. Extract to: `/SOME/LOCAL/PATH/android-ndk-r17b`
 3. Setup toolchains for arm and x86 somewhere in your home dir (`<android-ndk dir>`):

```
$ cd /SOME/LOCAL/PATH/android-ndk-r17/build/tools
$ ./make_standalone_toolchain.py --arch arm --api 16 --stl libc++ --install-dir <android-ndk dir>-arm
$ ./make_standalone_toolchain.py --arch arm64 --api 21 --stl libc++ --install-dir <android-ndk dir>-arm64
$ ./make_standalone_toolchain.py --arch x86 --api 16 --stl libc++ --install-dir <android-ndk dir>-x86
```

### Build Snapclient
Cross compile and install FLAC, opus, ogg, and tremor (only needed once):

    $ cd <snapcast dir>/externals
    $ make NDK_DIR=<android-ndk dir>-arm ARCH=arm
    $ make NDK_DIR=<android-ndk dir>-arm64 ARCH=aarch64
    $ make NDK_DIR=<android-ndk dir>-x86 ARCH=x86
  
Compile the Snapclient:

    $ cd <snapcast dir>/client
    $ ./build_android_all.sh <android-ndk dir> <snapdroid jniLibs dir>

The binaries for `armeabi`, `arm64-v8a` and `x86` will be copied into the Android's jniLibs directory (`<snapdroid jniLibs dir>/`) and so will be bundled with the Snapcast App.


## OpenWrt/LEDE (Cross compile)
Cross compilation for OpenWrt is done with the [OpenWrt build system](https://wiki.openwrt.org/about/toolchain) on a Linux host machine:  
https://wiki.openwrt.org/doc/howto/build

For LEDE:
https://lede-project.org/docs/guide-developer/quickstart-build-images

### OpenWrt/LEDE build system setup
https://wiki.openwrt.org/doc/howto/buildroot.exigence

Clone OpenWrt to some place in your home directory (`<buildroot dir>`)

    $ git clone git://git.openwrt.org/15.05/openwrt.git
  
...LEDE

    $ git clone https://git.lede-project.org/source.git

Download and install available feeds

    $ cd <buildroot dir>
    $ ./scripts/feeds update -a
    $ ./scripts/feeds install -a

Within the `<buildroot dir>` directory create symbolic links to the Snapcast source directory `<snapcast source>` and to the OpenWrt Makefile:

    $ mkdir -p <buildroot dir>/package/sxx/snapcast
    $ cd <buildroot dir>/package/sxx/snapcast
    $ ln -s <snapcast source> src
    $ ln -s <snapcast source>/openWrt/Makefile.openwrt Makefile

Build  
in menuconfig in `sxx/snapcast` select `Compile snapserver` and/or `Compile snapclient`

    $ cd <buildroot dir>
    $ make defconfig
    $ make menuconfig
    $ make

Rebuild Snapcast:

    $ make package/sxx/snapcast/clean
    $ make package/sxx/snapcast/compile

The packaged `ipk` files are for OpenWrt in `<buildroot dir>/bin/ar71xx/packages/base/snap[client|server]_x.x.x_ar71xx.ipk` and for LEDE `<buildroot dir>/bin/packages/mips_24kc/base/snap[client|server]_x.x.x_mips_24kc.ipk`

## Buildroot (Cross compile)
This example will show you how to add snapcast to [Buildroot](https://buildroot.org/).

### Buildroot setup
Buildroot recommends [keeping customizations outside of the main Buildroot directory](https://buildroot.org/downloads/manual/manual.html#outside-br-custom) which is what this example will walk through.

Clone Buildroot to some place in your home directory (`<buildroot dir>`):

    $ BUILDROOT_VERSION=2016.11.2
    $ git clone --branch $BUILDROOT_VERSION --depth=1 git://git.buildroot.net/buildroot

The `<snapcast dir>/buildroot` is currently set up as an external Buildroot folder following the [recommended structure](https://buildroot.org/downloads/manual/manual.html#customize-dir-structure). As of [Buildroot 2016.11](https://git.buildroot.net/buildroot/tag/?h=2016.11) you may specify multiple BR2_EXTERNAL trees. If you are using a version of Buildroot prior to this, then you will need to manually merge `<snapcast dir>/buildroot` with your existing Buildroot external tree.

Now configure buildroot with the [required packages](/buildroot/configs/snapcast_defconfig) (you can also manually add them to your project's existing defconfig):

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot snapcast_defconfig

Then use `menuconfig` to configure the rest of your project:

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot menuconfig

And finally run the build:

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot

## Raspberry Pi (Cross compile)
This example will show you how to add snapcast to [Buildroot](https://buildroot.org/) and compile for Raspberry Pi.

* https://github.com/nickaknudson/snapcast-pi

## Windows (vcpkg)

Prerequisites:

 * CMake 
 * Visual Studio 2017 or 2019 with C++

Set up [vcpkg](https://github.com/Microsoft/vcpkg)

Install dependencies

    $ vcpkg.exe install libflac libvorbis soxr opus boost-asio --triplet x64-windows

Build

    $ cd <snapcast dir>
    $ mkdir build
    $ cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg_dir>/scripts/buildsystems/vcpkg.cmake
    $ cmake --build . --config Release