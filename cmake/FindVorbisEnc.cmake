# - Try to find vorbisenc
# Once done this will define
#
# VORBISENC_FOUND - system has libvorbisenc
# VORBISENC_INCLUDE_DIRS - the libvorbisenc include directory
# VORBISENC_LIBRARIES - The libvorbisenc libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules (VORBISENC vorbisenc)
  list(APPEND VORBISENC_INCLUDE_DIRS ${VORBISENC_INCLUDEDIR})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vorbis DEFAULT_MSG VORBISENC_INCLUDE_DIRS VORBISENC_LIBRARIES)

mark_as_advanced(VORBISENC_INCLUDE_DIRS VORBISENC_LIBRARIES)
