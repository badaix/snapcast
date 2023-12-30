/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#include "resampler.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"

#include <cmath>

using namespace std;

static constexpr auto LOG_TAG = "Resampler";

Resampler::Resampler(const SampleFormat& in_format, const SampleFormat& out_format) : in_format_(in_format), out_format_(out_format)
{
#ifdef HAS_SOXR
    if ((out_format_.rate() != in_format_.rate()) || (out_format_.bits() != in_format_.bits()))
    {
        LOG(INFO, LOG_TAG) << "Resampling from " << in_format_.toString() << " to " << out_format_.toString() << "\n";
        soxr_error_t error;

        soxr_datatype_t in_type = SOXR_INT16_I;
        soxr_datatype_t out_type = SOXR_INT16_I;
        if (in_format_.sampleSize() > 2)
            in_type = SOXR_INT32_I;
        if (out_format_.sampleSize() > 2)
            out_type = SOXR_INT32_I;
        soxr_io_spec_t iospec = soxr_io_spec(in_type, out_type);
        // HQ should be fine: http://sox.sourceforge.net/Docs/FAQ
        soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
        soxr_ = soxr_create(static_cast<double>(in_format_.rate()), static_cast<double>(out_format_.rate()), in_format_.channels(), &error, &iospec, &q_spec,
                            nullptr);
        if (error != nullptr)
        {
            LOG(ERROR, LOG_TAG) << "Error soxr_create: " << error << "\n";
            soxr_ = nullptr;
        }
        // initialize the buffer with 20ms (~latency of the reampler)
        resample_buffer_.resize(out_format_.frameSize() * static_cast<uint16_t>(ceil(out_format_.msRate() * 20)));
    }
#else
    LOG(WARNING, LOG_TAG) << "Soxr not available, resampling not supported\n";
    if ((out_format_.rate() != in_format_.rate()) || (out_format_.bits() != in_format_.bits()))
    {
        throw SnapException("Resampling requested, but not supported");
    }
#endif
    // resampled_chunk_ = std::make_unique<msg::PcmChunk>(out_format_, 0);
}


bool Resampler::resamplingNeeded() const
{
#ifdef HAS_SOXR
    return soxr_ != nullptr;
#else
    return false;
#endif
}


// std::shared_ptr<msg::PcmChunk> Resampler::resample(std::shared_ptr<msg::PcmChunk> chunk, chronos::usec duration)
// {
//     auto resampled_chunk = resample(chunk);
//     if (!resampled_chunk)
//         return nullptr;
// std::cerr << "1\n";
//     resampled_chunk_->append(*resampled_chunk);
// std::cerr << "2\n";
//     while (resampled_chunk_->duration<chronos::usec>() >= duration)
//     {
//         LOG(DEBUG, LOG_TAG) << "resampled duration: " << resampled_chunk_->durationMs() << ", consuming: " << out_format_.usRate() * duration.count() <<
//         "\n";
//         auto chunk = resampled_chunk_->consume(out_format_.usRate() * duration.count());
//         LOG(DEBUG, LOG_TAG) << "consumed: " << chunk->durationMs() << ", resampled duration: " << resampled_chunk_->durationMs() << "\n";
//         return chunk;
//     }
//     // resampled_chunks_.push_back(resampled_chunk);
//     // chronos::usec avail;
//     // for (const auto& chunk: resampled_chunks_)
//     // {
//     //     avail += chunk->durationLeft<chronos::usec>();
//     //     if (avail >= duration)
//     //     {

//     //     }
//     // }
// }


std::shared_ptr<msg::PcmChunk> Resampler::resample(const msg::PcmChunk& chunk)
{
#ifndef HAS_SOXR
    return std::make_shared<msg::PcmChunk>(chunk);
#else
    if (!resamplingNeeded())
    {
        return std::make_shared<msg::PcmChunk>(chunk);
    }
    else
    {
        if (in_format_.bits() == 24)
        {
            // sox expects 32 bit input, shift 8 bits left
            auto* frames = reinterpret_cast<int32_t*>(chunk.payload);
            for (size_t n = 0; n < chunk.getSampleCount(); ++n)
                frames[n] = frames[n] << 8;
        }

        size_t idone;
        size_t odone;
        auto resample_buffer_framesize = resample_buffer_.size() / out_format_.frameSize();
        const auto* error = soxr_process(soxr_, chunk.payload, chunk.getFrameCount(), &idone, resample_buffer_.data(), resample_buffer_framesize, &odone);
        if (error != nullptr)
        {
            LOG(ERROR, LOG_TAG) << "Error soxr_process: " << error << "\n";
        }
        else
        {
            LOG(TRACE, LOG_TAG) << "Resample idone: " << idone << "/" << chunk.getFrameCount() << ", odone: " << odone << "/"
                                << resample_buffer_.size() / out_format_.frameSize() << ", delay: " << soxr_delay(soxr_) << "\n";

            // some data has been resampled (odone frames) and some is still in the pipe (soxr_delay frames)
            if (odone > 0)
            {
                // get the resampled ts from the input ts
                auto input_end_ts = chunk.start() + chunk.duration<std::chrono::microseconds>();
                double resampled_ms = (odone + soxr_delay(soxr_)) / out_format_.msRate();
                auto resampled_start = input_end_ts - std::chrono::microseconds(static_cast<int>(resampled_ms * 1000.));

                auto resampled_chunk = std::make_shared<msg::PcmChunk>(out_format_, 0);
                auto us = chrono::duration_cast<chrono::microseconds>(resampled_start.time_since_epoch()).count();
                resampled_chunk->timestamp.sec = static_cast<int32_t>(us / 1000000);
                resampled_chunk->timestamp.usec = static_cast<int32_t>(us % 1000000);

                // copy from the resample_buffer to the resampled chunk
                resampled_chunk->payloadSize = static_cast<uint32_t>(odone * out_format_.frameSize());
                resampled_chunk->payload = static_cast<char*>(realloc(resampled_chunk->payload, resampled_chunk->payloadSize));
                memcpy(resampled_chunk->payload, resample_buffer_.data(), resampled_chunk->payloadSize);

                if (out_format_.bits() == 24)
                {
                    // sox has quantized to 32 bit, shift 8 bits right
                    auto* frames = reinterpret_cast<int32_t*>(resampled_chunk->payload);
                    for (size_t n = 0; n < resampled_chunk->getSampleCount(); ++n)
                    {
                        // +128 to round to the nearest so that quantisation steps are distributed evenly
                        frames[n] = (frames[n] + 128) >> 8;
                        if (frames[n] > 0x7fffffff)
                            frames[n] = 0x7fffffff;
                    }
                }

                // check if the resample_buffer is large enough, or if soxr was using all available space
                if (odone == resample_buffer_framesize)
                {
                    // buffer for resampled data too small, add space for 5ms
                    resample_buffer_.resize(resample_buffer_.size() + out_format_.frameSize() * static_cast<uint16_t>(ceil(out_format_.msRate() * 5)));
                    LOG(DEBUG, LOG_TAG) << "Resample buffer completely filled, adding space for 5ms; new buffer size: " << resample_buffer_.size()
                                        << " bytes\n";
                }

                // //LOG(TRACE, LOG_TAG) << "ts: " << out->timestamp.sec << "s, " << out->timestamp.usec/1000.f << " ms, duration: " << odone / format_.msRate()
                // << "\n";
                // int64_t next_us = us + static_cast<int64_t>(odone / format_.msRate() * 1000);
                // LOG(TRACE, LOG_TAG) << "ts: " << us << ", next: " << next_us << ", diff: " << next_us_ - us << "\n";
                // next_us_ = next_us;

                return resampled_chunk;
            }
        }
    }
    return nullptr;
#endif
}


shared_ptr<msg::PcmChunk> Resampler::resample(shared_ptr<msg::PcmChunk> chunk)
{
#ifndef HAS_SOXR
    return chunk;
#else
    if (!resamplingNeeded())
    {
        return chunk;
    }
    else
    {
        return resample(*chunk);
    }
#endif
}


Resampler::~Resampler()
{
#ifdef HAS_SOXR
    if (soxr_ != nullptr)
        soxr_delete(soxr_);
#endif
}
