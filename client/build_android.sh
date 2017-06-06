export NDK_DIR="/home/johannes/Develop/android-toolchain-arm-14"
export ADD_CFLAGS="-march=armv7"
make clean; make TARGET=ANDROID -j 4; cp ./snapclient ../android/Snapcast/src/main/assets/bin/armeabi/
export ADD_CFLAGS="-march=armv7-a"
make clean; make TARGET=ANDROID -j 4; cp ./snapclient ../android/Snapcast/src/main/assets/bin/armeabi-v7a/
