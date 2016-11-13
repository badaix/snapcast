# - Try to find vorbis
# Once done this will define
#
# VORBIS_FOUND - system has libvorbis
# VORBIS_INCLUDE_DIRS - the libvorbis include directory
# VORBIS_LIBRARIES - The libvorbis libraries

if (UNIX AND NOT ANDROID)
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules (VORBIS vorbis)
    list(APPEND VORBIS_INCLUDE_DIRS ${VORBIS_INCLUDEDIR})
  endif()
endif()

if(NOT VORBIS_FOUND)
  find_path(VORBIS_INCLUDE_DIRS vorbis/codec.h
    PATHS ${CMAKE_SOURCE_DIR}/externals/vorbis/include)
  find_library(VORBIS_LIBRARIES libvorbis
    PATHS ${CMAKE_SOURCE_DIR}/externals/vorbis/win32/VS2010/x64/Release)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vorbis DEFAULT_MSG VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)

mark_as_advanced(VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)
