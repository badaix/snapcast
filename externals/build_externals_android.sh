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
rm -rf .deps/
rm Makefile
rm Makefile.in
rm Version_script
rm aclocal.m4
rm -rf autom4te.cache/
rm compile
rm config.guess
rm config.h
rm config.h.in
rm config.log
rm config.status
rm config.sub
rm configure
rm depcomp
rm install-sh
rm libtool
rm ltmain.sh
rm missing
rm stamp-h1
rm vorbisidec.pc

cd ..
