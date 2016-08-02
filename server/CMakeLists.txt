find_package(Threads)

set(SERVER_SOURCES snapServer.cpp config.cpp controlServer.cpp controlSession.cpp streamServer.cpp streamSession.cpp json/jsonrpc.cpp streamreader/streamUri.cpp streamreader/streamManager.cpp streamreader/pcmStream.cpp streamreader/pipeStream.cpp streamreader/fileStream.cpp encoder/encoderFactory.cpp encoder/flacEncoder.cpp encoder/pcmEncoder.cpp encoder/oggEncoder.cpp publishAvahi.cpp ${PROJECT_SOURCE_DIR}/common/log.cpp ${PROJECT_SOURCE_DIR}/common/sampleFormat.cpp ${PROJECT_SOURCE_DIR}/message/pcmChunk.cpp)

set(SERVER_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

set(SERVER_INCLUDE ${PROJECT_SOURCE_DIR}/externals/asio/asio/include ${PROJECT_SOURCE_DIR}/externals/popl/include ${PROJECT_SOURCE_DIR}/server ${PROJECT_SOURCE_DIR})

if (UNIX)
  find_package(Avahi REQUIRED)
  find_package(FLAC REQUIRED)
  find_package(Vorbis REQUIRED)
  find_package(Ogg REQUIRED)
  find_package(VorbisEnc REQUIRED)
  list(APPEND SERVER_LIBRARIES ${AVAHI_LIBRARIES} ${FLAC_LIBRARIES} ${OGG_LIBRARIES} ${VORBIS_LIBRARIES} ${VORBISENC_LIBRARIES} rt)
  list(APPEND SERVER_INCLUDE ${AVAHI_INCLUDE_DIRS} ${FLAC_INCLUDE_DIRS} ${VORBIS_INCLUDE_DIRS} ${OGG_INCLUDE_DIRS})
endif()

include_directories(${SERVER_INCLUDE})
add_executable(snapserver ${SERVER_SOURCES})
target_link_libraries(snapserver ${SERVER_LIBRARIES})
