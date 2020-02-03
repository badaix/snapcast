/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "airplay_stream.hpp"
#include "base64.h"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils.hpp"
#include "common/utils/string_utils.hpp"

using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "AirplayStream";

namespace
{
string hex2str(string input)
{
    typedef unsigned char byte;
    unsigned long x = strtoul(input.c_str(), nullptr, 16);
    byte a[] = {byte(x >> 24), byte(x >> 16), byte(x >> 8), byte(x), 0};
    return string((char*)a);
}
} // namespace

/*
 * Expat is used in metadata parsing from Shairport-sync.
 * Without HAS_EXPAT defined no parsing will occur.
 */

AirplayStream::AirplayStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri)
    : ProcessStream(pcmListener, ioc, uri), port_(5000), pipe_open_timer_(ioc)
{
    logStderr_ = true;

    string devicename = uri_.getQuery("devicename", "Snapcast");
    params_wo_port_ = "\"--name=" + devicename + "\" --output=stdout --use-stderr";

    port_ = cpt::stoul(uri_.getQuery("port", "5000"));
    setParamsAndPipePathFromPort();

#ifdef HAS_EXPAT
    createParser();
#else
    LOG(INFO, LOG_TAG) << "Metadata support not enabled (HAS_EXPAT not defined)"
                       << "\n";
#endif
}


AirplayStream::~AirplayStream()
{
#ifdef HAS_EXPAT
    parse(string("</metatags>"));
    XML_ParserFree(parser_);
#endif
}

#ifdef HAS_EXPAT
int AirplayStream::parse(string line)
{
    enum XML_Status result;

    if ((result = XML_Parse(parser_, line.c_str(), line.length(), false)) == XML_STATUS_ERROR)
    {
        XML_ParserFree(parser_);
        createParser();
    }
    return result;
}

void AirplayStream::createParser()
{
    parser_ = XML_ParserCreate("UTF-8");
    XML_SetElementHandler(parser_, element_start, element_end);
    XML_SetCharacterDataHandler(parser_, data);
    XML_SetUserData(parser_, this);

    // Make an outer element to keep parsing going
    parse(string("<metatags>"));
}

void AirplayStream::push()
{
    string data = entry_->data;
    if (entry_->isBase64 && entry_->length > 0)
        data = base64_decode(data);

    if (entry_->type == "ssnc" && entry_->code == "mdst")
        jtag_ = json();

    if (entry_->code == "asal")
        jtag_["ALBUM"] = data;
    if (entry_->code == "asar")
        jtag_["ARTIST"] = data;
    if (entry_->code == "minm")
        jtag_["TITLE"] = data;

    if (entry_->type == "ssnc" && entry_->code == "mden")
    {
        // LOG(INFO, LOG_TAG) << "metadata=" << jtag_.dump(4) << "\n";
        setMeta(jtag_);
    }
}
#endif


void AirplayStream::setParamsAndPipePathFromPort()
{
    pipePath_ = "/tmp/shairmeta." + cpt::to_string(getpid()) + "." + cpt::to_string(port_);
    params_ = params_wo_port_ + " \"--metadata-pipename=" + pipePath_ + "\" --port=" + cpt::to_string(port_);
}


void AirplayStream::do_connect()
{
    ProcessStream::do_connect();
    pipeReadLine();
}


void AirplayStream::pipeReadLine()
{
    if (!pipe_fd_ || !pipe_fd_->is_open())
    {
        try
        {
            int fd = open(pipePath_.c_str(), O_RDONLY | O_NONBLOCK);
            pipe_fd_ = std::make_unique<boost::asio::posix::stream_descriptor>(ioc_, fd);
            LOG(INFO, LOG_TAG) << "Metadata pipe opened: " << pipePath_ << "\n";
        }
        catch (const std::exception& e)
        {
            LOG(ERROR, LOG_TAG) << "Error opening metadata pipe, retrying in 500ms. Error: " << e.what() << "\n";
            pipe_fd_ = nullptr;
            wait(pipe_open_timer_, 500ms, [this] { pipeReadLine(); });
            return;
        }
    }

    const std::string delimiter = "\n";
    boost::asio::async_read_until(*pipe_fd_, streambuf_pipe_, delimiter, [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
        if (ec)
        {
            if (ec.value() == boost::asio::error::eof)
            {
                // For some reason, EOF is returned until the first metadata is written to the pipe.
                // Is this a boost bug?
                LOG(INFO, LOG_TAG) << "Waiting for metadata, retrying in 2500ms"
                                   << "\n";
                wait(pipe_open_timer_, 2500ms, [this] { pipeReadLine(); });
            }
            else
            {
                LOG(ERROR, LOG_TAG) << "Error while reading from metadata pipe: " << ec.message() << "\n";
            }
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_pipe_.data()), buffers_begin(streambuf_pipe_.data()) + bytes_transferred - delimiter.length()};
        if (!line.empty())
        {
            if (line.back() == '\r')
                line.resize(line.size() - 1);
#ifdef HAS_EXPAT
            parse(line);
#endif
        }
        streambuf_pipe_.consume(bytes_transferred);
        pipeReadLine();
    });
}

void AirplayStream::initExeAndPath(const string& filename)
{
    path_ = "";
    exe_ = findExe(filename);
    if (!fileExists(exe_) || (exe_ == "/"))
    {
        exe_ = findExe("shairport-sync");
        if (!fileExists(exe_))
            throw SnapException("shairport-sync not found");
    }

    if (exe_.find("/") != string::npos)
    {
        path_ = exe_.substr(0, exe_.find_last_of("/") + 1);
        exe_ = exe_.substr(exe_.find_last_of("/") + 1);
    }
}


void AirplayStream::onStderrMsg(const std::string& line)
{
    if (line.empty())
        return;
    LOG(INFO, LOG_TAG) << "(" << getName() << ") " << line << "\n";
    if (line.find("Is another instance of Shairport Sync running on this device") != string::npos)
    {
        LOG(ERROR, LOG_TAG) << "It seems there is another Shairport Sync runnig on port " << port_ << ", switching to port " << port_ + 1 << "\n";
        ++port_;
        setParamsAndPipePathFromPort();
    }
    else if (line.find("Invalid audio output specified") != string::npos)
    {
        LOG(ERROR, LOG_TAG) << "shairport sync compiled without stdout audio backend\n";
        LOG(ERROR, LOG_TAG) << "build with: \"./configure --with-stdout --with-avahi --with-ssl=openssl --with-metadata\"\n";
    }
}

#ifdef HAS_EXPAT
void XMLCALL AirplayStream::element_start(void* userdata, const char* element_name, const char** attr)
{
    AirplayStream* self = (AirplayStream*)userdata;
    string name(element_name);

    self->buf_.assign("");
    if (name == "item")
        self->entry_.reset(new TageEntry);

    for (int i = 0; attr[i]; i += 2)
    {
        string name(attr[i]);
        string value(attr[i + 1]);
        if (name == "encoding")
            self->entry_->isBase64 = (value == "base64"); // Quick & dirty..
    }
}

void XMLCALL AirplayStream::element_end(void* userdata, const char* element_name)
{
    AirplayStream* self = (AirplayStream*)userdata;
    string name(element_name);

    if (name == "code")
        self->entry_->code.assign(hex2str(self->buf_));

    else if (name == "type")
        self->entry_->type.assign(hex2str(self->buf_));

    else if (name == "length")
        self->entry_->length = strtoul(self->buf_.c_str(), 0, 10);

    else if (name == "data")
        self->entry_->data = self->buf_;

    else if (name == "item")
        self->push();

    else if (name == "metatags")
        ;
    else
        cout << "Unknown tag <" << name << ">\n";
}

void XMLCALL AirplayStream::data(void* userdata, const char* content, int length)
{
    AirplayStream* self = (AirplayStream*)userdata;
    string value(content, (size_t)length);
    self->buf_.append(value);
}

#endif

} // namespace streamreader
