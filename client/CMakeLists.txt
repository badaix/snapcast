set(CLIENT_SOURCES
    client_connection.cpp
    controller.cpp
    snapclient.cpp
    stream.cpp
    time_provider.cpp
    decoder/pcm_decoder.cpp
    decoder/null_decoder.cpp
    player/player.cpp
    player/file_player.cpp)

set(CLIENT_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${ATOMIC_LIBRARY} common)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/client)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})

if(MACOSX)
  # Bonjour
  if(BONJOUR_FOUND)
    list(APPEND CLIENT_SOURCES browseZeroConf/browse_bonjour.cpp)
  endif(BONJOUR_FOUND)

  # CoreAudio
  add_compile_definitions(HAS_COREAUDIO)
  list(APPEND CLIENT_SOURCES player/coreaudio_player.cpp)
  find_library(COREAUDIO_LIB CoreAudio)
  find_library(COREFOUNDATION_LIB CoreFoundation)
  find_library(IOKIT_LIB IOKit)
  find_library(AUDIOTOOLBOX_LIB AudioToolbox)
  list(APPEND CLIENT_LIBRARIES ${COREAUDIO_LIB} ${COREFOUNDATION_LIB}
       ${IOKIT_LIB} ${AUDIOTOOLBOX_LIB})
elseif(WIN32)
  list(APPEND CLIENT_SOURCES player/wasapi_player.cpp)
  list(
    APPEND
    CLIENT_LIBRARIES
    wsock32
    ws2_32
    avrt
    ksuser
    iphlpapi)
elseif(NOT ANDROID)
  # Avahi
  if(AVAHI_FOUND)
    list(APPEND CLIENT_SOURCES browseZeroConf/browse_avahi.cpp)
    list(APPEND CLIENT_LIBRARIES ${AVAHI_LIBRARIES})
    include_directories(${AVAHI_INCLUDE_DIRS})
  endif(AVAHI_FOUND)

  # ALSA
  if(ALSA_FOUND)
    list(APPEND CLIENT_SOURCES player/alsa_player.cpp)
    list(APPEND CLIENT_LIBRARIES ${ALSA_LIBRARIES})
    include_directories(${ALSA_INCLUDE_DIRS})
  endif(ALSA_FOUND)

  if(PULSE_FOUND)
    list(APPEND CLIENT_SOURCES player/pulse_player.cpp)
    list(APPEND CLIENT_LIBRARIES ${PULSE_LIBRARIES})
    include_directories(${PULSE_INCLUDE_DIRS})
  endif(PULSE_FOUND)
endif(MACOSX)

if(ANDROID)
  list(APPEND CLIENT_LIBRARIES oboe::oboe)
  list(APPEND CLIENT_LIBRARIES boost::boost)
  list(APPEND CLIENT_LIBRARIES flac::flac)
  list(APPEND CLIENT_LIBRARIES opus::opus)
  list(APPEND CLIENT_LIBRARIES tremor::tremor)
  list(APPEND CLIENT_LIBRARIES ogg::ogg)
  list(APPEND CLIENT_SOURCES player/oboe_player.cpp)
  list(APPEND CLIENT_SOURCES player/opensl_player.cpp)
  list(APPEND CLIENT_SOURCES decoder/flac_decoder.cpp)
  list(APPEND CLIENT_SOURCES decoder/ogg_decoder.cpp)
  list(APPEND CLIENT_SOURCES decoder/opus_decoder.cpp)
  list(APPEND CLIENT_LIBRARIES OpenSLES)

else()
  # Tremor (fixed-point) or libvorbis (floating-point)
  if(TREMOR_FOUND)
    list(APPEND CLIENT_LIBRARIES ${TREMOR_LIBRARIES})
    include_directories(${TREMOR_INCLUDE_DIRS})
  elseif(VORBIS_FOUND)
    list(APPEND CLIENT_LIBRARIES ${VORBIS_LIBRARIES})
    include_directories(${VORBIS_INCLUDE_DIRS})
    link_directories(${VORBIS_LIBRARY_DIRS})
  endif(TREMOR_FOUND)

  # if OGG then tremor or vorbis
  if(OGG_FOUND)
    list(APPEND CLIENT_SOURCES decoder/ogg_decoder.cpp)
    list(APPEND CLIENT_LIBRARIES ${OGG_LIBRARIES})
    include_directories(${OGG_INCLUDE_DIRS})
    link_directories(${OGG_LIBRARY_DIRS})
  endif(OGG_FOUND)

  if(FLAC_FOUND)
    list(APPEND CLIENT_SOURCES decoder/flac_decoder.cpp)
    list(APPEND CLIENT_LIBRARIES ${FLAC_LIBRARIES})
    include_directories(${FLAC_INCLUDE_DIRS})
    link_directories(${FLAC_LIBRARY_DIRS})
  endif(FLAC_FOUND)

  if(OPUS_FOUND)
    list(APPEND CLIENT_SOURCES decoder/opus_decoder.cpp)
    list(APPEND CLIENT_LIBRARIES ${OPUS_LIBRARIES})
    include_directories(${OPUS_INCLUDE_DIRS})
    link_directories(${OPUS_LIBRARY_DIRS})
  endif(OPUS_FOUND)

  if(SOXR_FOUND)
    list(APPEND CLIENT_LIBRARIES ${SOXR_LIBRARIES})
    include_directories(${SOXR_INCLUDE_DIRS})
    link_directories(${SOXR_LIBRARY_DIRS})
  endif(SOXR_FOUND)
endif()

if(ANDROID)
  add_executable(libsnapclient.so ${CLIENT_SOURCES})
  target_link_libraries(libsnapclient.so ${CLIENT_LIBRARIES} log OpenSLES)
else()
  add_executable(snapclient ${CLIENT_SOURCES})
  target_link_libraries(snapclient ${CLIENT_LIBRARIES})

  install(
    TARGETS snapclient
    COMPONENT client
    DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(
    FILES snapclient.1
    COMPONENT client
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
  install(
    FILES etc/snapcast.svg
    COMPONENT client
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
endif()
