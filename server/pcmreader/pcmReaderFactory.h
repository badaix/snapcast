#ifndef PCM_READER_FACTORY_H
#define PCM_READER_FACTORY_H

#include <string>
#include "pcmReader.h"

class PcmReaderFactory
{
public:
	PcmReader* createPcmReader(const std::string& uri) const;
};


#endif
