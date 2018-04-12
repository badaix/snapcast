#/bin/sh

export NDK_DIR_ARM="$1-arm"
export NDK_DIR_X86="$1-x86"
export ASSETS_DIR="$2"

./build_android.sh
