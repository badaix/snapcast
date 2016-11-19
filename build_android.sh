#!/bin/bash
build_arch()
{
  mkdir -p build_$1
  pushd build_$1
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_NDK=~/Android/ndk-bundle -DANDROID_NATIVE_API_LEVEL="android-23" -DANDROID_ABI=$1 \
      -DFLAC_INCLUDE_DIRS="$(pwd)/../externals/env/include"   -DFLAC_LIBRARIES="$(pwd)/../externals/env/lib/libFLAC.a"   \
    -DVORBIS_INCLUDE_DIRS="$(pwd)/../externals/env/include" -DVORBIS_LIBRARIES="$(pwd)/../externals/env/lib/libvorbis.a" \
       -DOGG_INCLUDE_DIRS="$(pwd)/../externals/env/include"    -DOGG_LIBRARIES="$(pwd)/../externals/env/lib/libogg.a"    \
    -Wno-deprecated
  make
  rm ../android/Snapcast/src/main/assets/bin/$1/libcommon.a ../android/Snapcast/src/main/assets/bin/$1/libmessage.a
  popd
}

build_arch armeabi
build_arch armeabi-v7a

pushd android
./gradlew build
cp Snapcast/build/outputs/apk/* ../bin
popd
