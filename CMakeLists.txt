cmake_minimum_required(VERSION 3.2)

project(snapcast LANGUAGES CXX VERSION 0.13.0)
set(PROJECT_DESCRIPTION "Multi-room client-server audio player")
set(PROJECT_URL "https://github.com/badaix/snapcast")

option(BUILD_SHARED_LIBS "Build snapcast in a shared context" ON)
option(BUILD_STATIC_LIBS "Build snapcast in a static context" ON)
option(BUILD_TESTS "Build tests (run tests with make test)" ON)

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "One or both of BUILD_SHARED_LIBS or BUILD_STATIC_LIBS"
                        "must be set to ON to build")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set (ANDROID TRUE)
    message(FATAL_ERROR "Android not yet supported")
endif()

# Configure paths
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
    SET(CMAKE_INSTALL_BINDIR bin CACHE
    PATH "Output directory for binary files")
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    SET(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Output directory for libraries")
endif()

if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
    SET(CMAKE_INSTALL_INCLUDEDIR include CACHE
        PATH "Output directory for header files")
endif()

include_directories("${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/externals/asio/asio/include"
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="${PROJECT_VERSION}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Configure compiler options
set(CMAKE_CXX_STANDARD 11)

# Get arch
include(${CMAKE_SOURCE_DIR}/cmake/TargetArch.cmake)
target_architecture(ARCH)

#message(STATUS "System name:  ${CMAKE_SYSTEM_NAME}")
#message(STATUS "Architecture: ${ARCH}")
#message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")

INCLUDE(CheckLibraryExists)
check_library_exists(atomic __atomic_fetch_add_4 "" HAS_LIBATOMIC)
if(HAS_LIBATOMIC)
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN)
IF(${BIGENDIAN})
    add_definitions("-DIS_BIG_ENDIAN")
ENDIF(${BIGENDIAN})


#set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by CMakeLists.txt." FORCE)

# Check dependencies
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

include(CMakePushCheckState)
include(CheckIncludeFileCXX)

cmake_push_check_state()
set (CMAKE_REQUIRED_INCLUDES "${CMAKE_SOURCE_DIR}/externals/asio/asio/include")
check_include_file_cxx(asio.hpp HAS_ASIO_HPP "-pthread -DASIO_STANDALONE -std=c++11")
cmake_pop_check_state()
if(HAS_ASIO_HPP)
    add_definitions("-DHAS_ASIO_HPP -DASIO_STANDALONE")
else()
    message(FATAL_ERROR "Need to have asio installed")
endif()

check_include_file_cxx(popl.hpp HAS_POPL_HPP -std=c++11)
if(NOT HAS_POPL_HPP)
    message(FATAL_ERROR "Need to have popl installed")
endif()

check_include_file_cxx(aixlog.hpp HAS_AIXLOG_HPP -std=c++11)
if(NOT HAS_AIXLOG_HPP)
    message(FATAL_ERROR "Need to have aixlog installed")
endif()

if(MACOSX)
    set(BONJOUR_FOUND true)
#    pkg_search_module(BONJOUR REQUIRED bonjour)
    if (BONJOUR_FOUND)
        add_definitions(-DHAS_BONJOUR)
    endif (BONJOUR_FOUND)

    add_definitions(-DFREEBSD -DHAS_DAEMON)
elseif(ANDROID)
    add_definitions("-DNO_CPP11_STRING")
else()
    pkg_search_module(ALSA REQUIRED alsa)
    if (ALSA_FOUND)
        add_definitions(-DHAS_ALSA)
    endif (ALSA_FOUND)

    pkg_search_module(AVAHI REQUIRED avahi-client)
    if (AVAHI_FOUND)
        add_definitions(-DHAS_AVAHI)
    endif (AVAHI_FOUND)

    add_definitions(-DHAS_DAEMON)
endif()

pkg_search_module(FLAC flac)
if (FLAC_FOUND)
    add_definitions("-DHAS_FLAC")
endif (FLAC_FOUND)

pkg_search_module(OGG ogg)
if (OGG_FOUND)
    add_definitions("-DHAS_OGG")
endif (OGG_FOUND)

pkg_search_module(VORBIS vorbis)
if (VORBIS_FOUND)
    add_definitions("-DHAS_VORBIS")
endif (VORBIS_FOUND)

pkg_search_module(TREMOR vorbisidec)
if (TREMOR_FOUND)
    add_definitions("-DHAS_TREMOR")
endif (TREMOR_FOUND)

pkg_search_module(VORBISENC vorbisenc)
if (VORBISENC_FOUND)
    add_definitions("-DHAS_VORBISENC")
endif()


add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
