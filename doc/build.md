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


## Linux (Native)
Install the build tools and required libs:  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install build-essential
    $ sudo apt-get install libasound2-dev libvorbisidec-dev libvorbis-dev libflac-dev alsa-utils libavahi-client-dev avahi-daemon

Compilation requires gcc 4.8 or higher, so it's highly recommended to use Debian (Raspbian) Jessie.

For Arch derivates:

    $ sudo pacman -S base-devel
    $ sudo pacman -S alsa-lib avahi libvorbis flac alsa-utils

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


## FreeBSD (Native)
Install the build tools and required libs:  

    $ sudo pkg install gmake gcc bash avahi libogg libvorbis flac

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

## macOS (Native)

*Warning: macOS support is experimental*

 1. Install Xcode from the App Store
 2. Install [Homebrew](http://brew.sh)
 3. Install the required libs

```    
$ brew install flac libvorbis
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
$ ./make_standalone_toolchain.py --arch x86 --api 16 --stl libc++ --install-dir <android-ndk dir>-x86
```

### Build Snapclient
Cross compile and install FLAC, ogg, and tremor (only needed once):

    $ cd <snapcast dir>/externals
    $ make NDK_DIR=<android-ndk dir>-arm ARCH=arm
    $ make NDK_DIR=<android-ndk dir>-x86 ARCH=x86
   
Compile the Snapclient:

    $ cd <snapcast dir>/client
    $ ./build_android_all.sh <android-ndk dir> <snapdroid assets dir>

The binaries for `armeabi` and `x86` will be copied into the Android's assets directory (`<snapdroid assets dir>/bin/`) and so will be bundled with the Snapcast App.


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

The `<snapcast dir>/buildroot` is currently setup as an external Buildroot folder following the [recommended structure](https://buildroot.org/downloads/manual/manual.html#customize-dir-structure). As of [Buildroot 2016.11](https://git.buildroot.net/buildroot/tag/?h=2016.11) you may specify multiple BR2_EXTERNAL trees. If you are using a version of Buildroot prior to this, then you will need to manually merge `<snapcast dir>/buildroot` with your existing Buildroot external tree.

Now configure buildroot with the [required packages](/buildroot/configs/snapcast_defconfig) (you can also manually add them to your project's existing defconfig):

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot snapcast_defconfig

Then use `menuconfig` to configure the rest of your project:

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot menuconfig

And finally run the build:

    $ cd <buildroot dir> && make BR2_EXTERNAL=<snapcast dir>/buildroot

## Raspberry Pi (Cross compile)
This example will show you how to add snapcast to [Buildroot](https://buildroot.org/) and compile for Raspberry Pi.

* https://github.com/nickaknudson/snapcast-pi
