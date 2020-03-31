# - Try to find Opus
# Once done this will define
#
# OPUS_FOUND - system has opus
# OPUS_INCLUDE_DIRS - the opus include directory
# OPUS_LIBRARIES - The opus libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules (OPUS opus)
  list(APPEND OPUS_INCLUDE_DIRS ${OPUS_INCLUDEDIR})
endif()

if(NOT OPUS_FOUND)
  find_path(OPUS_INCLUDE_DIRS opus/opus.h)
  find_library(OPUS_LIBRARIES opus)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus DEFAULT_MSG OPUS_INCLUDE_DIRS OPUS_LIBRARIES)

mark_as_advanced(OPUS_INCLUDE_DIRS OPUS_LIBRARIES)