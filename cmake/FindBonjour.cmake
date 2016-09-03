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
    PATHS "C:/Program Files/Bonjour SDK/Include" "C:/Program Files (x86)/Bonjour SDK/Include"
    NO_DEFAULT_PATH)
  find_library(BONJOUR_LIBRARIES dnssd
    PATHS "C:/Program Files (x86)/Bonjour SDK/Lib/x64" "C:/Program Files (x86)/Bonjour SDK/Lib/Win32" "C:/Program Files/Bonjour SDK/Lib/x64" "C:/Program Files/Bonjour SDK/Lib/Win32"
    NO_DEFAULT_PATH)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(bonjour DEFAULT_MSG BONJOUR_INCLUDE_DIRS BONJOUR_LIBRARIES)

mark_as_advanced(BONJOUR_INCLUDE_DIRS BONJOUR_LIBRARIES)
