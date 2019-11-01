/***
        This file is part of snapcast
        Copyright (C) 2015  Hannes Ellinger

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

#include "opus_decoder.hpp"
#include "common/aixlog.hpp"


OpusDecoderWrapper::OpusDecoderWrapper() : Decoder()
{
    int error;
    dec = opus_decoder_create(48000, 2, &error); // fixme
    if (error != 0)
    {
        LOG(ERROR) << "Failed to initialize opus decoder: " << error << '\n' << " Rate:     " << 48000 << '\n' << " Channels: " << 2 << '\n';
    }
}


OpusDecoderWrapper::~OpusDecoderWrapper()
{
}


bool OpusDecoderWrapper::decode(msg::PcmChunk* /*chunk*/)
{
    // int samples = 480;
    // // reserve space for decoded audio
    // opus_int16 pcm[samples * 2];

    // msg::PcmChunk* decodedChunk = new msg::PcmChunk();
    // decodedChunk->payloadSize = samples * 4;
    // decodedChunk->payload = (char*)realloc(decodedChunk->payload, decodedChunk->payloadSize);

    // // decode
    // int frame_size = opus_decode(dec, (unsigned char*)chunk->payload, chunk->payloadSize, pcm, samples, 0);
    // if (frame_size < 0)
    // {
    //     LOG(ERROR) << "Failed to decode chunk: " << frame_size << '\n'
    //                << " IN size:  " << chunk->payloadSize << '\n'
    //                << " OUT size: " << decodedChunk->payloadSize << '\n';
    // }
    // else
    // {
    //     // logD << "Decoded chunk: size " << chunk->payloadSize << " bytes, decoded " << frame_size << " bytes" << '\n';

    //     // copy encoded data to chunk
    //     chunk->payloadSize = samples * 4;
    //     chunk->payload = (char*)realloc(chunk->payload, chunk->payloadSize);

    //     // uninterleave audio
    //     for (int i = 0; i < samples * 2; i++)
    //     {
    //         chunk->payload[2 * i + 1] = (char)(pcm[i] >> 8);
    //         chunk->payload[2 * i] = (char)(pcm[i] & 0x00ff);
    //     }
    // }

    return true;
}


SampleFormat OpusDecoderWrapper::setHeader(msg::CodecHeader* /*chunk*/)
{
    return {48000, 16, 2};
}
