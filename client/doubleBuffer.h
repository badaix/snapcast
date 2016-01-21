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

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include<deque>
#include<algorithm>


/// Size limited queue
/**
 * Size limited queue with basic statistic functions:
 * median, mean, percentile
 */
template <class T>
class DoubleBuffer
{
public:
	DoubleBuffer(size_t size = 10) : bufferSize(size)
	{
	}

	inline void add(const T& element)
	{
		buffer.push_back(element);
		if (buffer.size() > bufferSize)
			buffer.pop_front();
	}

	/// Median as mean over N values around the median
	T median(unsigned int mean = 1) const
	{
		if (buffer.empty())
			return 0;
		std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
		std::sort(tmpBuffer.begin(), tmpBuffer.end());
		if ((mean <= 1) || (tmpBuffer.size() < mean))
			return tmpBuffer[tmpBuffer.size() / 2];
		else
		{
			unsigned int low = tmpBuffer.size() / 2;
			unsigned int high = low;
			low -= mean/2;
			high += mean/2;
			T result((T)0);
			for (unsigned int i=low; i<=high; ++i)
			{
				result += tmpBuffer[i];
			}
			return result / mean;
		}
	}

	double mean() const
	{
		if (buffer.empty())
			return 0;
		double mean = 0.;
		for (size_t n=0; n<buffer.size(); ++n)
			mean += (float)buffer[n] / (float)buffer.size();
		return mean;
	}

	T percentile(unsigned int percentile) const
	{
		if (buffer.empty())
			return 0;
		std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
		std::sort(tmpBuffer.begin(), tmpBuffer.end());
		return tmpBuffer[(size_t)(tmpBuffer.size() * ((float)percentile / (float)100))];
	}

	inline bool full() const
	{
		return (buffer.size() == bufferSize);
	}

	inline void clear()
	{
		buffer.clear();
	}

	inline size_t size() const
	{
		return buffer.size();
	}

	inline bool empty() const
	{
		return (buffer.size() == 0);
	}

	void setSize(size_t size)
	{
		bufferSize = size;
	}

	const std::deque<T>& getBuffer() const
	{
		return &buffer;
	}

private:
	size_t bufferSize;
	std::deque<T> buffer;

};



#endif


