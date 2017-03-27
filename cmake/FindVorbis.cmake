#
# Try to find Vorbis libraries and include paths.
# Once done this will define
#
# VORBIS_FOUND
# VORBIS_INCLUDE_DIRS
# VORBIS_LIBRARIES
#

find_path(VORBIS_INCLUDE_DIRS vorbis/codec.h)

find_library(VORBIS_LIBRARIES NAMES vorbis)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VORBIS DEFAULT_MSG VORBIS_LIBRARIES VORBIS_INCLUDE_DIRS)

mark_as_advanced(VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)