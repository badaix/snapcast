#!/bin/sh
LD_LIBRARY_PATH=../externals/target:$LD_LIBRARY_PATH LD_RUN_PATH=../externals/target:$LD_LIBRARY_PATH PKG_CONFIG_LIBDIR=../externals/target/usr/lib/pkgconfig CMAKE_PREFIX_PATH=../externals/target cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/Toolchain.cmake -DCMAKE_BUILD_TYPE:STRING=Debug ..
