#/bin/sh

if [ -z "$NDK_DIR_ARM" ] && [ -z "$NDK_DIR_X86" ]; then
	echo "Specify at least one NDK_DIR_[ARM|X86]"
	exit
fi

if [ -z "$ASSETS_DIR" ]; then
	echo "Specify the snapdroid assets root dir ASSETS_DIR"
	exit
fi

if [ -n "$NDK_DIR_ARM" ]; then
	export NDK_DIR="$NDK_DIR_ARM"
	export ARCH=arm
	make clean; make TARGET=ANDROID -j 4; mv ./snapclient "$ASSETS_DIR/bin/armeabi/"
fi

if [ -n "$NDK_DIR_X86" ]; then
	export NDK_DIR="$NDK_DIR_X86"
	export ARCH=x86
	make clean; make TARGET=ANDROID -j 4; mv ./snapclient "$ASSETS_DIR/bin/x86/"
fi
