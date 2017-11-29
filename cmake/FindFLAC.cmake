#
# Try to find FLAC libraries and include paths.
# Once done this will define
#
# FLAC_FOUND
# FLAC_INCLUDE_DIRS
# FLAC_LIBRARIES
#

find_path(FLAC_INCLUDE_DIRS FLAC/all.h HINTS ${CMAKE_SOURCE_DIR}/externals/flac/include)

find_library(FLAC_LIBRARIES NAMES FLAC libflac_dynamic HINTS ${CMAKE_SOURCE_DIR}/externals/flac/objs/x64/Release/lib ${CMAKE_SOURCE_DIR}/externals/flac/objs/win32/Release/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FLAC DEFAULT_MSG FLAC_LIBRARIES FLAC_INCLUDE_DIRS)

mark_as_advanced(FLAC_INCLUDE_DIRS FLAC_LIBRARIES)