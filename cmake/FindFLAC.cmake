# - Try to find FLAC
# Once done this will define
#
# FLAC_FOUND - system has libFLAC
# FLAC_INCLUDE_DIRS - the libFLAC include directory
# FLAC_LIBRARIES - The libFLAC libraries

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules (FLAC flac)
    list(APPEND FLAC_INCLUDE_DIRS ${FLAC_INCLUDEDIR})
  endif()
endif()

if(NOT FLAC_FOUND)
  find_path(FLAC_INCLUDE_DIRS NAMES FLAC/all.h
    PATHS ${CMAKE_SOURCE_DIR}/externals/flac/include)
  find_library(FLAC_LIBRARIES NAMES libFLAC_dynamic
    PATHS ${CMAKE_SOURCE_DIR}/externals/flac/objs/x64/Release/lib)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FLAC DEFAULT_MSG FLAC_INCLUDE_DIRS FLAC_LIBRARIES)

mark_as_advanced(FLAC_INCLUDE_DIRS FLAC_LIBRARIES)
