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


#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include "processStream.h"
#include "common/snapException.h"
#include "common/utils.h"
#include "common/log.h"


using namespace std;




ProcessStream::ProcessStream(PcmListener* pcmListener, const StreamUri& uri) : PcmStream(pcmListener, uri), path_(""), process_(nullptr)
{
	params_ = uri_.getQuery("params");
	logStderr_ = (uri_.getQuery("logStderr", "false") == "true");
}


ProcessStream::~ProcessStream()
{
	if (process_)
		process_->kill();
}


bool ProcessStream::fileExists(const std::string& filename) 
{
	struct stat buffer;
	return (stat(filename.c_str(), &buffer) == 0); 
}


std::string ProcessStream::findExe(const std::string& filename)
{
	/// check if filename exists
	if (fileExists(filename))
		return filename;

	std::string exe = filename;
	if (exe.find("/") != string::npos)
		exe = exe.substr(exe.find_last_of("/") + 1);

	/// check with "whereis"
	string whereis = execGetOutput("whereis " + exe);
	if (whereis.find(":") != std::string::npos)
	{
		whereis = trim_copy(whereis.substr(whereis.find(":") + 1));
		if (!whereis.empty())
			return whereis;
	}

	/// check in the same path as this binary
	char buff[PATH_MAX];
	char szTmp[32];
	sprintf(szTmp, "/proc/%d/exe", getpid());
	ssize_t len = readlink(szTmp, buff, sizeof(buff)-1);
	if (len != -1)
	{
		buff[len] = '\0';
		return string(buff) + "/" + exe;
	}

	return "";
}


void ProcessStream::initExeAndPath(const std::string& filename)
{
	path_ = "";
	exe_ = findExe(filename);
	if (exe_.find("/") != string::npos)
	{
		path_ = exe_.substr(0, exe_.find_last_of("/") + 1);
		exe_ = exe_.substr(exe_.find_last_of("/") + 1);
	}

	if (!fileExists(path_ + exe_))
		throw SnapException("file not found: \"" + filename + "\"");
}


void ProcessStream::start()
{
	initExeAndPath(uri_.path);
	PcmStream::start();
}


void ProcessStream::stop()
{
	if (process_)
		process_->kill();
	PcmStream::stop();
	stderrReaderThread_.join();
}


void ProcessStream::onStderrMsg(const char* buffer, size_t n)
{
	if (logStderr_)
	{
		string line = trim_copy(string(buffer, n));
		if ((line.find('\0') == string::npos) && !line.empty())
			logO << "(" << exe_ << ") " << line << "\n";
	}
}


void ProcessStream::stderrReader()
{
	size_t buffer_size = 8192;
	auto buffer = std::unique_ptr<char[]>(new char[buffer_size]);
	ssize_t n;
	stringstream message;
	while (active_ && (n=read(process_->getStderr(), buffer.get(), buffer_size)) > 0)
		onStderrMsg(buffer.get(), n);
}


void ProcessStream::worker()
{
	timeval tvChunk;
	std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, pcmReadMs_));

	setState(kPlaying);

	while (active_)
	{
		process_.reset(new Process(path_ + exe_ + " " + params_, path_));
		int flags = fcntl(process_->getStdout(), F_GETFL, 0);
		fcntl(process_->getStdout(), F_SETFL, flags | O_NONBLOCK);

		stderrReaderThread_ = thread(&ProcessStream::stderrReader, this);
		stderrReaderThread_.detach();

		gettimeofday(&tvChunk, NULL);
		tvEncodedChunk_ = tvChunk;
		long nextTick = chronos::getTickCount();
		try
		{
			while (active_)
			{
				chunk->timestamp.sec = tvChunk.tv_sec;
				chunk->timestamp.usec = tvChunk.tv_usec;
				int toRead = chunk->payloadSize;
				int len = 0;
				do
				{
					int count = read(process_->getStdout(), chunk->payload + len, toRead - len);
					if (count < 0)
					{
						setState(kIdle);
						chronos::sleep(100);
					}
					else if (count == 0)
						throw SnapException("end of file");
					else
						len += count;
				}
				while ((len < toRead) && active_);

				encoder_->encode(chunk.get());
				nextTick += pcmReadMs_;
				chronos::addUs(tvChunk, pcmReadMs_ * 1000);
				long currentTick = chronos::getTickCount();

				if (nextTick >= currentTick)
				{
					setState(kPlaying);
					chronos::sleep(nextTick - currentTick);
				}
				else
				{
					gettimeofday(&tvChunk, NULL);
					tvEncodedChunk_ = tvChunk;
					pcmListener_->onResync(this, currentTick - nextTick);
					nextTick = currentTick;
				}
			}
		}
		catch(const std::exception& e)
		{
			logE << "Exception: " << e.what() << std::endl;
			process_->kill();
			int sleepMs = 30000;
			while (active_ && (sleepMs > 0))
			{
				chronos::sleep(100);
				sleepMs -= 100;
			}
		}
	}
}
