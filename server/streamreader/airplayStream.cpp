/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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

#include "airplayStream.h"
#include "common/snapException.h"
#include "common/utils/string_utils.h"
#include "common/utils.h"
#include "base64.h"
#include "aixlog.hpp"

using namespace std;

static string hex2str(string input)
{
	typedef unsigned char byte;
	unsigned long x = strtoul(input.c_str(), 0, 16);
	byte a[] = {byte(x >> 24), byte(x >> 16), byte(x >> 8), byte(x), 0};
	return string((char *)a);
}

/*
 * Expat is used in metadata parsing from Shairport-sync.
 * Without HAS_EXPAT defined no parsing will occur.
 *
 * This is currently defined in airplayStream.h, prolly should 
 * move to Makefile?
 */

AirplayStream::AirplayStream(PcmListener* pcmListener, const StreamUri& uri) : ProcessStream(pcmListener, uri), port_(5000)
{
	logStderr_ = true;

	pipePath_ = "/tmp/shairmeta." + cpt::to_string(getpid());
	//cout << "Pipe [" << pipePath_ << "]\n";

	// XXX: Check if pipe exists, delete or throw error

	sampleFormat_ = SampleFormat("44100:16:2");
 	uri_.query["sampleformat"] = sampleFormat_.getFormat();

	port_ = cpt::stoul(uri_.getQuery("port", "5000")); 

	string devicename = uri_.getQuery("devicename", "Snapcast");
	params_wo_port_ = "--name=\"" + devicename + "\" --output=stdout";
	params_wo_port_ += " --metadata-pipename " + pipePath_;
	params_ = params_wo_port_ + " --port=" + cpt::to_string(port_);

	pipeReaderThread_ = thread(&AirplayStream::pipeReader, this);
	pipeReaderThread_.detach();
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

	if((result = XML_Parse(parser_, line.c_str(), line.length(), false)) == XML_STATUS_ERROR)
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
	if(entry_->isBase64 && entry_->length > 0)
		data = base64_decode(data);

	if(entry_->type == "ssnc" && entry_->code == "mdst")
		jtag_ = json();

	if(entry_->code == "asal") jtag_["ALBUM"]  = data;
	if(entry_->code == "asar") jtag_["ARTIST"] = data;
	if(entry_->code == "minm") jtag_["TITLE"]  = data;

	if(entry_->type == "ssnc" && entry_->code == "mden"){
		//LOG(INFO) << "metadata=" << jtag_.dump(4) << "\n";
		setMeta(jtag_);
	}
}
#endif

void AirplayStream::pipeReader()
{
#ifdef HAS_EXPAT
	createParser();
#endif

	while(true)
	{
		ifstream pipe(pipePath_);

		if(pipe){
			string line;

			while(getline(pipe, line)){
#ifdef HAS_EXPAT
				parse(line);
#endif
			}
		}

		// Wait a little until we try to open it again
		this_thread::sleep_for(chrono::milliseconds(500));
	}
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


void AirplayStream::onStderrMsg(const char* buffer, size_t n)
{
	string logmsg = utils::string::trim_copy(string(buffer, n));
	if (logmsg.empty())
		return;
	LOG(INFO) << "(" << getName() << ") " << logmsg << "\n";
	if (logmsg.find("Is another Shairport Sync running on this device") != string::npos)
	{
		LOG(ERROR) << "Seem there is another Shairport Sync runnig on port " << port_ << ", switching to port " << port_ + 1 << "\n";
		++port_;
		params_ = params_wo_port_ + " --port=" + cpt::to_string(port_);
	}
	else if (logmsg.find("Invalid audio output specified") != string::npos)
	{
		LOG(ERROR) << "shairport sync compiled without stdout audio backend\n";
		LOG(ERROR) << "build with: \"./configure --with-stdout --with-avahi --with-ssl=openssl --with-metadata\"\n";
	}
}

#ifdef HAS_EXPAT
void XMLCALL AirplayStream::element_start(void *userdata, const char *element_name, const char **attr)
{
	AirplayStream *self = (AirplayStream *)userdata;
	string name(element_name);

	self->buf_.assign("");
	if(name == "item") self->entry_.reset(new TageEntry);

	for(int i = 0; attr[i]; i += 2){
		string name(attr[i]);
		string value(attr[i+1]);
		if(name == "encoding")
			self->entry_->isBase64 = (value == "base64"); // Quick & dirty..
	}
}

void XMLCALL AirplayStream::element_end(void *userdata, const char *element_name)
{
	AirplayStream *self = (AirplayStream *)userdata;
	string name(element_name);

	if(name == "code")
		self->entry_->code.assign(hex2str(self->buf_));

	else if(name == "type")
		self->entry_->type.assign(hex2str(self->buf_));

	else if(name == "length")
		self->entry_->length = strtoul(self->buf_.c_str(), 0, 10);

	else if(name == "data")
		self->entry_->data = self->buf_;

	else if(name == "item")
		self->push();

	else if(name == "metatags") ;
	else cout << "Unknown tag <" << name << ">\n";
}

void XMLCALL AirplayStream::data(void *userdata, const char *content, int length)
{
	AirplayStream *self = (AirplayStream *)userdata;
	string value(content, (size_t)length);
	self->buf_.append(value);
}
#endif

