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

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <cerrno>
#include "common/snapException.h"
#include "common/strCompat.h"
#include "common/log.h"

using namespace std;


Config::Config()
{
	string dir;
	if (getenv("HOME") == NULL)
		dir = "/var/lib/snapcast/";
	else
		dir = getenv("HOME") + string("/.config/snapcast/");
	int status = mkdirRecursive(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if ((status != 0) && (errno != EEXIST))
		throw SnapException("failed to create settings directory: \"" + dir + "\": " + cpt::to_string(errno));

	filename_ = dir + "server.json";
	logO << "Settings file: " << filename_ << "\n";

	try
	{
		ifstream ifs(filename_, std::ifstream::in);
		if (ifs.good())
		{
			json j;
			ifs >> j;
			if (j.count("ConfigVersion"))
			{
				json jClient = j["Client"];
				for (json::iterator it = jClient.begin(); it != jClient.end(); ++it)
				{
					ClientInfoPtr client = make_shared<ClientInfo>();
					client->fromJson(*it);
					if (client->host.mac.empty())
						continue;
					for (const auto& c: clients)
					{
						if (c->host.mac == client->host.mac)
							continue;
					}

					client->connected = false;
					clients.push_back(client);
				}
			}
		}
	}
	catch(const std::exception& e)
	{
		logE << "Error reading config: " << e.what() << "\n";
	}
}


void Config::save()
{
	std::ofstream ofs(filename_.c_str(), std::ofstream::out|std::ofstream::trunc);
	json clients = {
		{"ConfigVersion", 1},
		{"Client", getClientInfos()}
	};
	ofs << std::setw(4) << clients;
	ofs.close();
}


ClientInfoPtr Config::getClientInfo(const std::string& macAddress, bool add)
{
	if (macAddress.empty())
		return nullptr;

	for (auto client: clients)
	{
		if (client->host.mac == macAddress)
			return client;
	}

	if (!add)
		return nullptr;

	ClientInfoPtr client = make_shared<ClientInfo>(macAddress);
	clients.push_back(client);

	return client;
}


json Config::getClientInfos() const
{
	json result = json::array();
	for (auto client: clients)
		result.push_back(client->toJson());
	return result;
}


void Config::remove(ClientInfoPtr client)
{
	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

