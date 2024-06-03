/***
    This file is part of snapcast
    Copyright (C) 2017-2017  https://github.com/frafall

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

#include <string>

std::string base64_encode(const unsigned char* bytes_to_encode, size_t in_len);
std::string base64_encode(const std::string& text);
std::string base64_decode(const std::string& encoded_string);

std::string base64url_encode(const unsigned char* bytes_to_encode, size_t in_len);
std::string base64url_encode(const std::string& text);
std::string base64url_decode(const std::string& encoded_string);
