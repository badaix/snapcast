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
				json jGroups = j["Groups"];
				for (auto it = jGroups.begin(); it != jGroups.end(); ++it)
				{
					GroupPtr group = make_shared<Group>();
					group->fromJson(*it);
//					if (client->id.empty() || getClientInfo(client->id))
//						continue;
					groups.push_back(group);
				}
			}
		}
	}
	catch(const std::exception& e)
	{
		logE << "Error reading config: " << e.what() << "\n";
	}
}


Config::~Config()
{
	save();
}


void Config::save()
{
	std::ofstream ofs(filename_.c_str(), std::ofstream::out|std::ofstream::trunc);
	json clients = {
		{"ConfigVersion", 2},
		{"Groups", getGroups()}
	};
	ofs << std::setw(4) << clients;
	ofs.close();
}


ClientInfoPtr Config::getClientInfo(const std::string& clientId) const
{
	if (clientId.empty())
		return nullptr;

	for (auto client: clients)
	{
		if (client->id == clientId)
			return client;
	}

	return nullptr;
}


ClientInfoPtr Config::addClientInfo(const std::string& clientId)
{
	ClientInfoPtr client = getClientInfo(clientId);
	if (!client)
	{
		client = make_shared<ClientInfo>(clientId);
		clients.push_back(client);
	}

	return client;
}


GroupPtr Config::getGroup(ClientInfoPtr client)
{
	for (auto group: groups)
	{
		for (auto c: group->clients)
		{
			if (c->id == client->id)
				return group;
		}
	}

	GroupPtr group = std::make_shared<Group>();
	group->id = generateUUID();
	group->clients.push_back(client);
	groups.push_back(group);

	return group;
}


json Config::getServerStatus(const std::string& clientId, const json& streams) const
{
	json jClient = json::array();
	if (clientId != "")
	{
		ClientInfoPtr client = getClientInfo(clientId);
		if (client)
			jClient += client->toJson();
	}
	else
		jClient = getClientInfos();

	Host host;
	host.update();
	//TODO: Set MAC and IP
	Snapserver snapserver("Snapserver", VERSION);
	json serverStatus = {
		{"server", {
			{"host", host.toJson()},//getHostName()},
			{"snapserver", snapserver.toJson()}
		}},
		{"clients", jClient},
		{"streams", streams}
	};

	return serverStatus;
}


json Config::getClientInfos() const
{
	json result = json::array();
	for (auto client: clients)
		result.push_back(client->toJson());
	return result;
}


json Config::getGroups() const
{
	json result = json::array();
	for (auto group: groups)
		result.push_back(group->toJson());
	return result;
}


void Config::remove(ClientInfoPtr client)
{
	auto group = getGroup(client);
	if (group->clients.size() == 1)
		groups.erase(std::remove(groups.begin(), groups.end(), group), groups.end());

	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

