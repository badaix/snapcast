/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#include "control_error.hpp"

namespace snapcast::error::control
{

namespace detail
{

struct category : public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const override;
};


const char* category::name() const noexcept
{
    return "control";
}

std::string category::message(int value) const
{
    switch (static_cast<ControlErrc>(value))
    {
        case ControlErrc::success:
            return "Success";
        case ControlErrc::can_not_control:
            return "Stream can not be controlled";
        case ControlErrc::can_go_next_is_false:
            return "Stream property canGoNext is false";
        case ControlErrc::can_go_previous_is_false:
            return "Stream property canGoPrevious is false";
        case ControlErrc::can_play_is_false:
            return "Stream property canPlay is false";
        case ControlErrc::can_pause_is_false:
            return "Stream property canPause is false";
        case ControlErrc::can_seek_is_false:
            return "Stream property canSeek is false";
        case ControlErrc::can_control_is_false:
            return "Stream property canControl is false";
        case ControlErrc::parse_error:
            return "Parse error";
        case ControlErrc::invalid_request:
            return "Invalid request";
        case ControlErrc::method_not_found:
            return "Method not found";
        case ControlErrc::invalid_params:
            return "Invalid params";
        case ControlErrc::internal_error:
            return "Internal error";
        default:
            return "Unknown";
    }
}

} // namespace detail

const std::error_category& category()
{
    // The category singleton
    static detail::category instance;
    return instance;
}

} // namespace snapcast::error::control

std::error_code make_error_code(ControlErrc errc)
{
    // Create an error_code with the original mpg123 error value
    // and the mpg123 error category.
    return std::error_code(static_cast<int>(errc), snapcast::error::control::category());
}
