set(CLIENT_SOURCES snapClient.cpp stream.cpp clientConnection.cpp timeProvider.cpp player/player.cpp decoder/pcmDecoder.cpp decoder/flacDecoder.cpp controller.cpp decoder/oggDecoder.cpp)

set(CLIENT_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${FLAC_LIBRARIES} ${OGG_LIBRARIES} ${VORBIS_LIBRARIES} ${OGG_LIBRARIES} common message)

set(CLIENT_INCLUDE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/externals/asio/asio/include ${CMAKE_SOURCE_DIR}/externals/popl/include ${CMAKE_SOURCE_DIR}/client ${FLAC_INCLUDE_DIRS} ${OGG_INCLUDE_DIRS} ${VORBIS_INCLUDE_DIRS})

add_definitions(-DHAS_OGG -DASIO_STANDALONE)

if (UNIX AND NOT ANDROID)
  if (AVAHI_FOUND)
    list(APPEND CLIENT_SOURCES browseZeroConf/browseAvahi.cpp)
    list(APPEND CLIENT_LIBRARIES ${AVAHI_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${AVAHI_INCLUDE_DIRS})
    add_definitions(-DHAS_AVAHI)
  elseif (mDNSResponder_FOUND)
    list(APPEND CLIENT_SOURCES browserZeroConf/browseBonjour.cpp)
    list(APPEND CLIENT_LIBRARIES ${mDNSResponder_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${mDNSResponder_INCLUDE_DIRS})
    add_definitions(-DHAS_BONJOUR)
  endif()
  list(APPEND CLIENT_SOURCES player/alsaPlayer.cpp)
  list(APPEND CLIENT_LIBRARIES ${ALSA_LIBRARIES})
  add_definitions(-DHAS_ALSA -DHAS_DAEMON)
  list(APPEND CLIENT_INCLUDE ${ALSA_INCLUDE_DIRS})
elseif (ANDROID)
  add_definitions(-DHAS_OPENSL -DANDROID)
  list(APPEND CLIENT_SOURCES player/openslPlayer.cpp)
  list(APPEND CLIENT_LIBRARIES OpenSLES ${CMAKE_ANDROID_STL_TYPE} atomic)
elseif (WIN32)
  configure_file(snapclient.rc.in snapclient.rc)
  list(APPEND CLIENT_SOURCES player/wasapiPlayer.cpp ${CMAKE_BINARY_DIR}/client/snapclient.rc)
  add_definitions(-DHAS_WASAPI -DHAS_BONJOUR)
  list(APPEND CLIENT_LIBRARIES ${mDNSResponder_LIBRARIES} wsock32 ws2_32 avrt ksuser iphlpapi)
  list(APPEND CLIENT_INCLUDE ${mDNSResponder_INCLUDE_DIR})
endif()

include_directories(${CLIENT_INCLUDE})
add_executable(snapclient ${CLIENT_SOURCES})
target_link_libraries(snapclient ${CLIENT_LIBRARIES})

if(UNIX)
  install(TARGETS snapclient COMPONENT client DESTINATION sbin)
  install(FILES snapclient.1 COMPONENT client DESTINATION usr/share/man/man1/snapclient.1)
  install(FILES ${CMAKE_SOURCE_DIR}/platform/debian/client/copyright COMPONENT client DESTINATION usr/share/doc/snapclient)
  install(FILES ${CMAKE_SOURCE_DIR}/platform/debian/client/snapclient.service COMPONENT client DESTINATION lib/systemd/system)
  install(FILES ${CMAKE_SOURCE_DIR}/platform/debian/client/snapclient.init COMPONENT client DESTINATION etc/init.d RENAME snapclient)
  install(FILES ${CMAKE_SOURCE_DIR}/platform/debian/client/snapclient.default COMPONENT client DESTINATION etc/default RENAME snapclient)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../README.md.gz COMPONENT client DESTINATION share/doc/snapclient)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../changelog.gz COMPONENT client DESTINATION share/doc/snapclient)
elseif(WIN32)
  install(TARGETS snapclient COMPONENT client DESTINATION ./)
  if (ARCH STREQUAL "x86_64")
    set(ARCH_PATH "x64")
    set(SOURCE_WXS "${CMAKE_SOURCE_DIR}/platform/windows/client/bundle.x86_64.wxs.in")
    set(MSI_SUFFIX "win64")
  elseif (ARCH STREQUAL "i386")
    set(ARCH_PATH "Win32")
    set(SOURCE_WXS "${CMAKE_SOURCE_DIR}/platform/windows/client/bundle.i386.wxs.in")
    set(MSI_SUFFIX "win32")
  else()
    message(FATAL_ERROR "Trying to build for windows on arch other than x86_64/i386.  Failing")
  endif()
  set(SNAPCLIENT_MSI ${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${MSI_SUFFIX}.msi)

  find_package(Wix REQUIRED)
  set(BUNDLE_OUTPUT   ${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${MSI_SUFFIX}.exe)
  set(SOURCE_WIX_FILE ${CMAKE_CURRENT_BINARY_DIR}/bundle.wxs)
  set(OUTPUT_WIXOBJ   ${CMAKE_CURRENT_BINARY_DIR}/bundle.wixobj)
  configure_file(${SOURCE_WXS} ${SOURCE_WIX_FILE})
  ADD_CUSTOM_COMMAND(
    OUTPUT    ${OUTPUT_WIXOBJ}
    COMMAND   ${WIX_CANDLE}
    ARGS      ${SOURCE_WIX_FILE} -ext WixBalExtension
    DEPENDS   ${SOURCE_WIX_FILE}
    COMMENT   "Compiling ${SOURCE_WIX_FILE} -> ${OUTPUT_WIXOBJ}"
  )
  ADD_CUSTOM_COMMAND( 
    OUTPUT    ${BUNDLE_OUTPUT}
    COMMAND   ${WIX_LIGHT}
    ARGS      -out ${BUNDLE_OUTPUT} -ext WixBalExtension ${OUTPUT_WIXOBJ}
    DEPENDS   ${OUTPUT_WIXOBJ}
    COMMENT   "Linking ${OUTPUT_WIXOBJ} -> ${BUNDLE_OUTPUT}"
  )
  add_custom_target("BUNDLE" DEPENDS ${BUNDLE_OUTPUT})
  add_dependencies("BUNDLE" "PACKAGE")
endif()
