#/bin/sh

if [ -z "$NDK_DIR_ARM" ] && [ -z "$NDK_DIR_ARM64" ] && [ -z "$NDK_DIR_X86" ]; then
	echo "Specify at least one NDK_DIR_[ARM|ARM64|X86]"
	exit
fi

if [ -z "$JNI_LIBS_DIR" ]; then
	echo "Specify the snapdroid jniLibs dir JNI_LIBS_DIR"
	exit
fi

if [ -n "$NDK_DIR_ARM" ]; then
	export NDK_DIR="$NDK_DIR_ARM"
	export ARCH=arm
	make clean; make TARGET=ANDROID -j 4; $NDK_DIR/bin/arm-linux-androideabi-strip ./snapclient; mv ./snapclient "$JNI_LIBS_DIR/armeabi/libsnapclient.so"
fi

if [ -n "$NDK_DIR_ARM64" ]; then
	export NDK_DIR="$NDK_DIR_ARM64"
	export ARCH=arm64
	make clean; make TARGET=ANDROID -j 4; $NDK_DIR/bin/aarch64-linux-android-strip ./snapclient; mv ./snapclient "$JNI_LIBS_DIR/arm64-v8a/libsnapclient.so"
fi

if [ -n "$NDK_DIR_X86" ]; then
	export NDK_DIR="$NDK_DIR_X86"
	export ARCH=x86
	make clean; make TARGET=ANDROID -j 4; $NDK_DIR/bin/i686-linux-android-strip ./snapclient; mv ./snapclient "$JNI_LIBS_DIR/x86/libsnapclient.so"
fi
