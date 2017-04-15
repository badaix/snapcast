#
# Try to find mDNSResponder libraries and include paths.
# Once done this will define
#
# MDNSRESPONDER_FOUND
# MDNSRESPONDER_INCLUDE_DIRS
# MDNSRESPONDER_LIBRARIES
#

find_path(MDNSRESPONDER_INCLUDE_DIRS dns_sd.h)

find_library(MDNSRESPONDER_LIBRARIES NAMES dnssd)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MDNSRESPONDER DEFAULT_MSG MDNSRESPONDER_LIBRARIES MDNSRESPONDER_INCLUDE_DIRS)

mark_as_advanced(MDNSRESPONDER_INCLUDE_DIRS MDNSRESPONDER_LIBRARIES)