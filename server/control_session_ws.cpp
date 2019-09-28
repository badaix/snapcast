/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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

#include "control_session_ws.hpp"
#include "aixlog.hpp"
#include "message/pcmChunk.h"
#include <iostream>
#include <mutex>

using namespace std;



ControlSessionWs::ControlSessionWs(ControlMessageReceiver* receiver, tcp::socket&& socket) : ControlSession(receiver, std::move(socket))
{
}


ControlSessionWs::~ControlSessionWs()
{
    LOG(DEBUG) << "ControlSessionWs::~ControlSessionWs()\n";
    stop();
}

void ControlSessionWs::start()
{
}


void ControlSessionWs::stop()
{
}


void ControlSessionWs::sendAsync(const std::string& message)
{
}


bool ControlSessionWs::send(const std::string& message)
{
    return true;
}
