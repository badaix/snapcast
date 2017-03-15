set(SERVER_SOURCES
    config.cpp
    controlServer.cpp
    controlSession.cpp
    snapServer.cpp
    streamServer.cpp
    streamSession.cpp
    encoder/encoderFactory.cpp
    encoder/pcmEncoder.cpp
    streamreader/streamUri.cpp
    streamreader/streamManager.cpp
    streamreader/pcmStream.cpp
    streamreader/pipeStream.cpp
    streamreader/fileStream.cpp
    streamreader/airplayStream.cpp
    streamreader/spotifyStream.cpp
    streamreader/watchdog.cpp
    streamreader/processStream.cpp)

set(SERVER_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${JSONRPCPP_LIBRARIES}
    ${FLAC_LIBRARIES}
    common message)

set(SERVER_INCLUDE
    ${CMAKE_SOURCE_DIR}/server
    ${JSONRPCPP_INCLUDE_DIRS}
    ${FLAC_INCLUDE_DIRS})

# Avahi
list(APPEND SERVER_SOURCES publishZeroConf/publishAvahi.cpp)
list(APPEND SERVER_LIBRARIES ${AVAHI_LIBRARIES})
list(APPEND SERVER_INCLUDE ${AVAHI_INCLUDE_DIRS})

if (OGG_FOUND AND VORBIS_FOUND AND VORBISENC_FOUND)
    list(APPEND SERVER_SOURCES encoder/oggEncoder.cpp)
    list(APPEND SERVER_LIBRARIES
        ${OGG_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${VORBISENC_LIBRARIES})
    list(APPEND SERVER_INCLUDE
        ${OGG_INCLUDE_DIRS}
        ${VORBIS_INCLUDE_DIRS}
        ${VORBISENC_INCLUDE_DIRS})
endif (OGG_FOUND AND VORBIS_FOUND AND VORBIS_ENC_FOUND)

if (FLAC_FOUND)
    list(APPEND SERVER_SOURCES encoder/flacEncoder.cpp)
    list(APPEND SERVER_LIBRARIES ${FLAC_LIBRARIES})
    list(APPEND SERVER_INCLUDE ${FLAC_INCLUDE_DIRS})
endif (FLAC_FOUND)

include_directories(${SERVER_INCLUDE})
add_executable(snapserver ${SERVER_SOURCES})
target_link_libraries(snapserver ${SERVER_LIBRARIES})

install(TARGETS snapserver COMPONENT server DESTINATION "${CMAKE_INSTALL_BINDIR}")
