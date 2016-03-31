export NDK_DIR="/home/johannes/Develop/android-toolchain-arm-14"
export ADD_CFLAGS=""
make clean; make TARGET=ANDROID -j 3; cp ./snapclient ../android/Snapcast/src/main/assets/bin/armeabi/
export ADD_CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
make clean; make TARGET=ANDROID -j 3; cp ./snapclient ../android/Snapcast/src/main/assets/bin/armeabi-v7a/
