#/bin/sh

export NDK_DIR="$1"
export JNI_LIBS_DIR="$2"
export TOOLCHAIN="$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64"

export ARCH=x86
make clean; 
make TARGET=ANDROID -j 4; $TOOLCHAIN/x86_64-linux-android/bin/strip ./snapclient; cp ./snapclient "$JNI_LIBS_DIR/x86_64/libsnapclient.so"; mv ./snapclient "$JNI_LIBS_DIR/x86/libsnapclient.so"

export ARCH=arm
make clean; 
make TARGET=ANDROID -j 4; $TOOLCHAIN/arm-linux-androideabi/bin/strip ./snapclient; mv ./snapclient "$JNI_LIBS_DIR/armeabi-v7a/libsnapclient.so"

export ARCH=aarch64
make clean; 
make TARGET=ANDROID -j 4; $TOOLCHAIN/aarch64-linux-android/bin/strip ./snapclient; mv ./snapclient "$JNI_LIBS_DIR/arm64-v8a/libsnapclient.so"
