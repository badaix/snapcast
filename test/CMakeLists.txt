# Prepare "Catch" library for other executables
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR} ${CMAKE_SOURCE_DIR})

# Make test executable
set(TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp ${CMAKE_SOURCE_DIR}/server/streamreader/stream_uri.cpp)
add_executable(snapcast_test ${TEST_SOURCES})
target_link_libraries(snapcast_test Catch)

