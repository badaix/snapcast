set(SOURCES resampler.cpp sample_format.cpp)

if(NOT WIN32 AND NOT ANDROID)
  list(APPEND SOURCES daemon.cpp)
endif()

if(SOXR_FOUND)
  include_directories(${SOXR_INCLUDE_DIRS})
endif(SOXR_FOUND)

add_library(common STATIC ${SOURCES})

if(ANDROID)
  target_link_libraries(common soxr::soxr)
elseif(SOXR_FOUND)
  target_link_libraries(common ${SOXR_LIBRARIES})
endif()
