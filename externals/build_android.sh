#!/bin/bash
NDK_PATH			= "/your/path/here"
HOST_PLATFORM_ANDROID		= "linux-x86_64"
HOST_PLATFORM_AUTOTOOLS		= "x86_64-pc-linux-gnu"
TARGET_PLATFORM			= "arch-arm"
ANDROID_VERSION			= "android-23"

export ANDROID_NDK=$NDK_PATH
export CROSS_COMPILE=arm-linux-androideabi
export ANDROID_PREFIX=${ANDROID_NDK}/toolchains/${CROSS_COMPILE}-4.9/prebuilt/$HOST_PLATFORM_ANDROID
export SYSROOT=${ANDROID_NDK}/platforms/$ANDROID_VERSION/$TARGET_PLATFORM
export CROSS_PATH=${ANDROID_PREFIX}/bin/${CROSS_COMPILE}

mkdir -p env/lib/pkgconfig

pushd env
export PREFIX=$(pwd)
pushd lib/pkgconfig
export PKG_CONFIG_PATH=$(pwd)
popd
popd

export CPP=${CROSS_PATH}-cpp
export AR=${CROSS_PATH}-ar
export AS=${CROSS_PATH}-as
export NM=${CROSS_PATH}-nm
export CC=${CROSS_PATH}-gcc
export CXX=${CROSS_PATH}-g++
export LD=${CROSS_PATH}-ld
export RANLIB=${CROSS_PATH}-ranlib

export CFLAGS="${CFLAGS} --sysroot=${SYSROOT} -I${SYSROOT}/usr/include -I${ANDROID_PREFIX}/include -static"
export CPPFLAGS="${CFLAGS}"
export CXXFLAGS="${CFLAGS} -I${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.9/include"
export LDFLAGS="${LDFLAGS} -L${SYSROOT}/usr/lib -L${ANDROID_PREFIX}/lib -static"

pushd flac
./autogen.sh
PATH=$PATH:${ANDROID_PREFIX}/bin ./configure --host=${CROSS_COMPILE} --build=$HOST_PLATFORM_AUTOTOOLS --with-sysroot=${SYSROOT} --prefix=${PREFIX}
PATH=$PATH:${ANDROID_PREFIX}/bin make
PATH=$PATH:${ANDROID_PREFIX}/bin make install
popd

pushd ogg
./autogen.sh
PATH=$PATH:${ANDROID_PREFIX}/bin ./configure --host=${CROSS_COMPILE} --build=$HOST_PLATFORM_AUTOTOOLS --with-sysroot=${SYSROOT} --prefix=${PREFIX}
PATH=$PATH:${ANDROID_PREFIX}/bin make
PATH=$PATH:${ANDROID_PREFIX}/bin make install
popd

pushd vorbis
./autogen.sh
PATH=$PATH:${ANDROID_PREFIX}/bin ./configure --host=${CROSS_COMPILE} --build=$HOST_PLATFORM_AUTOTOOLS --with-sysroot=${SYSROOT} --prefix=${PREFIX}
PATH=$PATH:${ANDROID_PREFIX}/bin make
PATH=$PATH:${ANDROID_PREFIX}/bin make install
popd
