#/bin/sh

if [ -z "$NDK_DIR_ARM" ] && [ -z "$NDK_DIR_MIPS" ] && [ -z "$NDK_DIR_X86" ]; then
	echo "Specify at least one NDK_DIR_[ARM|MIPS|X86]"
	exit
fi

if [ -n "$NDK_DIR_ARM" ]; then
	export NDK_DIR="$NDK_DIR_ARM"
	export ARCH=arm
	make clean; make TARGET=ANDROID -j 4; mv ./snapclient ../android/Snapcast/src/main/assets/bin/armeabi/
fi

if [ -n "$NDK_DIR_MIPS" ]; then
	export NDK_DIR="$NDK_DIR_MIPS"
	export ARCH=mips
	make clean; make TARGET=ANDROID -j 4; mv ./snapclient ../android/Snapcast/src/main/assets/bin/mips/
fi

if [ -n "$NDK_DIR_X86" ]; then
	export NDK_DIR="$NDK_DIR_X86"
	export ARCH=x86
	make clean; make TARGET=ANDROID -j 4; mv ./snapclient ../android/Snapcast/src/main/assets/bin/x86/
fi
