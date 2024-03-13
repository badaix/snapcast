set(SERVER_SOURCES
    config.cpp
    control_server.cpp
    control_session_tcp.cpp
    control_session_http.cpp
    control_session_ws.cpp
    snapserver.cpp
    server.cpp
    stream_server.cpp
    stream_session.cpp
    stream_session_tcp.cpp
    stream_session_ws.cpp
    encoder/encoder_factory.cpp
    encoder/pcm_encoder.cpp
    encoder/null_encoder.cpp
    streamreader/base64.cpp
    streamreader/control_error.cpp
    streamreader/stream_control.cpp
    streamreader/stream_uri.cpp
    streamreader/stream_manager.cpp
    streamreader/pcm_stream.cpp
    streamreader/tcp_stream.cpp
    streamreader/pipe_stream.cpp
    streamreader/file_stream.cpp
    streamreader/airplay_stream.cpp
    streamreader/librespot_stream.cpp
    streamreader/meta_stream.cpp
    streamreader/watchdog.cpp
    streamreader/properties.cpp
    streamreader/metadata.cpp
    streamreader/process_stream.cpp)

set(SERVER_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${ATOMIC_LIBRARY} common)

set(SERVER_INCLUDE ${Boost_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/server
                   ${CMAKE_SOURCE_DIR})

if(ANDROID)
  find_package(vorbis REQUIRED CONFIG)
  list(APPEND SERVER_LIBRARIES boost::boost)
  list(APPEND SERVER_LIBRARIES flac::flac)
  list(APPEND SERVER_LIBRARIES opus::opus)
  list(APPEND SERVER_LIBRARIES ogg::ogg)
  list(APPEND SERVER_LIBRARIES vorbis::vorbis)
  list(APPEND SERVER_SOURCES encoder/flac_encoder.cpp)
  list(APPEND SERVER_SOURCES encoder/ogg_encoder.cpp)
  list(APPEND SERVER_SOURCES encoder/opus_encoder.cpp)

else()
  # Avahi
  if(AVAHI_FOUND)
    list(APPEND SERVER_SOURCES publishZeroConf/publish_avahi.cpp)
    list(APPEND SERVER_LIBRARIES ${AVAHI_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${AVAHI_INCLUDE_DIRS})
  endif(AVAHI_FOUND)

  if(BONJOUR_FOUND)
    list(APPEND SERVER_SOURCES publishZeroConf/publish_bonjour.cpp)
    # list(APPEND SERVER_LIBRARIES ${BONJOUR_LIBRARIES}) list(APPEND
    # SERVER_INCLUDE ${BONJOUR_INCLUDE_DIRS})
  endif(BONJOUR_FOUND)

  if(OGG_FOUND
     AND VORBIS_FOUND
     AND VORBISENC_FOUND)
    list(APPEND SERVER_SOURCES encoder/ogg_encoder.cpp)
    list(APPEND SERVER_LIBRARIES ${OGG_LIBRARIES} ${VORBIS_LIBRARIES}
         ${VORBISENC_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${OGG_INCLUDE_DIRS} ${VORBIS_INCLUDE_DIRS}
         ${VORBISENC_INCLUDE_DIRS})
  endif(
    OGG_FOUND
    AND VORBIS_FOUND
    AND VORBISENC_FOUND)

  if(FLAC_FOUND)
    list(APPEND SERVER_SOURCES encoder/flac_encoder.cpp)
    list(APPEND SERVER_LIBRARIES ${FLAC_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${FLAC_INCLUDE_DIRS})
  endif(FLAC_FOUND)

  if(OPUS_FOUND)
    list(APPEND SERVER_SOURCES encoder/opus_encoder.cpp)
    list(APPEND SERVER_LIBRARIES ${OPUS_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${OPUS_INCLUDE_DIRS})
  endif(OPUS_FOUND)

  if(ALSA_FOUND)
    list(APPEND SERVER_SOURCES streamreader/alsa_stream.cpp)
    list(APPEND SERVER_LIBRARIES ${ALSA_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${ALSA_INCLUDE_DIRS})
  endif(ALSA_FOUND)

  if(EXPAT_FOUND)
    list(APPEND SERVER_LIBRARIES ${EXPAT_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${EXPAT_INCLUDE_DIRS})
  endif(EXPAT_FOUND)
endif()

# list(APPEND SERVER_LIBRARIES Boost::boost)

include_directories(${SERVER_INCLUDE})
if(ANDROID)
  add_executable(libsnapserver.so ${SERVER_SOURCES})
  target_link_libraries(libsnapserver.so ${SERVER_LIBRARIES} log)
else()
  add_executable(snapserver ${SERVER_SOURCES})
  target_link_libraries(snapserver ${SERVER_LIBRARIES})

  install(
    TARGETS snapserver
    COMPONENT server
    DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(
    FILES snapserver.1
    COMPONENT server
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
  install(
    FILES etc/snapserver.conf
    COMPONENT server
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
  install(
    FILES etc/index.html
    COMPONENT server
    DESTINATION ${CMAKE_INSTALL_DATADIR}/snapserver)
  if(SNAPWEB_DIR)
    install(DIRECTORY "${SNAPWEB_DIR}/"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/snapserver/snapweb)
  else()
    install(DIRECTORY etc/snapweb/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/snapserver/snapweb)
  endif()

  install(
    FILES etc/plug-ins/meta_mpd.py etc/plug-ins/meta_mopidy.py
          etc/plug-ins/meta_librespot-java.py
    PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_EXECUTE
      WORLD_READ
      WORLD_EXECUTE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/snapserver/plug-ins/)
  # install(FILES ../debian/snapserver.service DESTINATION
  # ${SYSTEMD_SERVICES_INSTALL_DIR})
endif()
