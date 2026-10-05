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

# Exercise the actual TCP source lifecycle with loopback sockets.
if(BUILD_SERVER AND NOT ANDROID)
  get_target_property(TCP_TEST_LIBRARIES snapserver LINK_LIBRARIES)
  get_target_property(TCP_TEST_LINK_DIRS snapserver LINK_DIRECTORIES)
  get_target_property(TCP_TEST_INCLUDES snapserver INCLUDE_DIRECTORIES)
  get_target_property(TCP_TEST_SERVER_SOURCES snapserver SOURCES)
  set(TCP_TEST_SOURCES tcp_stream_test.cpp)
  foreach(source IN LISTS TCP_TEST_SERVER_SOURCES)
    if(source MATCHES "^encoder/" OR
       source MATCHES "^streamreader/(tcp_stream|pcm_stream|stream_control|control_error|properties|metadata)\\.cpp$")
      list(APPEND TCP_TEST_SOURCES "${CMAKE_SOURCE_DIR}/server/${source}")
    endif()
  endforeach()
  add_executable(tcp_stream_test ${TCP_TEST_SOURCES})
  target_include_directories(tcp_stream_test PRIVATE ${TCP_TEST_INCLUDES})
  if(TCP_TEST_LINK_DIRS)
    target_link_directories(tcp_stream_test PRIVATE ${TCP_TEST_LINK_DIRS})
  endif()
  target_link_libraries(tcp_stream_test PRIVATE ${TCP_TEST_LIBRARIES} Catch2::Catch2WithMain)
endif()
