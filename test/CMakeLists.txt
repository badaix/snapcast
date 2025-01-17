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
    ${CMAKE_SOURCE_DIR}/common/jwt.cpp
    ${CMAKE_SOURCE_DIR}/common/base64.cpp
    ${CMAKE_SOURCE_DIR}/common/utils/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/server/authinfo.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/control_error.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/properties.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/metadata.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/stream_uri.cpp)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_executable(snapcast_test ${TEST_SOURCES})

if(ANDROID)
  target_link_libraries(snapcast_test log)
endif(ANDROID)

target_link_libraries(snapcast_test OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(snapcast_test Catch2::Catch2WithMain Catch2::Catch2)
