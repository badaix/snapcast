#ifndef PCM_DEVICE_H
#define PCM_DEVICE_H

#include <string>


struct PcmDevice
{
	PcmDevice() : idx(-1){};
	int idx;
	std::string name;
	std::string description;
};


#endif

