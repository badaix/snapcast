/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef PCM_READER_H
#define PCM_READER_H

#include <thread>
#include <atomic>
#include <string>
#include <map>
#include "streamUri.h"
#include "../encoder/encoder.h"
#include "../json/json.hpp"
#include "message/sampleFormat.h"
#include "message/header.h"


class PcmStream;

enum ReaderState
{
	kUnknown = 0,
	kIdle = 1,
	kPlaying = 2,
	kDisabled = 3
};


/// Callback interface for users of PcmStream
/**
 * Users of PcmStream should implement this to get the data
 */
class PcmListener
{
public:
	virtual void onStateChanged(const PcmStream* pcmStream, const ReaderState& state) = 0;
	virtual void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk* chunk, double duration) = 0;
	virtual void onResync(const PcmStream* pcmStream, double ms) = 0;
};


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class PcmStream : public EncoderListener
{
public:
	/// ctor. Encoded PCM data is passed to the PcmListener
	PcmStream(PcmListener* pcmListener, const StreamUri& uri);
	virtual ~PcmStream();

	virtual void start();
	virtual void stop();

	/// Implementation of EncoderListener::onChunkEncoded
	virtual void onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration);
	virtual std::shared_ptr<msg::Header> getHeader();

	virtual const StreamUri& getUri() const;
	virtual const std::string& getName() const;
	virtual const SampleFormat& getSampleFormat() const;

	virtual ReaderState getState() const;
	virtual json toJson() const;


protected:
	virtual void worker() = 0;
	void setState(const ReaderState& newState);

	timeval tvEncodedChunk_;
	std::atomic<bool> active_;
	std::thread readerThread_;
	PcmListener* pcmListener_;
	StreamUri uri_;
	SampleFormat sampleFormat_;
	size_t pcmReadMs_;
	std::unique_ptr<Encoder> encoder_;
	std::string name_;
	ReaderState state_;
};


#endif
