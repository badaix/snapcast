#!/bin/bash
pushd `dirname $0`

alias docbook-to-man=docbook2man # flac you that's why

if [ ! -f android-ndk-r13b-linux-x86_64.zip ]; then
		wget https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
fi
if [ ! -d android-ndk-r13b ]; then
		unzip android-ndk-r13b-linux-x86_64.zip
fi

NDK=$(pwd)/android-ndk-r13b
ANDROID_PRJ=$(pwd)/android-app

build_for_arch ()
{
		ARCH=$1
		ABI=$2
		OPTIONS=$3
		
		mkdir -p build_$ARCH
		pushd build_$ARCH
		ANDROID_TOOLCHAIN=$(pwd)/toolchain
		$NDK/build/tools/make_standalone_toolchain.py --arch $ARCH --api 21 --install-dir $ANDROID_TOOLCHAIN
		cmake -DCMAKE_SYSTEM_NAME=Android -DCMAKE_ANDROID_STANDALONE_TOOLCHAIN=$ANDROID_TOOLCHAIN -DCMAKE_ANDROID_STL_TYPE="gnustl_shared" -DCMAKE_SHARED_LINKER_FLAGS="-L$ANDROID_TOOLCHAIN/sysroot/usr/lib -lm -lgnustl_shared" -DCMAKE_ANDROID_ARCH_ABI=$ABI -DAUTOTOOLS_OPTS=$OPTIONS ..
		make
		popd
}

build_for_arch arm armeabi-v7a
build_for_arch arm64 arm64-v8a
build_for_arch mips mips
build_for_arch mips64 mips64
build_for_arch x86 x86 "--disable-asm-optimizations --disable-sse"
build_for_arch x86_64 x86_64 "--disable-asm-optimizations --disable-sse"

pushd android-app
./gradlew build
popd

popd
