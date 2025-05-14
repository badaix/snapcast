#/bin/sh

export NDK_DIR_ARM="$1-arm"
export NDK_DIR_ARM64="$1-arm64"
export NDK_DIR_X86="$1-x86"
export JNI_LIBS_DIR="$2"

./build_android.sh
