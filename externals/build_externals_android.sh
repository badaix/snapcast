export NDK_DIR="/home/johannes/Develop/android-toolchain-arm-14"
export CC=$NDK_DIR/bin/arm-linux-androideabi-gcc
export CXX=$NDK_DIR/bin/arm-linux-androideabi-g++
export CPPFLAGS="-U_ARM_ASSEM_ -I$NDK_DIR/include"

cd flac
./autogen.sh
./configure --host=arm --disable-ogg --prefix=$NDK_DIR
make
make install
make clean

cd ../ogg
./autogen.sh
./configure --host=arm --prefix=$NDK_DIR
make
make install
make clean

cd ../tremor
./autogen.sh
./configure --host=arm --prefix=$NDK_DIR --with-ogg=$NDK_DIR
make
make install
make clean

