#ifndef COMMAND_MSG_H
#define COMMAND_MSG_H

#include "message.h"
#include <string>


namespace msg
{

class Command : public BaseMessage
{
public:
	Command() : BaseMessage(message_type::kCommand), command("")
	{
	}

	Command(const std::string& _command) : BaseMessage(message_type::kCommand), command(_command)
	{
	}

	virtual ~Command()
	{
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		command.resize(size);
		stream.read(&command[0], size);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int16_t) + command.size();
	}

	std::string command;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		int16_t size(command.size());
		stream.write(reinterpret_cast<char *>(&size), sizeof(int16_t));
		stream.write(command.c_str(), size);
	}
};

}


#endif


