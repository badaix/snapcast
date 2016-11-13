#!/bin/bash
mkdir -p build
pushd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DANDROID_NDK=~/Android/ndk-bundle/ -DANDROID_NATIVE_API_LEVEL="android-24"\
    -DFLAC_INCLUDE_DIRS="$(pwd)/../externals/env/include"   -DFLAC_LIBRARIES="$(pwd)/../externals/env/lib/libFLAC.a"   \
  -DVORBIS_INCLUDE_DIRS="$(pwd)/../externals/env/include" -DVORBIS_LIBRARIES="$(pwd)/../externals/env/lib/libvorbis.a" \
     -DOGG_INCLUDE_DIRS="$(pwd)/../externals/env/include"    -DOGG_LIBRARIES="$(pwd)/../externals/env/lib/libogg.a"    \
  -Wno-deprecated
make
popd
