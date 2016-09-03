cmake_minimum_required(VERSION 3.4)
project(snapcast)

include_directories(${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="FOOBAR")

find_package(Threads)
find_package(FLAC REQUIRED)
find_package(Vorbis REQUIRED)
find_package(Ogg REQUIRED)

if(WIN32)
  set(CMAKE_SYSTEM_NAME WindowsStore)
  set(CMAKE_SYSTEM_VERSION 10.0)
  add_definitions(-D_WIN32_WINNT=0x0600 -DWINDOWS -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE)
  find_package(Bonjour REQUIRED)
else()
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

# Set generators
if(UNIX)
  set(CPACK_GENERATOR "DEB" "TZ")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP" "NSIS")
endif()
set(CPACK_PACKAGING_INSTALL_PREFIX "/")

# Configure package settings
set(CPACK_PACKAGE_NAME "snapcast")
set(CPACK_PACKAGE_VENDOR "badaix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "https://github.com/badaix/snapcast")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "snapcast")
set(CPACK_PACKAGE_CONTACT "Johannes Pohl <johannes.pohl@badaix.de>")

# Debian specifics
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
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

# NSIS Specifics
set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/cmake/icon.ico)
set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/cmake/icon.ico)
set(CPACK_PACKAGE_ICON ${CMAKE_SOURCE_DIR}/cmake/installer.bmp)
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "")
set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME snapclient.exe)
set(CPACK_NSIS_HELP_LINK "https://github.com/badaix/snapcast")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/badaix/snapcast")
set(CPACK_NSIS_NSIS_CONTACT "Johannes Pohl <johannes.pohl@badaix.de>")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

# Enable component packages
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

include(CPack)
