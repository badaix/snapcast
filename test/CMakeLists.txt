find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found, fetching it")
  include(FetchContent)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0 # or a later release
  )

  FetchContent_MakeAvailable(Catch2)
endif()

# Make test executable
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_SOURCE_DIR}/common/stream_uri.cpp
    ${CMAKE_SOURCE_DIR}/common/base64.cpp
    ${CMAKE_SOURCE_DIR}/common/utils/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/common/utils/file_utils.cpp
    ${CMAKE_SOURCE_DIR}/server/authinfo.cpp
    # ${CMAKE_SOURCE_DIR}/server/jwt.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/control_error.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/properties.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/metadata.cpp)

find_package(OpenSSL REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_executable(snapcast_test ${TEST_SOURCES})

if(ANDROID)
  target_link_libraries(snapcast_test log)
endif(ANDROID)

target_link_libraries(snapcast_test OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(snapcast_test Catch2::Catch2WithMain Catch2::Catch2)

# GNU linker wrappers inject ALSA errors around the real null PCM device.
if(BUILD_CLIENT AND ALSA_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(alsa_player_test alsa_player_test.cpp
    ${CMAKE_SOURCE_DIR}/client/player/alsa_player.cpp
    ${CMAKE_SOURCE_DIR}/client/player/player.cpp
    ${CMAKE_SOURCE_DIR}/client/stream.cpp
    ${CMAKE_SOURCE_DIR}/client/time_provider.cpp)
  target_include_directories(alsa_player_test PRIVATE ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/client ${ALSA_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
  target_link_libraries(alsa_player_test PRIVATE common ${ALSA_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} ${ATOMIC_LIBRARY} Catch2::Catch2WithMain)
  target_link_options(alsa_player_test PRIVATE
    -Wl,--wrap=snd_pcm_open -Wl,--wrap=snd_pcm_close
    -Wl,--wrap=snd_pcm_prepare -Wl,--wrap=snd_pcm_wait
    -Wl,--wrap=snd_pcm_writei -Wl,--wrap=snd_pcm_hw_params
    -Wl,--wrap=snd_pcm_hw_params_set_format)
endif()
