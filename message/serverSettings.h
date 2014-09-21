#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

#include "message/message.h"



class ServerSettings : public BaseMessage
{
public:
	ServerSettings(size_t _port = 0) : BaseMessage(message_type::serversettings), port(_port)
	{
	}

	virtual ~ServerSettings()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&port), sizeof(int32_t));
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t);
	}

	int32_t port;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&port), sizeof(int32_t));
	}
};




#endif


