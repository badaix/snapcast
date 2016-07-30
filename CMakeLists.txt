cmake_minimum_required(VERSION 3.1)
project(snapcast)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="FOOBAR")

if(NOT WIN32)
  add_subdirectory(server)
else()
endif()

add_subdirectory(client)
