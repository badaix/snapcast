# - Try to find bonjour
# Once done this will define
#
# BONJOUR_FOUND - system has bonjour
# BONJOUR_INCLUDE_DIRS - the bonjour include directory
# BONJOUR_LIBRARIES - The bonjour libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules (BONJOUR bonjour)
  list(APPEND BONJOUR_INCLUDE_DIRS ${BONJOUR_INCLUDEDIR})
endif()

if(NOT BONJOUR_FOUND)
  find_path(BONJOUR_INCLUDE_DIRS dns_sd.h
    PATHS ${CMAKE_SOURCE_DIR}/externals/mDNSResponder-320.10.80/mDNSShared
    NO_DEFAULT_PATH)
  find_library(BONJOUR_LIBRARIES dns_sd
    PATHS ${CMAKE_SOURCE_DIR}/externals/mDNSResponder-320.10.80/mDNSPosix/build/prod
    NO_DEFAULT_PATH)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(bonjour DEFAULT_MSG BONJOUR_INCLUDE_DIRS BONJOUR_LIBRARIES)

mark_as_advanced(BONJOUR_INCLUDE_DIRS BONJOUR_LIBRARIES)
