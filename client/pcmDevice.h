#ifndef PCM_DEVICE_H
#define PCM_DEVICE_H

#include <string>


class PcmDevice
{
public:
	PcmDevice() : idx(-1){};
	int idx;
	std::string name;
	std::string description;
};


#endif

