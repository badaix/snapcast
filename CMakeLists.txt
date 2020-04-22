cmake_minimum_required(VERSION 3.2)

project(snapcast LANGUAGES CXX VERSION 0.19.0)
set(PROJECT_DESCRIPTION "Multiroom client-server audio player")
set(PROJECT_URL "https://github.com/badaix/snapcast")

option(BUILD_SHARED_LIBS "Build snapcast in a shared context" ON)
option(BUILD_STATIC_LIBS "Build snapcast in a static context" ON)
option(BUILD_TESTS "Build tests (run tests with make test)" ON)

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


if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "One or both of BUILD_SHARED_LIBS or BUILD_STATIC_LIBS must be set to ON to build")
endif()

if (NOT BUILD_CLIENT AND NOT BUILD_SERVER)
    message(FATAL_ERROR "One or both of BUILD_CLIENT or BUILD_SERVER must be set to ON to build")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set (FREEBSD TRUE)
    if (BUILD_CLIENT)
        message(FATAL_ERROR "Snapclient not yet supported for FreeBSD, use \"-DBUILD_CLIENT=OFF\"")
    endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set (ANDROID TRUE)
    add_definitions("-DASIO_DISABLE_STD_FUTURE")
    if (BUILD_SERVER)
        message(FATAL_ERROR "Snapserver not yet supported for Android, use \"-DBUILD_SERVER=OFF\"")
    endif()
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

set(INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_INSTALL_INCLUDEDIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="${PROJECT_VERSION}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Configure compiler options
set(CMAKE_CXX_STANDARD 14)

# Get arch
include(${CMAKE_SOURCE_DIR}/cmake/TargetArch.cmake)
target_architecture(ARCH)

#message(STATUS "System name:  ${CMAKE_SYSTEM_NAME}")
#message(STATUS "Architecture: ${ARCH}")
#message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")

include(CheckAtomic)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN)
IF(${BIGENDIAN})
    add_definitions("-DIS_BIG_ENDIAN")
ENDIF(${BIGENDIAN})

# Check dependencies

if(NOT WIN32) # no PkgConfig on Windows...
    find_package(PkgConfig REQUIRED)
endif()

find_package(Threads REQUIRED)

include(CMakePushCheckState)
include(CheckIncludeFileCXX)
include_directories(${INCLUDE_DIRS})

include(${CMAKE_SOURCE_DIR}/cmake/CheckCXX11StringSupport.cmake)

CHECK_CXX11_STRING_SUPPORT(HAS_CXX11_STRING_SUPPORT)
if(NOT HAS_CXX11_STRING_SUPPORT)
    add_definitions("-DNO_CPP11_STRING")
endif()


if(NOT WIN32)

    if(MACOSX)
        set(BONJOUR_FOUND true)
        if (BONJOUR_FOUND)
            add_definitions(-DHAS_BONJOUR)
        endif (BONJOUR_FOUND)

        add_definitions(-DFREEBSD -DHAS_DAEMON)
        link_directories("/usr/local/lib")
        list(APPEND INCLUDE_DIRS "/usr/local/include")
        list(APPEND CMAKE_REQUIRED_INCLUDES "${INCLUDE_DIRS}")
    elseif(ANDROID)
    #    add_definitions("-DNO_CPP11_STRING")
    else()
        if (BUILD_CLIENT)
            pkg_search_module(ALSA REQUIRED alsa)
            if (ALSA_FOUND)
                add_definitions(-DHAS_ALSA)
            endif (ALSA_FOUND)
        endif()

        if(BUILD_WITH_AVAHI)
            pkg_search_module(AVAHI avahi-client)
            if (AVAHI_FOUND)
                add_definitions(-DHAS_AVAHI)
            endif (AVAHI_FOUND)
        endif(BUILD_WITH_AVAHI)

        add_definitions(-DHAS_DAEMON)

        if(FREEBSD)
            add_definitions(-DFREEBSD)
            link_directories("/usr/local/lib")
            list(APPEND INCLUDE_DIRS "/usr/local/include")
            list(APPEND CMAKE_REQUIRED_INCLUDES "${INCLUDE_DIRS}")
        endif()
    endif()


    if(BUILD_WITH_FLAC)
        pkg_search_module(FLAC flac)
        if (FLAC_FOUND)
            add_definitions("-DHAS_FLAC")
        endif (FLAC_FOUND)
    endif()

    if(BUILD_WITH_VORBIS OR BUILD_WITH_TREMOR)
        pkg_search_module(OGG ogg)
        if (OGG_FOUND)
            add_definitions("-DHAS_OGG")
        endif (OGG_FOUND)
    endif()

    if(BUILD_WITH_VORBIS)
        pkg_search_module(VORBIS vorbis)
        if (VORBIS_FOUND)
            add_definitions("-DHAS_VORBIS")
        endif (VORBIS_FOUND)
    endif()

    if(BUILD_WITH_TREMOR)
        pkg_search_module(TREMOR vorbisidec)
        if (TREMOR_FOUND)
            add_definitions("-DHAS_TREMOR")
        endif (TREMOR_FOUND)
    endif()

    if(BUILD_WITH_VORBIS)
        pkg_search_module(VORBISENC vorbisenc)
        if (VORBISENC_FOUND)
            add_definitions("-DHAS_VORBIS_ENC")
        endif(VORBISENC_FOUND)
    endif()

    if(BUILD_WITH_OPUS)
        pkg_search_module(OPUS opus)
        if (OPUS_FOUND)
            add_definitions("-DHAS_OPUS")
        endif (OPUS_FOUND)
    endif()

    if(BUILD_WITH_EXPAT)
        pkg_search_module(EXPAT expat)
        if (EXPAT_FOUND)
            add_definitions("-DHAS_EXPAT")
        endif (EXPAT_FOUND)
    endif()
endif()

find_package(Boost 1.70 REQUIRED)
add_definitions("-DBOOST_ERROR_CODE_HEADER_ONLY")
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${BOOST_LIBRARY_DIRS})

if(WIN32)
	include(FindPackageHandleStandardArgs)

	find_path(FLAC_INCLUDE_DIRS FLAC/all.h)
	find_library(FLAC_LIBRARIES FLAC)
	find_package_handle_standard_args(FLAC DEFAULT_MSG FLAC_INCLUDE_DIRS FLAC_LIBRARIES)

	find_path(OGG_INCLUDE_DIRS ogg/ogg.h)
	find_library(OGG_LIBRARIES ogg)
	find_package_handle_standard_args(Ogg DEFAULT_MSG OGG_INCLUDE_DIRS OGG_LIBRARIES)

	find_path(VORBIS_INCLUDE_DIRS vorbis/vorbisenc.h)
	find_library(VORBIS_LIBRARIES vorbis)
	find_package_handle_standard_args(Vorbis DEFAULT_MSG VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)

	find_path(OPUS_INCLUDE_DIRS opus/opus.h)
	find_library(OPUS_LIBRARIES opus)
	find_package_handle_standard_args(Opus REQUIRED OPUS_INCLUDE_DIRS OPUS_LIBRARIES)

	add_definitions(-DNTDDI_VERSION=0x06020000 -D_WIN32_WINNT=0x0602 -DWINVER=0x0602 -DWINDOWS -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS )
	add_definitions(-DHAS_OGG -DHAS_VORBIS -DHAS_FLAC -DHAS_VORBIS_ENC -DHAS_OPUS -DHAS_WASAPI)
endif()

add_subdirectory(common)

if (BUILD_SERVER)
    add_subdirectory(server)
endif()

if (BUILD_CLIENT)
    add_subdirectory(client)
endif()
