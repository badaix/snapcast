find_package(VorbisEnc REQUIRED)

set(SERVER_SOURCES snapServer.cpp config.cpp controlServer.cpp controlSession.cpp streamServer.cpp streamSession.cpp json/jsonrpc.cpp streamreader/streamUri.cpp streamreader/streamManager.cpp streamreader/pcmStream.cpp streamreader/pipeStream.cpp streamreader/fileStream.cpp encoder/encoderFactory.cpp encoder/flacEncoder.cpp encoder/pcmEncoder.cpp encoder/oggEncoder.cpp streamreader/airplayStream.cpp streamreader/spotifyStream.cpp streamreader/watchdog.cpp streamreader/processStream.cpp)

set(SERVER_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${FLAC_LIBRARIES} ${OGG_LIBRARIES} ${VORBIS_LIBRARIES} ${VORBISENC_LIBRARIES} common message)

set(SERVER_INCLUDE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/externals/asio/asio/include ${CMAKE_SOURCE_DIR}/externals/popl/include ${CMAKE_SOURCE_DIR}/server ${FLAC_INCLUDE_DIRS} ${VORBIS_INCLUDE_DIRS} ${OGG_INCLUDE_DIRS})

if (APPLE)
  list(APPEND SERVER_SOURCES publishZeroConf/publishBonjour.cpp)
elseif (UNIX)
  list(APPEND SERVER_SOURCES publishZeroConf/publishAvahi.cpp)
  list(APPEND SERVER_LIBRARIES ${AVAHI_LIBRARIES} rt)
  list(APPEND SERVER_INCLUDE ${AVAHI_INCLUDE_DIRS})
endif()

include_directories(${SERVER_INCLUDE})
add_executable(snapserver ${SERVER_SOURCES})
target_link_libraries(snapserver ${SERVER_LIBRARIES})

install(TARGETS snapserver COMPONENT server DESTINATION bin)
if(UNIX)
  install(FILES snapserver.1 COMPONENT server DESTINATION share/man/man1/snapserver.1)
  install(FILES debian/copyright COMPONENT server DESTINATION usr/share/doc/snapserver)
  install(FILES debian/snapserver.service COMPONENT server DESTINATION lib/systemd/system)
  install(FILES debian/snapserver.init COMPONENT server DESTINATION etc/init.d RENAME snapserver)
  install(FILES debian/snapserver.default COMPONENT server DESTINATION etc/default RENAME snapserver)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../README.md.gz COMPONENT server DESTINATION share/doc/snapserver)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../changelog.gz COMPONENT server DESTINATION share/doc/snapserver)
endif()
