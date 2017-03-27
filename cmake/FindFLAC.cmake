#
# Try to find FLAC libraries and include paths.
# Once done this will define
#
# FLAC_FOUND
# FLAC_INCLUDE_DIRS
# FLAC_LIBRARIES
#

find_path(FLAC_INCLUDE_DIRS FLAC/all.h)
find_path(FLAC_INCLUDE_DIRS FLAC/stream_decoder.h)

find_library(FLAC_LIBRARIES NAMES FLAC)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FLAC DEFAULT_MSG FLAC_LIBRARIES FLAC_INCLUDE_DIRS)

mark_as_advanced(FLAC_INCLUDE_DIRS FLAC_LIBRARIES)