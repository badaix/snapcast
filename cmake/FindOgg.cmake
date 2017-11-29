#
# Try to find Ogg libraries and include paths.
# Once done this will define
#
# OGG_FOUND
# OGG_INCLUDE_DIRS
# OGG_LIBRARIES
#

find_path(OGG_INCLUDE_DIRS ogg/ogg.h HINTS ${CMAKE_SOURCE_DIR}/externals/ogg/include)

find_library(OGG_LIBRARIES NAMES ogg libogg HINTS ${CMAKE_SOURCE_DIR}/externals/ogg/win32/VS2015/x64/Release ${CMAKE_SOURCE_DIR}/externals/ogg/win32/VS2015/win32/Release)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OGG DEFAULT_MSG OGG_LIBRARIES OGG_INCLUDE_DIRS)

mark_as_advanced(OGG_INCLUDE_DIRS OGG_LIBRARIES)