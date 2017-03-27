#
# Try to find Ogg libraries and include paths.
# Once done this will define
#
# OGG_FOUND
# OGG_INCLUDE_DIRS
# OGG_LIBRARIES
#

find_path(OGG_INCLUDE_DIRS ogg/ogg.h)

find_library(OGG_LIBRARIES NAMES ogg)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OGG DEFAULT_MSG OGG_LIBRARIES OGG_INCLUDE_DIRS)

mark_as_advanced(OGG_INCLUDE_DIRS OGG_LIBRARIES)