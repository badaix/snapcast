#ifndef PCM_READER_FACTORY_H
#define PCM_READER_FACTORY_H

#include <string>
#include "pcmReader.h"

class PcmReaderFactory
{
public:
	static PcmReader* createPcmReader(PcmListener* pcmListener, const std::string& uri, const std::string& defaultSampleFormat, const std::string& defaultCodec, size_t defaultReadBufferMs = 20);
};


#endif
