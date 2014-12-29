#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

#include "message/message.h"


namespace msg
{

class ServerSettings : public BaseMessage
{
public:
	ServerSettings() : BaseMessage(message_type::kServerSettings)
	{
	}

	virtual ~ServerSettings()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&bufferMs), sizeof(int32_t));
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t);
	}

	int32_t bufferMs;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&bufferMs), sizeof(int32_t));
	}
};

}


#endif


