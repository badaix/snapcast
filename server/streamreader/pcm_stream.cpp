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

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"
#include "pcm_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PcmStream";
static constexpr auto SCRIPT_LOG_TAG = "Script";


CtrlScript::CtrlScript(boost::asio::io_context& ioc, const std::string& script) : ioc_(ioc), script_(script)
{
}


CtrlScript::~CtrlScript()
{
    stop();
}


void CtrlScript::start(const std::string& stream_id, const ServerSettings& server_setttings)
{
    pipe_stderr_ = bp::pipe();
    pipe_stdout_ = bp::pipe();
    stringstream params;
    params << " \"--stream=" + stream_id + "\"";
    if (server_setttings.http.enabled)
        params << " --snapcast-port=" << server_setttings.http.port;
    process_ = bp::child(script_ + params.str(), bp::std_out > pipe_stdout_, bp::std_err > pipe_stderr_);
    stream_stdout_ = make_unique<boost::asio::posix::stream_descriptor>(ioc_, pipe_stdout_.native_source());
    stream_stderr_ = make_unique<boost::asio::posix::stream_descriptor>(ioc_, pipe_stderr_.native_source());
    stderrReadLine();
    stdoutReadLine();
}


void CtrlScript::logScript(const std::string& source, std::string line)
{
    if (line.empty())
        return;

    std::ignore = source;
    if (line.back() == '\r')
        line.resize(line.size() - 1);
    auto tmp = utils::string::tolower_copy(line);
    AixLog::Severity severity = AixLog::Severity::info;
    if (tmp.find(" trace") != string::npos)
        severity = AixLog::Severity::trace;
    else if (tmp.find(" debug") != string::npos)
        severity = AixLog::Severity::debug;
    else if (tmp.find(" info") != string::npos)
        severity = AixLog::Severity::info;
    else if (tmp.find(" warning") != string::npos)
        severity = AixLog::Severity::warning;
    else if (tmp.find(" error") != string::npos)
        severity = AixLog::Severity::error;
    else if ((tmp.find(" fatal") != string::npos) || (tmp.find(" critical") != string::npos))
        severity = AixLog::Severity::fatal;
    LOG(severity, SCRIPT_LOG_TAG) << line << "\n";
}


void CtrlScript::stderrReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(
        *stream_stderr_, streambuf_stderr_, delimiter,
        [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Error while reading from stderr: " << ec.message() << "\n";
                return;
            }

            // Extract up to the first delimiter.
            std::string line{buffers_begin(streambuf_stderr_.data()), buffers_begin(streambuf_stderr_.data()) + bytes_transferred - delimiter.length()};
            logScript("stderr", std::move(line));
            streambuf_stderr_.consume(bytes_transferred);
            stderrReadLine();
        });
}


void CtrlScript::stdoutReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(
        *stream_stdout_, streambuf_stdout_, delimiter,
        [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Error while reading from stdout: " << ec.message() << "\n";
                return;
            }

            // Extract up to the first delimiter.
            std::string line{buffers_begin(streambuf_stdout_.data()), buffers_begin(streambuf_stdout_.data()) + bytes_transferred - delimiter.length()};
            logScript("stdout", std::move(line));
            streambuf_stdout_.consume(bytes_transferred);
            stdoutReadLine();
        });
}



void CtrlScript::stop()
{
    if (process_.running())
    {
        ::kill(-process_.native_handle(), SIGINT);
    }
}


PcmStream::PcmStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : active_(false), pcmListeners_{pcmListener}, uri_(uri), chunk_ms_(20), state_(ReaderState::kIdle), ioc_(ioc), server_settings_(server_settings)
{
    encoder::EncoderFactory encoderFactory;
    if (uri_.query.find(kUriCodec) == uri_.query.end())
        throw SnapException("Stream URI must have a codec");
    encoder_ = encoderFactory.createEncoder(uri_.query[kUriCodec]);

    if (uri_.query.find(kUriName) == uri_.query.end())
        throw SnapException("Stream URI must have a name");
    name_ = uri_.query[kUriName];

    if (uri_.query.find(kUriSampleFormat) == uri_.query.end())
        throw SnapException("Stream URI must have a sampleformat");
    sampleFormat_ = SampleFormat(uri_.query[kUriSampleFormat]);
    LOG(INFO, LOG_TAG) << "PcmStream: " << name_ << ", sampleFormat: " << sampleFormat_.toString() << "\n";

    if (uri_.query.find(kControlScript) != uri_.query.end())
        ctrl_script_ = std::make_unique<CtrlScript>(ioc, uri_.query[kControlScript]);

    if (uri_.query.find(kUriChunkMs) != uri_.query.end())
        chunk_ms_ = cpt::stoul(uri_.query[kUriChunkMs]);

    setMeta(json());
}


PcmStream::~PcmStream()
{
    stop();
}


std::shared_ptr<msg::CodecHeader> PcmStream::getHeader()
{
    return encoder_->getHeader();
}


const StreamUri& PcmStream::getUri() const
{
    return uri_;
}


const std::string& PcmStream::getName() const
{
    return name_;
}


const std::string& PcmStream::getId() const
{
    return getName();
}


const SampleFormat& PcmStream::getSampleFormat() const
{
    return sampleFormat_;
}


std::string PcmStream::getCodec() const
{
    return encoder_->name();
}


void PcmStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start: " << name_ << ", type: " << uri_.scheme << ", sampleformat: " << sampleFormat_.toString() << ", codec: " << getCodec()
                        << "\n";
    encoder_->init([this](const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration) { chunkEncoded(encoder, chunk, duration); },
                   sampleFormat_);
    active_ = true;

    if (ctrl_script_)
        ctrl_script_->start(getId(), server_settings_);
}


void PcmStream::stop()
{
    active_ = false;
}


ReaderState PcmStream::getState() const
{
    return state_;
}


void PcmStream::setState(ReaderState newState)
{
    if (newState != state_)
    {
        LOG(INFO, LOG_TAG) << "State changed: " << name_ << ", state: " << state_ << " => " << newState << "\n";
        state_ = newState;
        for (auto* listener : pcmListeners_)
        {
            if (listener != nullptr)
                listener->onStateChanged(this, newState);
        }
    }
}


void PcmStream::chunkEncoded(const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration)
{
    std::ignore = encoder;
    // LOG(TRACE, LOG_TAG) << "onChunkEncoded: " << getName() << ", duration: " << duration << " ms, compression ratio: " << 100 - ceil(100 *
    // (chunk->durationMs() / duration)) << "%\n";
    if (duration <= 0)
        return;

    // absolute start timestamp is the tvEncodedChunk_
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(tvEncodedChunk_.time_since_epoch()).count();
    chunk->timestamp.sec = microsecs / 1000000;
    chunk->timestamp.usec = microsecs % 1000000;

    // update tvEncodedChunk_ to the next chunk start by adding the current chunk duration
    tvEncodedChunk_ += std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(duration * 1000000));
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onChunkEncoded(this, chunk, duration);
    }
}


void PcmStream::chunkRead(const msg::PcmChunk& chunk)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onChunkRead(this, chunk);
    }
    encoder_->encode(chunk);
}


void PcmStream::resync(const std::chrono::nanoseconds& duration)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onResync(this, duration.count() / 1000000.);
    }
}


json PcmStream::toJson() const
{
    json j = {
        {"uri", uri_.toJson()},
        {"id", getId()},
        {"status", to_string(state_)},
    };

    if (meta_)
        j["meta"] = meta_->msg;

    return j;
}


void PcmStream::addListener(PcmListener* pcmListener)
{
    pcmListeners_.push_back(pcmListener);
}


std::shared_ptr<msg::StreamTags> PcmStream::getMeta() const
{
    return meta_;
}


void PcmStream::setMeta(const json& jtag)
{
    meta_.reset(new msg::StreamTags(jtag));
    meta_->msg["STREAM"] = name_;
    LOG(INFO, LOG_TAG) << "Stream: " << name_ << ", metadata=" << meta_->msg.dump(4) << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onMetaChanged(this);
    }
}

} // namespace streamreader
