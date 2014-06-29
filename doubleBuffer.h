#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include<deque>
#include<algorithm>

template <class T>
class DoubleBuffer
{
public:
	DoubleBuffer(size_t bufferSize) : size(bufferSize)
	{		
	}

	void add(const T& element)
	{
		buffer.push_back(element);
		if (buffer.size() > size)
			buffer.pop_front();
	}

	T median()
	{
		if (buffer.empty())
			return 0;
		std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
		std::sort(tmpBuffer.begin(), tmpBuffer.end());
		return tmpBuffer[tmpBuffer.size() / 2];
	}

private:
	size_t size;	
	std::deque<T> buffer;
	
};



#endif
