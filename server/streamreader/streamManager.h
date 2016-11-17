#ifndef PCM_READER_FACTORY_H
#define PCM_READER_FACTORY_H

#include <string>
#include <vector>
#include <memory>
#include "pcmStream.h"

typedef std::shared_ptr<PcmStream> PcmStreamPtr;

class StreamManager
{
public:
	StreamManager(PcmListener* pcmListener, const std::string& defaultSampleFormat, const std::string& defaultCodec, size_t defaultReadBufferMs = 20);

	PcmStreamPtr addStream(const std::string& uri);
	void start();
	void stop();
	const std::vector<PcmStreamPtr>& getStreams();
	const PcmStreamPtr getDefaultStream();
	const PcmStreamPtr getStream(const std::string& id);
	json toJson() const;

private:
	std::vector<PcmStreamPtr> streams_;
	PcmListener* pcmListener_;
	std::string sampleFormat_;
	std::string codec_;
	size_t readBufferMs_;
};


#endif
