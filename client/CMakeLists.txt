set(CLIENT_SOURCES
    clientConnection.cpp
    controller.cpp
    snapClient.cpp
    stream.cpp
    timeProvider.cpp
    decoder/pcmDecoder.cpp
    player/player.cpp)

set(CLIENT_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} common)

set(CLIENT_INCLUDE
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/common
    ${ASIO_INCLUDE_DIRS}
    ${POPL_INCLUDE_DIRS})


if(MACOSX)
    # Bonjour
    if (BONJOUR_FOUND)
        list(APPEND CLIENT_SOURCES browseZeroConf/browseBonjour.cpp)
    endif (BONJOUR_FOUND)

    # CoreAudio
    add_definitions(-DHAS_COREAUDIO)
    list(APPEND CLIENT_SOURCES player/coreAudioPlayer.cpp)
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    list(APPEND CLIENT_LIBRARIES ${COREAUDIO_LIB} ${COREFOUNDATION_LIB} ${AUDIOTOOLBOX_LIB})
else()
    # Avahi
    if (AVAHI_FOUND)
        list(APPEND CLIENT_SOURCES browseZeroConf/browseAvahi.cpp)
        list(APPEND CLIENT_LIBRARIES ${AVAHI_LIBRARIES})
        list(APPEND CLIENT_INCLUDE ${AVAHI_INCLUDE_DIRS})
    endif (AVAHI_FOUND)

    # ALSA
    if (ALSA_FOUND)
        list(APPEND CLIENT_SOURCES player/alsaPlayer.cpp)
        list(APPEND CLIENT_LIBRARIES ${ALSA_LIBRARIES})
        list(APPEND CLIENT_INCLUDE ${ALSA_INCLUDE_DIRS})
    endif (ALSA_FOUND)
endif (MACOSX)

# if OGG then tremor or vorbis
if (OGG_FOUND)
    list(APPEND CLIENT_SOURCES decoder/oggDecoder.cpp)
    list(APPEND CLIENT_LIBRARIES ${OGG_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${OGG_INCLUDE_DIRS})
endif (OGG_FOUND)

# Tremor (fixed-point) or libvorbis (floating-point)
if (TREMOR_FOUND)
    list(APPEND CLIENT_LIBRARIES ${TREMOR_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${TREMOR_INCLUDE_DIRS})
elseif (VORBIS_FOUND)
    list(APPEND CLIENT_LIBRARIES ${VORBIS_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${VORBIS_INCLUDE_DIRS})
endif (TREMOR_FOUND)

if (FLAC_FOUND)
    list(APPEND CLIENT_SOURCES decoder/flacDecoder.cpp)
    list(APPEND CLIENT_LIBRARIES ${FLAC_LIBRARIES})
    list(APPEND CLIENT_INCLUDE ${FLAC_INCLUDE_DIRS})
endif (FLAC_FOUND)

include_directories(${CLIENT_INCLUDE})
add_executable(snapclient ${CLIENT_SOURCES})
target_link_libraries(snapclient ${CLIENT_LIBRARIES})

install(TARGETS snapclient COMPONENT client DESTINATION "${CMAKE_INSTALL_BINDIR}")
