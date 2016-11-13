cmake_minimum_required(VERSION 3.4)
project(snapcast VERSION 0.9.0.0)

# Configure paths
include_directories(${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="${PROJECT_VERSION}")
if(ANDROID)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/android/Snapcast/src/main/assets/bin/${ANDROID_ABI})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/android/Snapcast/src/main/assets/bin/${ANDROID_ABI})
  set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_SOURCE_DIR}/android/Snapcast/src/main/assets/bin/${ANDROID_ABI})
else()
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
  set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_SOURCE_DIR}/bin)
endif()

# Configure compiler options
set(CMAKE_CXX_STANDARD 11)
if(ANDROID)
  add_definitions(-DNO_CPP11_STRING)
endif()

# Configure compiler options
set (CMAKE_CXX_STANDARD 11)
if (ANDROID)
  add_definitions(-DNO_CPP11_STRING)
endif()

# Find dependencies
if (NOT ANDROID)
  find_package(Threads)
endif()
find_package(FLAC REQUIRED)
find_package(Vorbis REQUIRED)
find_package(Ogg REQUIRED)

# Get arch
include(${CMAKE_SOURCE_DIR}/cmake/TargetArch.cmake)
target_architecture(ARCH)

# Set generators
if(UNIX)
  set(CPACK_GENERATOR "DEB" "TZ")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP" "WIX")
endif()
set(CPACK_PACKAGING_INSTALL_PREFIX "/")

# Configure package settings
set(CPACK_PACKAGE_NAME "snapcast")
set(CPACK_PACKAGE_VENDOR "badaix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "https://github.com/badaix/snapcast")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "snapcast")
set(CPACK_PACKAGE_CONTACT "Johannes Pohl <johannes.pohl@badaix.de>")

# Debian specifics
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${ARCH})
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_COMPONENT_CLIENT_DESCRIPTION "Snapcast client\
 Snapcast is a multi-room client-server audio player, where all clients are\
 time synchronized with the server to play perfectly synced audio. It's not a\
 standalone player, but an extension that turns your existing audio player into\
 a Sonos-like multi-room solution.\
 .\
 The server's audio input is a named pipe `/tmp/snapfifo`. All data that is fed\
 into this file will be send to the connected clients. One of the most generic\
 ways to use Snapcast is in conjunction with the music player daemon or Mopidy,\
 which can be configured to use a named pipe as audio output.\
")
set(CPACK_COMPONENT_SERVER_DESCRIPTION "Snapcast server\n\
 Snapcast is a multi-room client-server audio player, where all clients are\n\
 time synchronized with the server to play perfectly synced audio. It's not a\n\
 standalone player, but an extension that turns your existing audio player into\n\
 a Sonos-like multi-room solution.\n\
 .\n\
 The server's audio input is a named pipe `/tmp/snapfifo`. All data that is fed\n\
 into this file will be send to the connected clients. One of the most generic\n\
 ways to use Snapcast is in conjunction with the music player daemon or Mopidy,\n\
 which can be configured to use a named pipe as audio output.\n\
")
set(CPACK_DEBIAN_CLIENT_PACKAGE_CONTROL_EXTRA ${CMAKE_SOURCE_DIR}/client/debian/conffiles ${CMAKE_SOURCE_DIR}/client/debian/prerm ${CMAKE_SOURCE_DIR}/client/debian/postrm ${CMAKE_SOURCE_DIR}/client/debian/postinit)
set(CPACK_DEBIAN_SERVER_PACKAGE_CONTROL_EXTRA ${CMAKE_SOURCE_DIR}/server/debian/conffiles ${CMAKE_SOURCE_DIR}/server/debian/prerm ${CMAKE_SOURCE_DIR}/server/debian/postrm ${CMAKE_SOURCE_DIR}/server/debian/postinit)

# WiX Specifics
set(CPACK_WIX_UPGRADE_GUID 7D46CFF3-436E-4381-A34C-7D87274A7FB4)
set(CPACK_WIX_PRODUCT_GUID 7D46CFF3-436E-4381-A34C-7D87274A7FB4)
set(CPACK_WIX_LICENSE_RTF ${CMAKE_SOURCE_DIR}/LICENSE.txt)
set(CPACK_WIX_PRODUCT_ICON ${CMAKE_SOURCE_DIR}/cmake/icon.ico)
set(CPACK_WIX_UI_BANNER ${CMAKE_SOURCE_DIR}/cmake/banner.bmp)
set(CPACK_WIX_UI_DIALOG ${CMAKE_SOURCE_DIR}/cmake/welcome.bmp)
set(MSI_ICON ${CMAKE_SOURCE_DIR}/cmake/icon.bmp)
set(CPACK_WIX_TEMPLATE ${CMAKE_SOURCE_DIR}/main.wxs)

# Enable component packages
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# Configure components
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0600 -DWINDOWS -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)
  find_package(Bonjour REQUIRED)
elseif(NOT ANDROID)
  find_package(Avahi REQUIRED)
  add_subdirectory(server)
endif()

add_subdirectory(common)
add_subdirectory(message)
add_subdirectory(client)

if(UNIX)
  add_custom_command(OUTPUT README.md.gz
    COMMAND cmake -E copy ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md
  	COMMAND gzip --force README.md)

  add_custom_command(OUTPUT changelog.gz
    COMMAND cmake -E copy ${CMAKE_SOURCE_DIR}/client/debian/changelog ${CMAKE_CURRENT_BINARY_DIR}/changelog
  	COMMAND gzip --force changelog)
	
  add_custom_target(doc ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/README.md.gz ${CMAKE_CURRENT_BINARY_DIR}/changelog.gz)
endif()

# Create CPack definition
include(CPack)
