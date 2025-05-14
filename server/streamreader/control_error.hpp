/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#pragma once


// standard headers
#include <system_error>


// https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/motivation/plug_error_code.html
// https://akrzemi1.wordpress.com/examples/error_code-example/
// https://breese.github.io/2017/05/12/customizing-error-codes.html
// http://blog.think-async.com/2010/04/system-error-support-in-c0x-part-5.html


enum class ControlErrc
{
    success = 0,

    // Stream can not be controlled
    can_not_control = 1,

    // Stream property can_go_next is false
    can_go_next_is_false = 2,
    // Stream property can_go_previous is false
    can_go_previous_is_false = 3,
    // Stream property can_play is false
    can_play_is_false = 4,
    // Stream property can_pause is false
    can_pause_is_false = 5,
    // Stream property can_seek is false
    can_seek_is_false = 6,
    // Stream property can_control is false
    can_control_is_false = 7,

    // Invalid JSON was received by the server. An error occurred on the server while parsing the JSON text.
    parse_error = -32700,
    // The JSON sent is not a valid Request object.
    invalid_request = -32600,
    // The method does not exist / is not available.
    method_not_found = -32601,
    // Invalid method parameter(s).
    invalid_params = -32602,
    // Internal JSON-RPC error.
    internal_error = -32603
};

namespace snapcast::error::control
{
const std::error_category& category();
}



namespace std
{
template <>
struct is_error_code_enum<ControlErrc> : public std::true_type
{
};
} // namespace std

std::error_code make_error_code(ControlErrc);
