# - Try to find Bonjour 
# (See http://developer.apple.com/networking/bonjour/index.html)
# By default available on MacOS X and on Linux via the Avahi package.
# Check for libdns_sd
#
#  BONJOUR_INCLUDE_DIR - where to find dns_sd.h, etc.
#  BONJOUR_LIBRARIES   - List of libraries when using ....
#  BONJOUR_FOUND       - True if Bonjour libraries found.

set(BONJOUR_FOUND FALSE)
set(BONJOUR_LIBRARIES)

message(STATUS "Checking whether Bonjour/Avahi is supported")

# Bonjour is built-in on MacOS X / iOS (i.e. available in libSystem)
if(NOT APPLE)
  IF (WIN32)
    FIND_PATH(BONJOUR_INCLUDE_DIR dns_sd.h
      PATHS $ENV{PROGRAMW6432}/Bonjour\ SDK/Include
    )
    FIND_LIBRARY(BONJOUR_LIBRARY
      NAMES dnssd
      PATHS $ENV{PROGRAMW6432}/Bonjour\ SDK/Lib/x64
    )
  ELSE(WIN32)
    find_path(BONJOUR_INCLUDE_DIR dns_sd.h 
      PATHS /opt/dnssd/include /usr/include  /usr/local/include
    )
    find_library(BONJOUR_LIBRARY 
      NAMES dns_sd
      PATHS /opt/dnssd/lib /usr/lib /usr/local/lib
    )
  ENDIF(WIN32)
  if(NOT BONJOUR_INCLUDE_DIR OR NOT BONJOUR_LIBRARY)
    return()
  else()
    set(BONJOUR_LIBRARIES ${BONJOUR_LIBRARY} )
    set(BONJOUR_FOUND TRUE)
  endif()
else()
  set(BONJOUR_FOUND TRUE)
endif()
if (CMAKE_SYSTEM_NAME MATCHES Linux)
  # The compatibility layer is needed for the Bonjour record management.
  find_path(AVAHI_INCLUDE_DIR avahi-client/client.h 
    PATHS /opt/include /usr/include /usr/local/include
  )
  if(AVAHI_INCLUDE_DIR)
   set(BONJOUR_INCLUDE_DIR ${BONJOUR_INCLUDE_DIR} ${AVAHI_INCLUDE_DIR})
  endif()

  # Also, the library is needed, as in Mac OS X. When found the compat
  # layer, also the other libraries must be in the same location.
  foreach(l client common core) 
    find_library(AVAHI_${l}_LIBRARY 
      NAMES libavahi-${l}.so
      PATHS /opt/lib /usr/lib /usr/local/lib
    )
    if(AVAHI_${l}_LIBRARY)
      set(BONJOUR_LIBRARIES ${BONJOUR_LIBRARIES} ${AVAHI_${l}_LIBRARY})
    endif()
  endforeach()

  if(BONJOUR_INCLUDE_DIR AND BONJOUR_LIBRARIES)
    set(BONJOUR_FOUND TRUE)
  endif()
endif()

mark_as_advanced( FORCE
  BONJOUR_INCLUDE_DIR
  BONJOUR_LIBRARY
)

