# - Try to find vorbis
# Once done this will define
#
# VORBIS_FOUND - system has libvorbis
# VORBIS_INCLUDE_DIRS - the libvorbis include directory
# VORBIS_LIBRARIES - The libvorbis libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules (VORBIS vorbis)
  list(APPEND VORBIS_INCLUDE_DIRS ${VORBIS_INCLUDEDIR})
endif()

if(NOT VORBIS_FOUND)
  find_path(VORBIS_INCLUDE_DIRS vorbis/vorbisenc.h)
  find_library(VORBIS_LIBRARIES vorbis)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vorbis DEFAULT_MSG VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)

mark_as_advanced(VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)
