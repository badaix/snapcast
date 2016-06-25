#!/bin/bash
# Create target env
mkdir -p target
mkdir -p target/include
mkdir -p target/lib
pushd target
TARGET_DIR=`pwd`
pushd include
TARGET_INCLUDE_DIR=`pwd`
popd
pushd lib
TARGET_LIB_DIR=`pwd`
popd
popd

# Configure ogg
pushd ogg
./autogen.sh
CPPFLAGS="-I$TARGET_INCLUDE_DIR -L$TARGET_LIB_DIR" ./configure --host=x86_64-w64-mingw32 --prefix=$TARGET_DIR
popd

# Build ogg
pushd ogg
make
make install
popd

# Configure flac/vorbis
pushd flac
./autogen.sh
CPPFLAGS="-I$TARGET_INCLUDE_DIR -L$TARGET_LIB_DIR" ./configure --host=x86_64-w64-mingw32 --prefix=$TARGET_DIR
popd
pushd vorbis
./autogen.sh
CPPFLAGS="-I$TARGET_INCLUDE_DIR -L$TARGET_LIB_DIR" ./configure --host=x86_64-w64-mingw32 --prefix=$TARGET_DIR
popd

# Build FLAC
pushd flac
make
make install
popd

# Build vorbis
pushd vorbis
make
make install
popd
