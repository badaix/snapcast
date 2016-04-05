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

#ifndef ENCODER_H
#define ENCODER_H

#include <string>
#include <memory>

#include "message/pcmChunk.h"
#include "message/codecHeader.h"
#include "common/sampleFormat.h"


class Encoder;

/// Callback interface for users of Encoder
/**
 * Users of Encoder should implement this to get the encoded PCM data
 */
class EncoderListener
{
public:
	virtual void onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration) = 0;
};



/// Abstract Encoder class
/**
 * Stream encoder. PCM chunks are fed into the encoder.
 * As soon as a frame is encoded, the encoded data is passed to the EncoderListener
 */
class Encoder
{
public:
	/// ctor. Codec options (E.g. compression level) are passed as string and are codec dependend
	Encoder(const std::string& codecOptions = "") : headerChunk_(NULL), codecOptions_(codecOptions)
	{
	}

	virtual ~Encoder()
	{
	}

	/// The listener will receive the encoded stream
	virtual void init(EncoderListener* listener, const SampleFormat& format)
	{
		if (codecOptions_ == "")
			codecOptions_ = getDefaultOptions();
		listener_ = listener;
		sampleFormat_ = format;
		initEncoder();
	}

	/// Here the work is done. Encoded data is passed to the EncoderListener.
	virtual void encode(const msg::PcmChunk* chunk) = 0;

	virtual std::string name() const = 0;

	virtual std::string getAvailableOptions() const
	{
		return "No codec options supported";
	}

	virtual std::string getDefaultOptions() const
	{
		return "";
	}

	/// Header information needed to decode the data
	virtual std::shared_ptr<msg::CodecHeader> getHeader() const
	{
		return headerChunk_;
	}

protected:
	virtual void initEncoder() = 0;

	SampleFormat sampleFormat_;
	std::shared_ptr<msg::CodecHeader> headerChunk_;
	EncoderListener* listener_;
	std::string codecOptions_;
};


#endif


