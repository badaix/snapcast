/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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


#ifndef PUBLISH_BONJOUR_H
#define PUBLISH_BONJOUR_H

#include <string>
#include <dns_sd.h>

class PublishBonjour;

#include "publishmDNS.h"

class PublishBonjour : public PublishmDNS
{
public:
	PublishBonjour(const std::string& serviceName);
	virtual ~PublishBonjour();
	virtual void publish(const std::vector<mDNSService>& services);

private:
	std::thread pollThread_;
	void worker();
	std::atomic<bool> active_;
    std::vector<DNSServiceRef> clients;
};


#endif


