export CC=/home/johannes/Develop/android-ndk-r10e-arm-21/bin/arm-linux-androideabi-gcc
export CXX=/home/johannes/Develop/android-ndk-r10e-arm-21/bin/arm-linux-androideabi-g++
#export INC=$(SYSROOT)/usr/include
cd flac
./configure --host=arm --disable-ogg --prefix=/home/johannes/Develop/build/arm
