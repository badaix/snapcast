#
# Try to find Vorbis libraries and include paths.
# Once done this will define
#
# VORBIS_FOUND
# VORBIS_INCLUDE_DIRS
# VORBIS_LIBRARIES
#

find_path(VORBIS_INCLUDE_DIRS vorbis/codec.h HINTS ${CMAKE_SOURCE_DIR}/externals/vorbis/include)

find_library(VORBIS_LIBRARIES NAMES vorbis libvorbis HINTS ${CMAKE_SOURCE_DIR}/externals/vorbis/win32/VS2010/x64/Release ${CMAKE_SOURCE_DIR}/externals/vorbis/win32/VS2010/win32/Release)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VORBIS DEFAULT_MSG VORBIS_LIBRARIES VORBIS_INCLUDE_DIRS)

mark_as_advanced(VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)