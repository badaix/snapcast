#ifndef PCM_READER_FACTORY_H
#define PCM_READER_FACTORY_H

#include <string>
#include <vector>
#include <memory>
#include "pcmReader.h"

typedef std::shared_ptr<PcmReader> PcmReaderPtr;

class StreamManager
{
public:
	StreamManager(PcmListener* pcmListener, const std::string& defaultSampleFormat, const std::string& defaultCodec, size_t defaultReadBufferMs = 20);

	PcmReader* addStream(const std::string& uri);
	void start();
	void stop();
	const std::vector<PcmReaderPtr>& getStreams();
	const PcmReaderPtr getDefaultStream();
	const PcmReaderPtr getStream(const std::string& id);
	json toJson() const;

private:
	std::vector<PcmReaderPtr> streams_;
	PcmListener* pcmListener_;
	std::string sampleFormat_;
	std::string codec_;
	size_t readBufferMs_;
};


#endif
