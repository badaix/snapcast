#Build Snapcast
Clone the Snapcast repository. To do this, you need git.  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install git

For Arch derivates:

    $ pacman -S git

For FreeBSD:

    $ sudo pkg install git

Clone Snapcast:

    $ git clone --recursive https://github.com/badaix/snapcast.git

##Linux (Native)
Install the build tools and required libs:  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install build-essential cmake
    $ sudo apt-get install libasound2-dev libvorbisidec-dev libvorbis-dev libflac-dev alsa-utils libavahi-client-dev avahi-daemon

Compilation requires gcc 4.8 or higher, so it's highly recommended to use Debian (Raspbian) Jessie.

For Arch derivates:

    $ pacman -S base-devel cmake
    $ pacman -S alsa-lib avahi libvorbis flac alsa-utils

###Build Snapclient and Snapserver
`cd` into the Snapcast src-root directory:

    $ cd <snapcast dir>
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make

Install Snapclient and Snapserver:

    $ sudo make install

This will copy the client and/or server binary to `/usr/sbin` and update init.d/systemd to start the client/server as a daemon.

###Packaging Snapcast
In the build directory:

    $ make package

A debian-based platform is required to build a function `.deb` package.  All platforms will build a tar binary archive.

##FreeBSD (Native)
Install the build tools and required libs:  

    $ sudo pkg install gmake gcc bash avahi libogg libvorbis flac

###Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ gmake TARGET=FREEBSD

Install Snapserver

    $ sudo gmake TARGET=FREEBSD install

This will copy the server binary to `/usr/local/sbin` and the startup script to `/usr/local/etc/rc.d/snapserver`. To enable the Snapserver, add this line to `/etc/rc.conf`: 

    snapserver_enable="YES"

For additional command line arguments, add in `/etc/rc.conf`:

    snapserver_opts="<your custom options>"

Start and stop the server with `sudo service snapserver start` and `sudo service snapserver stop`.

##macOS (Native)

*Warning: macOS support is experimental*

 1. Install Xcode from the App Store
 2. Install [Homebrew](http://brew.sh)
 3. Install the required libs

```    
$ brew install flac libvorbis
```

###Build Snapclient
`cd` into the Snapclient src-root directory:

    $ cd <snapcast dir>/client
    $ make TARGET=MACOS

Install Snapclient

    $ sudo make install

This will copy the client binary to `/usr/local/bin` and create a Launch Agent to start the client as a daemon.

###Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ make TARGET=MACOS

Install Snapserver

    $ sudo make install

This will copy the server binary to `/usr/local/bin` and create a Launch Agent to start the server as a daemon.

##Android (Cross compile)
Cross compilation for Android is done with the [Android NDK](http://developer.android.com/tools/sdk/ndk/index.html) on a Linux host machine.  

###Android NDK setup
http://developer.android.com/ndk/guides/standalone_toolchain.html
 1. Download NDK: `https://dl.google.com/android/repository/android-ndk-r13-linux-x86_64.zip`
 2. Extract to: `/SOME/LOCAL/PATH/android-ndk-r13`
 3. Setup toolchain somewhere in your home dir (`<android-ndk dir>`):

````
$ cd /SOME/LOCAL/PATH/android-ndk-r13/build/tools
$ ./make_standalone_toolchain.py --arch arm --api 14 --install-dir <android-ndk dir>
````

###Build Snapclient
Edit the first lines in `<snapcast dir>/client/build_android.sh` and in `<snapcast dir>/externals/build_externals_android.sh` to let `NDK_DIR` point to your `<android-ndk dir>`  
Cross compile and install FLAC, ogg, and tremor (only needed once):

    $ cd <snapcast dir>/externals
    $ ./build_externals_android.sh
   
Compile the Snapclient:

    $ cd <snapcast dir>/client
    $ ./build_android.sh

The binaries for `armeabi` and `armeabi-v7a` will be copied into the Android's assets directory (`<snapcast dir>/android/Snapcast/src/main/assets/bin/`) and will be part of the Snapcast App.


##OpenWrt (Cross compile)
Cross compilation for OpenWrt is done with the [OpenWrt build system](https://wiki.openwrt.org/about/toolchain) on a Linux host machine.  
https://wiki.openwrt.org/doc/howto/build

###OpenWrt build system setup
https://wiki.openwrt.org/doc/howto/buildroot.exigence

Clone OpenWrt to some place in your home directory (`<buildroot dir>`)

    $ git clone git://git.openwrt.org/15.05/openwrt.git

Download and install available feeds

    $ cd <buildroot dir>
    $ ./scripts/feeds update -a
    $ ./scripts/feeds install -a

Build

    $ make menuconfig
    $ make

Within the OpenWrt directory create symbolic links to the Snapcast source directory and to the OpenWrt Makefile:

    $ cd <buildroot dir>/package/sxx/snapcast
    $ ln -s <snapcast dir>/openWrt/Makefile.openwrt Makefile
    $ ln -s <snapcast dir> src

Build Snapcast:

    $ cd <buildroot dir>
    $ make package/sxx/snapcast/clean
    $ make package/sxx/snapcast/compile

The packaged `ipk` files are in `<buildroot dir>/bin/ar71xx/packages/base/snap[client|server]_x.x.x_ar71xx.ipk`

##Windows (Native)
The instructions below are for Visual Studio 2015 community, which is free for private use.  The project can be compiled using other toolsets such as MinGW however that is not documented here.

Builing FLAC requires nasm.exe to be on the system PATH.  Grab it from [nasm.us](http://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D).

###Compiling libogg
Open `externals/ogg/win32/VS2015/libogg_dynamic.sln`

When prompted, upgrade the toolset

Set the configuration to release

Set the platform to x64 or Win32 depending on which you are building for


Open `externals/ogg/win32/VS2015/libogg_static.sln`

Set the configuration to release

Set the platform to x64 or Win32 depending on which you are building for

Copy all files from `externals/ogg/win32/VS2015/(x64|Win32)/Release` to `externals/flac/objs/(x64|Win32)/Release/lib` (you may need to create part of this tree)

Copy `ogg` folder from `externals/ogg/include/ogg` to `externals/flac/include`

###Compiling libFLAC
Open `externals/flac/FLAC.sln`

When prompted, upgrade the toolset

Set the configuration to release

Set the platform to x64 or Win32 depending on which you are building for

###Compiling libvorbis
Open `externals/vorbis/win32/VS2010/vorbis_dynamic.sln`

When prompted, upgrade the toolset

Set the configuration to release

Set the platform to x64 or Win32 depending on which you are building for

Right click the project `libvorbis` and select `Properties`

Under `Linker\General`, modify `Additional Library Directories` to read `..\..\..\..\ogg\win32\VS2015\$(Platform)\$(Configuration);%(AdditionalLibraryDirectories)`

Close the property sheet

Right click `libvorbis` and select `Build`

###Compiling Snapcast
Launch CMake GUI and generate a Visual Studio solution

Open `snapcast.sln`.  Select release configuration.  Build the solution

###Packaging
Windows packaging comes in two flavours: bundled and standalone.  The standalone installer produces an MSI that only installs snapclient.  Bundled produces a bootstrapped EXE that installs all prerequisites before snapclient.  Producing a bundled package requires the Bonjour SDK and a signed redistribution agreement.  If a bundled installer is required, ensure the Bonjour SDK is installed before proceeding with the bundle steps

####Standalone MSI
Build the `PACKAGE` project.  The MSI will be created in the build directory.

####Bundled EXE
Build the `BUNDLE` project.  The EXE will be created in the build directory.
