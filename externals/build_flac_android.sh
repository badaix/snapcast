export NDK_DIR="/home/johannes/Develop/android-toolchain-arm-14"
export CC=$NDK_DIR/bin/arm-linux-androideabi-gcc
export CXX=$NDK_DIR/bin/arm-linux-androideabi-g++
#export INC=$(SYSROOT)/usr/include
cd flac
./autogen.sh
./configure --host=arm --disable-ogg --prefix=/home/johannes/Develop/build/arm
make
make install
