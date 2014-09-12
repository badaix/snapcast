#include "streamClient.h"


StreamClient::StreamClient(MessageReceiver* _receiver, const std::string& _ip, size_t _port) : ClientConnection(_receiver, _ip, _port)
{
}


StreamClient::~StreamClient()
{
}





