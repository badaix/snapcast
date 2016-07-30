#!/bin/bash
mkdir -p ogg_build
pushd ogg_build
cmake ../ogg -DBUILD_SHARED_LIBS=1 -DCMAKE_INSTALL_PREFIX=../target -G "MSYS Makefiles"
cmake --build . --target install
popd

pushd flac
./autogen.sh
./configure --prefix ../target
make install
popd

mkdir -p vorbis_build
pushd vorbis
cmake ../vorbis -DCMAKE_INSTALL_PREFIX=../target -G "MSYS Makefiles"
cmake --build . --target install
popd
