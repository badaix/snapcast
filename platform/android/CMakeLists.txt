cmake_minimum_required(VERSION 3.7)
project(snapcast_android)

string(FIND ${CMAKE_ANDROID_ARCH_ABI} "-" ABI_CUTOFF)
string(SUBSTRING ${CMAKE_ANDROID_ARCH_ABI} 0 ${ABI_CUTOFF} ABI_PART)
set(ANDROID_HOST "${ABI_PART}-linux-android")
string(REPLACE "eabi" "" ANDROID_HOST ${ANDROID_HOST})
string(REPLACE "arm64" "aarch64" ANDROID_HOST ${ANDROID_HOST})

include(ExternalProject)
ExternalProject_Add(ogg_1.3.2
  GIT_REPOSITORY "https://git.xiph.org/ogg.git"
	GIT_TAG "36606a06c60fabb1d87ec204ae4f14dfe1224362"
	CONFIGURE_COMMAND "${CMAKE_BINARY_DIR}/ogg_1.3.2-prefix/src/ogg_1.3.2/autogen.sh" "--host=${ANDROID_HOST}" "--with-sysroot=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot" "--prefix=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot/usr" "${AUTOTOOLS_OPTS}" "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CXXFLAGS=${CMAKE_CXX_FLAGS}" "CFLAGS=${CMAKE_C_FLAGS}" "LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
	BUILD_COMMAND make
	BUILD_IN_SOURCE 1
)
ExternalProject_Add(FLAC_1.3.2
  GIT_REPOSITORY "https://git.xiph.org/flac.git"
	GIT_TAG "ac39d3719f16dfb6e08d2fbde8ccaf34a266c81d"
	UPDATE_COMMAND "${CMAKE_BINARY_DIR}/FLAC_1.3.2-prefix/src/FLAC_1.3.2/autogen.sh"
	CONFIGURE_COMMAND "${CMAKE_BINARY_DIR}/FLAC_1.3.2-prefix/src/FLAC_1.3.2/configure" "--host=${ANDROID_HOST}" "--with-sysroot=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot" "--prefix=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot/usr" "${AUTOTOOLS_OPTS}" "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CXXFLAGS=${CMAKE_CXX_FLAGS}" "CFLAGS=${CMAKE_C_FLAGS}" "LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
	BUILD_COMMAND make
	BUILD_IN_SOURCE 1
)
ExternalProject_Add(vorbis_1.3.2
  GIT_REPOSITORY "https://git.xiph.org/vorbis.git"
	GIT_TAG "f4093202cb226b360d0edf8a948aa71142cec657"
	PATCH_COMMAND "sed" -i -e "s/-mno-ieee-fp//g" "${CMAKE_BINARY_DIR}/vorbis_1.3.2-prefix/src/vorbis_1.3.2/configure.ac"
	CONFIGURE_COMMAND "${CMAKE_BINARY_DIR}/vorbis_1.3.2-prefix/src/vorbis_1.3.2/autogen.sh" "--host=${ANDROID_HOST}" "--with-sysroot=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot" "--prefix=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot/usr" "${AUTOTOOLS_OPTS}" "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CXXFLAGS=${CMAKE_CXX_FLAGS} -Qunused-arguments" "CFLAGS=${CMAKE_C_FLAGS} -Qunused-arguments" "LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
	BUILD_COMMAND make
	BUILD_IN_SOURCE 1
)
ExternalProject_Add(snapcast
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/../..
	CMAKE_COMMAND "cmake"
	CMAKE_ARGS "-DCMAKE_SYSTEM_NAME=Android" "-DCMAKE_ANDROID_STANDALONE_TOOLCHAIN=${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}" "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}" '-DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -I${CMAKE_ANDROID_STANDALONE_TOOLCHAIN}/sysroot/usr/include"' "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}" "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
	BUILD_COMMAND make "VERBOSE=1"
	INSTALL_COMMAND echo
)
add_dependencies(FLAC_1.3.2 ogg_1.3.2)
add_dependencies(vorbis_1.3.2 ogg_1.3.2)
add_dependencies(snapcast ogg_1.3.2)
add_dependencies(snapcast vorbis_1.3.2)
add_dependencies(snapcast FLAC_1.3.2)
