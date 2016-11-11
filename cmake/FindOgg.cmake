# - Try to find ogg
# Once done this will define
#
# OGG_FOUND - system has ogg
# OGG_INCLUDE_DIRS - the ogg include directory
# OGG_LIBRARIES - The ogg libraries

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig)
endif()
if(PKG_CONFIG_FOUND)
  pkg_check_modules (OGG ogg)
  list(APPEND OGG_INCLUDE_DIRS ${OGG_INCLUDEDIR})
endif()

if(NOT OGG_FOUND)
  find_path(OGG_INCLUDE_DIRS ogg/ogg.h
    PATHS ${CMAKE_SOURCE_DIR}/externals/ogg/include)
  find_library(OGG_LIBRARIES libogg
    PATHS ${CMAKE_SOURCE_DIR}/externals/ogg/win32/VS2015/x64/Release)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ogg DEFAULT_MSG OGG_INCLUDE_DIRS OGG_LIBRARIES)

mark_as_advanced(OGG_INCLUDE_DIRS OGG_LIBRARIES)
