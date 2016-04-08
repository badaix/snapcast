#Build Snapcast
Clone the Snapcast repository. To do this, you need git.  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install git

For Arch derivates:

    $ pacman -S git

Clone Snapcast:

    $ git clone https://github.com/badaix/snapcast.git

this creates a directory `snapcast`, in the following referred to as `<snapcast dir>`.  
Next clone the external submodules:

    $ cd <snapcast dir>/externals
    $ git submodule update --init --recursive


##Linux (Native)
Install the build tools and required libs:  
For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install build-essential
    $ sudo apt-get install libasound2-dev libvorbisidec-dev libvorbis-dev libflac-dev alsa-utils libavahi-client-dev avahi-daemon

Compilation requires gcc 4.8 or higher, so it's highly recommended to use Debian (Respbian) Jessie.

For Arch derivates:

    $ pacman -S base-devel
    $ pacman -S alsa-lib avahi libvorbis flac alsa-utils

###Build Snapclient and Snapserver
`cd` into the Snapcast src-root directory:

    $ cd <snapcast dir>
    $ make

Install Snapclient and/or Snapserver:

    $ sudo make installserver
    $ sudo make installclient

This will copy the client and/or server binary to `/usr/sbin` and update init.d/systemd to start the client/server as a daemon.

###Build Snapclient
`cd` into the Snapclient src-root directory:

    $ cd <snapcast dir>/client
    $ make

Install Snapclient

    $ sudo make install

This will copy the client binary to `/usr/sbin` and update init.d/systemd to start the client as a daemon.

###Build Snapserver
`cd` into the Snapserver src-root directory:

    $ cd <snapcast dir>/server
    $ make

Install Snapserver

    $ sudo make install

This will copy the server binary to `/usr/sbin` and update init.d/systemd to start the server as a daemon.

##Android (Cross compile)
http://developer.android.com/tools/sdk/ndk/index.html  
TODO: Host Linux, compile flac, use build.sh
###Android NDK setup
http://developer.android.com/ndk/guides/standalone_toolchain.html
 1. Download NDK: `http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin`
 2. Extract to: `/SOME/LOCAL/PATH/android-ndk-r10e`
 3. Setup toolchain in somewhere in your home dir (`<android-ndk dir>`):

```
$ cd /SOME/LOCAL/PATH/android-ndk-r10e/build/tools
$ ./make-standalone-toolchain.sh --arch=arm --platform=android-14 --install-dir=<android-ndk dir> --ndk-dir=/SOME/LOCAL/PATH/android-ndk-r10e --system=linux-x86_64
```

###Build Snapclient
`cd` into the Snapclient src-root directory:

    $ cd <snapcast dir>/client
    $ make TARGET=ANDROID`

##OpenWrt (Cross compile)
https://wiki.openwrt.org/doc/howto/build
###OpenWrt build system setup
https://wiki.openwrt.org/doc/howto/buildroot.exigence

1. Do everything as non-root user
2. Issue all commands in the <buildroot dir> directory, e.g. ~/openwrt

```
git clone git://git.openwrt.org/15.05/openwrt.git
```


```
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```


```
make menuconfig
make
```


Within the OpenWrt directory I'm linking to it, like this:
```
cd <buildroot dir>/package/sxx/snapcast
ln -s <snapcast dir>/openWrt/Makefile.openwrt Makefile
ln -s <snapcast dir> src

ls -l
Makefile -> <snapcast dir>/openWrt/Makefile.openwrt
src -> <snapcast dir>
```
...and build like this:
```
cd <buildroot dir>
make package/sxx/snapcast/clean V=s
make package/sxx/snapcast/compile -j1 V=s
```
