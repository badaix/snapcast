# Prepare "Catch" library for other executables
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_library(Catch INTERFACE)
target_include_directories(
  Catch INTERFACE ${CATCH_INCLUDE_DIR} ${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIR})

set(TEST_LIBRARIES Catch)
if(ANDROID)
  list(APPEND TEST_LIBRARIES log)
endif(ANDROID)

# Make test executable
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/control_error.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/properties.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/metadata.cpp
    ${CMAKE_SOURCE_DIR}/server/streamreader/stream_uri.cpp)

add_executable(snapcast_test ${TEST_SOURCES})
target_link_libraries(snapcast_test ${TEST_LIBRARIES})
