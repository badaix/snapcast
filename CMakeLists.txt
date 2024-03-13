cmake_minimum_required(VERSION 3.5)

project(
  snapcast
  LANGUAGES CXX
  VERSION 0.28.0.1)

set(PROJECT_DESCRIPTION "Multiroom client-server audio player")
set(PROJECT_URL "https://github.com/badaix/snapcast")

option(BUILD_SHARED_LIBS "Build snapcast in a shared context" ON)
option(BUILD_STATIC_LIBS "Build snapcast in a static context" ON)
option(BUILD_TESTS "Build tests (in test/snapcast_test)" OFF)
option(WERROR "Treat warnings as errors" OFF)

option(ASAN "Enable AddressSanitizer" OFF)
option(TSAN "Enable ThreadSanitizer" OFF)
option(UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

option(TIDY "Enable clang tidy" OFF)

if(REVISION)
  add_compile_definitions(REVISION=\"${REVISION}\")
endif()

if(TIDY)
  find_program(CLANG_TIDY "clang-tidy")
  if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(MACOSX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  set(FREEBSD TRUE)
  if(BUILD_CLIENT)
    message(
      FATAL_ERROR
        "Snapclient not yet supported for FreeBSD, use \"-DBUILD_CLIENT=OFF\"")
  endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  set(ANDROID TRUE)
endif()

if(MSVC)
  # warning level 4 and all warnings as errors warning C4505: 'getArch':
  # unreferenced local function has been removed warning C4458: declaration of
  # 'size' hides class member warning C4459: declaration of 'query' hides global
  # declaration
  add_compile_options(/W4 /wd4458 /wd4459 /wd4505)
  if(WERROR)
    add_compile_options(/WX)
  endif()
else()
  # lots of warnings and all warnings as errors
  add_compile_options(-Wall -Wextra -pedantic -Wno-unused-function)
  if(MACOSX)
    add_compile_options(-Wno-deprecated-declarations)
  endif()

  if(WERROR)
    add_compile_options(-Werror)
  endif()

  if(ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()

  if(TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
  endif()

  if(UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()

include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/cppcheck.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/reformat.cmake)

if(NOT WIN32)
  option(BUILD_SERVER "Build Snapserver" ON) # no Windows server for now
endif()

option(BUILD_CLIENT "Build Snapclient" ON)

option(BUILD_WITH_FLAC "Build with FLAC support" ON)
option(BUILD_WITH_VORBIS "Build with VORBIS support" ON)
option(BUILD_WITH_TREMOR "Build with vorbis using TREMOR" ON)
option(BUILD_WITH_OPUS "Build with OPUS support" ON)
option(BUILD_WITH_AVAHI "Build with AVAHI support" ON)
option(BUILD_WITH_EXPAT "Build with EXPAT support" ON)
option(BUILD_WITH_PULSE "Build with PulseAudio support" ON)

if(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
  message(
    FATAL_ERROR
      "One or both of BUILD_SHARED_LIBS or BUILD_STATIC_LIBS must be set to ON to build"
  )
endif()

if(NOT BUILD_CLIENT AND NOT BUILD_SERVER)
  message(
    FATAL_ERROR
      "One or both of BUILD_CLIENT or BUILD_SERVER must be set to ON to build")
endif()

# Configure paths
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR
      bin
      CACHE PATH "Output directory for binary files")
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR
      lib
      CACHE PATH "Output directory for libraries")
endif()

if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR
      include
      CACHE PATH "Output directory for header files")
endif()

set(INCLUDE_DIRS "${CMAKE_SOURCE_DIR}" "${CMAKE_INSTALL_INCLUDEDIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_compile_definitions(VERSION="${PROJECT_VERSION}")

if(NOT ANDROID)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()

# Configure compiler options
set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_SOURCE_DIR}/cmake/TargetArch.cmake)
target_architecture(HOST_ARCH)

# message(STATUS "System name:  ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${HOST_ARCH}")
# message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")

if(ARCH)
  message(STATUS "Using arch: ${ARCH}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${ARCH}")
endif()

include(CheckAtomic)

include(TestBigEndian)
test_big_endian(BIGENDIAN)
if(${BIGENDIAN})
  add_compile_definitions(IS_BIG_ENDIAN)
endif(${BIGENDIAN})

# Check dependencies

if(NOT WIN32) # no PkgConfig on Windows...
  find_package(PkgConfig REQUIRED)
endif()

find_package(Threads REQUIRED)

include(CMakePushCheckState)
include(CheckIncludeFileCXX)
include_directories(${INCLUDE_DIRS})

include(${CMAKE_SOURCE_DIR}/cmake/CheckCXX11StringSupport.cmake)

check_cxx11_string_support(HAS_CXX11_STRING_SUPPORT)
if(NOT HAS_CXX11_STRING_SUPPORT)
  add_compile_definitions(NO_CPP11_STRING)
endif()

if(NOT WIN32 AND NOT ANDROID)

  if(MACOSX)
    set(BONJOUR_FOUND true)
    if(BONJOUR_FOUND)
      add_compile_definitions(HAS_BONJOUR)
    endif(BONJOUR_FOUND)

    add_compile_definitions(FREEBSD MACOS HAS_DAEMON)
    link_directories("/usr/local/lib")
    list(APPEND INCLUDE_DIRS "/usr/local/include")
  else()

    pkg_search_module(ALSA REQUIRED alsa)
    if(ALSA_FOUND)
      add_compile_definitions(HAS_ALSA)
    endif(ALSA_FOUND)

    if(BUILD_WITH_PULSE)
      pkg_search_module(PULSE libpulse)
      if(PULSE_FOUND)
        add_compile_definitions(HAS_PULSE)
      endif(PULSE_FOUND)
    endif(BUILD_WITH_PULSE)

    if(BUILD_WITH_AVAHI)
      pkg_search_module(AVAHI avahi-client)
      if(AVAHI_FOUND)
        add_compile_definitions(HAS_AVAHI)
      else()
        message(STATUS "avahi-client not found")
      endif(AVAHI_FOUND)
    endif(BUILD_WITH_AVAHI)

    add_compile_definitions(HAS_DAEMON)

    if(FREEBSD)
      add_compile_definitions(FREEBSD)
      link_directories("/usr/local/lib")
      list(APPEND INCLUDE_DIRS "/usr/local/include")
    endif()
  endif()

  pkg_search_module(SOXR soxr)
  if(SOXR_FOUND)
    add_compile_definitions(HAS_SOXR)
  else()
    message(STATUS "soxr not found")
  endif(SOXR_FOUND)

  if(BUILD_WITH_FLAC)
    pkg_search_module(FLAC flac)
    if(FLAC_FOUND)
      add_compile_definitions(HAS_FLAC)
    else()
      message(STATUS "flac not found")
    endif(FLAC_FOUND)
  endif()

  if(BUILD_WITH_VORBIS OR BUILD_WITH_TREMOR)
    pkg_search_module(OGG ogg)
    if(OGG_FOUND)
      add_compile_definitions(HAS_OGG)
    else()
      message(STATUS "ogg not found")
    endif(OGG_FOUND)
  endif()

  if(BUILD_WITH_VORBIS)
    pkg_search_module(VORBIS vorbis)
    if(VORBIS_FOUND)
      add_compile_definitions(HAS_VORBIS)
    endif(VORBIS_FOUND)
  endif()

  if(BUILD_WITH_TREMOR)
    pkg_search_module(TREMOR vorbisidec)
    if(TREMOR_FOUND)
      add_compile_definitions(HAS_TREMOR)
    endif(TREMOR_FOUND)
  endif()

  if((BUILD_WITH_VORBIS OR BUILD_WITH_TREMOR)
     AND NOT VORBIS_FOUND
     AND NOT TREMOR_FOUND)
    message(STATUS "tremor and vorbis not found")
  endif()

  if(BUILD_WITH_VORBIS)
    pkg_search_module(VORBISENC vorbisenc)
    if(VORBISENC_FOUND)
      add_compile_definitions(HAS_VORBIS_ENC)
    else()
      message(STATUS "vorbisenc not found")
    endif(VORBISENC_FOUND)
  endif()

  if(BUILD_WITH_OPUS)
    pkg_search_module(OPUS opus)
    if(OPUS_FOUND)
      add_compile_definitions(HAS_OPUS)
    else()
      message(STATUS "opus not found")
    endif(OPUS_FOUND)
  endif()

  if(BUILD_WITH_EXPAT)
    pkg_search_module(EXPAT expat)
    if(EXPAT_FOUND)
      add_compile_definitions(HAS_EXPAT)
    else()
      message(STATUS "expat not found")
    endif(EXPAT_FOUND)
  endif()
endif()

if(NOT ANDROID)
  find_package(Boost 1.74 REQUIRED)
else()
  find_package(oboe REQUIRED CONFIG)
  find_package(flac REQUIRED CONFIG)
  find_package(ogg REQUIRED CONFIG)
  find_package(opus REQUIRED CONFIG)
  find_package(soxr REQUIRED CONFIG)
  find_package(tremor REQUIRED CONFIG)
  find_package(boost REQUIRED CONFIG)

  add_compile_definitions(HAS_OBOE)
  add_compile_definitions(HAS_OPENSL)
  add_compile_definitions(HAS_FLAC)
  add_compile_definitions(HAS_OGG)
  add_compile_definitions(HAS_OPUS)
  add_compile_definitions(HAS_SOXR)
  add_compile_definitions(HAS_TREMOR)
endif()

add_compile_definitions(BOOST_ERROR_CODE_HEADER_ONLY BOOST_ASIO_NO_TS_EXECUTORS)

if(WIN32)
  include(FindPackageHandleStandardArgs)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

  find_path(FLAC_INCLUDE_DIRS FLAC/all.h)
  find_library(FLAC_LIBRARIES FLAC)
  find_package_handle_standard_args(FLAC REQUIRED FLAC_INCLUDE_DIRS
                                    FLAC_LIBRARIES)

  find_path(OGG_INCLUDE_DIRS ogg/ogg.h)
  find_library(OGG_LIBRARIES ogg)
  find_package_handle_standard_args(Ogg REQUIRED OGG_INCLUDE_DIRS OGG_LIBRARIES)

  find_path(VORBIS_INCLUDE_DIRS vorbis/vorbisenc.h)
  find_library(VORBIS_LIBRARIES vorbis)
  find_package_handle_standard_args(Vorbis REQUIRED VORBIS_INCLUDE_DIRS
                                    VORBIS_LIBRARIES)

  find_path(OPUS_INCLUDE_DIRS opus/opus.h)
  find_library(OPUS_LIBRARIES opus)
  find_package_handle_standard_args(Opus REQUIRED OPUS_INCLUDE_DIRS
                                    OPUS_LIBRARIES)

  find_path(SOXR_INCLUDE_DIRS soxr.h)
  find_library(SOXR_LIBRARIES soxr)
  find_package_handle_standard_args(Soxr REQUIRED SOXR_INCLUDE_DIRS
                                    SOXR_LIBRARIES)

  add_compile_definitions(
    NTDDI_VERSION=0x06020000
    _WIN32_WINNT=0x0602
    WINVER=0x0602
    WINDOWS
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(
    HAS_OGG
    HAS_VORBIS
    HAS_FLAC
    HAS_VORBIS_ENC
    HAS_OPUS
    HAS_WASAPI
    HAS_SOXR)
endif()

list(APPEND CMAKE_REQUIRED_INCLUDES "${INCLUDE_DIRS}")

# include(${CMAKE_SOURCE_DIR}/cmake/SystemdService.cmake)

add_subdirectory(common)

if(BUILD_SERVER)
  add_subdirectory(server)
endif()

if(BUILD_CLIENT)
  add_subdirectory(client)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif(BUILD_TESTS)
