include(CheckCXXSourceCompiles)

macro (CHECK_CXX11_STRING_SUPPORT _RESULT)
    set(CMAKE_REQUIRED_DEFINITIONS -std=c++0x)
    set(_CHECK_CXX11_STRING_SUPPORT_SOURCE_CODE "
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

    CHECK_CXX_SOURCE_COMPILES("${_CHECK_CXX11_STRING_SUPPORT_SOURCE_CODE}" ${_RESULT})
endmacro ()
