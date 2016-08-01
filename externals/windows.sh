#!/bin/bash
mkdir -p target/lib/pkgconfig
pushd target
TARGET_DIR=`pwd`
pushd lib/pkgconfig
PKG_DIR=`pwd`
popd
popd

export PKG_CONFIG_PATH=$PKG_DIR

mkdir -p ogg_build
pushd ogg_build
cmake ../ogg -DBUILD_SHARED_LIBS=1 -DCMAKE_INSTALL_PREFIX=$TARGET_DIR -G "MSYS Makefiles"
cmake --build . --target install
popd

pushd flac
dos2unix configure.ac
./autogen.sh
./configure --prefix $TARGET_DIR
make install
popd

mkdir -p vorbis_build
pushd vorbis
cmake ../vorbis -DBUILD_SHARED_LIBS=1 -DCMAKE_INSTALL_PREFIX=$TARGET_DIR -G "MSYS Makefiles"
cmake --build . --target install
popd
