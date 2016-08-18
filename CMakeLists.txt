cmake_minimum_required(VERSION 3.1)

include_directories(${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-DVERSION="FOOBAR")

find_package(Threads)
find_package(FLAC REQUIRED)
find_package(Vorbis REQUIRED)
find_package(Ogg REQUIRED)

if(NOT WIN32)
  find_package(Avahi REQUIRED)
  add_subdirectory(server)
endif()

add_subdirectory(common)
add_subdirectory(message)
add_subdirectory(client)
