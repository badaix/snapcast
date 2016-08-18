project(snapserver)

find_package(VorbisEnc REQUIRED)

set(SERVER_SOURCES snapServer.cpp config.cpp controlServer.cpp controlSession.cpp streamServer.cpp streamSession.cpp json/jsonrpc.cpp streamreader/streamUri.cpp streamreader/streamManager.cpp streamreader/pcmStream.cpp streamreader/pipeStream.cpp streamreader/fileStream.cpp encoder/encoderFactory.cpp encoder/flacEncoder.cpp encoder/pcmEncoder.cpp encoder/oggEncoder.cpp publishAvahi.cpp)

set(SERVER_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${FLAC_LIBRARIES} ${OGG_LIBRARIES} ${VORBIS_LIBRARIES} ${VORBISENC_LIBRARIES} common message)

set(SERVER_INCLUDE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/externals/asio/asio/include ${CMAKE_SOURCE_DIR}/externals/popl/include ${CMAKE_SOURCE_DIR}/server ${FLAC_INCLUDE_DIRS} ${VORBIS_INCLUDE_DIRS} ${OGG_INCLUDE_DIRS})

if (UNIX)
  list(APPEND SERVER_LIBRARIES ${AVAHI_LIBRARIES} rt)
  list(APPEND SERVER_INCLUDE ${AVAHI_INCLUDE_DIRS})
endif()

include_directories(${SERVER_INCLUDE})
add_executable(snapserver ${SERVER_SOURCES})
target_link_libraries(snapserver ${SERVER_LIBRARIES})

install(TARGETS snapserver DESTINATION usr/bin)
install(FILES snapserver.1 DESTINATION usr/share/man/man1/snapserver.1)

include(CPack)
