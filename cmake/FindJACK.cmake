# FindJACK.cmake - Find JACK audio connection kit
# This module finds JACK audio connection kit library
# It also handles PipeWire's JACK implementation on Fedora/RHEL
#
# This module defines:
#  JACK_FOUND - System has JACK
#  JACK_INCLUDE_DIRS - The JACK include directories
#  JACK_LIBRARIES - The libraries needed to use JACK
#  JACK_LIBRARY_DIRS - The directory containing JACK libraries
#  JACK_VERSION - The version of JACK found

find_package(PkgConfig QUIET)

# First try pkg-config with PipeWire paths
if(PKG_CONFIG_FOUND)
  # Save original PKG_CONFIG_PATH
  set(_PKG_CONFIG_PATH_ORIG $ENV{PKG_CONFIG_PATH})
  
  # Add PipeWire JACK paths
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/lib64/pipewire-0.3/jack/pkgconfig:/usr/lib/pipewire-0.3/jack/pkgconfig")
  
  pkg_check_modules(PC_JACK QUIET jack)
  
  # Restore original PKG_CONFIG_PATH
  set(ENV{PKG_CONFIG_PATH} ${_PKG_CONFIG_PATH_ORIG})
endif()

# Find include directory
find_path(JACK_INCLUDE_DIR
  NAMES jack/jack.h
  PATHS
    ${PC_JACK_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
    /opt/local/include
  PATH_SUFFIXES jack
)

# Find library
find_library(JACK_LIBRARY
  NAMES jack
  PATHS
    ${PC_JACK_LIBRARY_DIRS}
    /usr/lib64/pipewire-0.3/jack
    /usr/lib/pipewire-0.3/jack
    /usr/lib64
    /usr/lib
    /usr/local/lib64
    /usr/local/lib
    /opt/local/lib
)

# Get the directory containing the library
if(JACK_LIBRARY)
  get_filename_component(JACK_LIBRARY_DIR ${JACK_LIBRARY} DIRECTORY)
endif()

# Set the results
set(JACK_LIBRARIES ${JACK_LIBRARY})
set(JACK_LIBRARY_DIRS ${JACK_LIBRARY_DIR})
set(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})

# Version detection
if(PC_JACK_VERSION)
  set(JACK_VERSION ${PC_JACK_VERSION})
else()
  # Try to extract version from jack/jack.h if pkg-config didn't provide it
  if(JACK_INCLUDE_DIR AND EXISTS "${JACK_INCLUDE_DIR}/jack/jack.h")
    file(STRINGS "${JACK_INCLUDE_DIR}/jack/jack.h" jack_version_str
         REGEX "^#define[\t ]+JACK_VERSION[\t ]+\".*\"")
    if(jack_version_str)
      string(REGEX REPLACE "^#define[\t ]+JACK_VERSION[\t ]+\"([^\"]*)\".*" "\\1"
             JACK_VERSION "${jack_version_str}")
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JACK
  REQUIRED_VARS JACK_LIBRARY JACK_INCLUDE_DIR
  VERSION_VAR JACK_VERSION
)

if(JACK_FOUND)
  message(STATUS "Found JACK: ${JACK_LIBRARIES} (found version \"${JACK_VERSION}\")")
  message(STATUS "JACK library directory: ${JACK_LIBRARY_DIRS}")
endif()

mark_as_advanced(JACK_INCLUDE_DIR JACK_LIBRARY)
