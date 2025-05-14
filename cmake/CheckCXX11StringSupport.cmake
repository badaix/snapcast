# This file is part of snapcast Copyright (C) 2014-2020  Johannes Pohl

# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.

# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.

# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.

#[=======================================================================[.rst:
CheckCXX11StringSupport
----------------------

Check if the C++ compiler supports std::to_string, std::stoi, ...
See also: https://stackoverflow.com/questions/17950814/how-to-use-stdstoul-and-stdstoull-in-android/18124627#18124627

.. command:: CHECK_CXX11_STRING_SUPPORT

  ::

    CHECK_CXX11_STRING_SUPPORT(resultVar)

  Checks support for std::to_string, std::stoul, std::stoi, std::stod.
  The result will be stored in the internal cache variable specified
  by ``resultVar``, with a boolean true value for success and boolean false for
  failure.
#]=======================================================================]

include(CheckCXXSourceCompiles)

macro(CHECK_CXX11_STRING_SUPPORT _RESULT)
  set(CMAKE_REQUIRED_DEFINITIONS -std=c++0x)
  set(_CHECK_CXX11_STRING_SUPPORT_SOURCE_CODE
      "
#include <string>
int main()
{
    std::to_string(1);
    std::stoul(\"1\");
    std::stoi(\"1\");
    std::stod(\"1\");
    return 0;
}
")

  check_cxx_source_compiles("${_CHECK_CXX11_STRING_SUPPORT_SOURCE_CODE}"
                            ${_RESULT})
endmacro()
