#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include<deque>
#include<algorithm>

template <class T>
class DoubleBuffer
{
public:
	DoubleBuffer(size_t size) : bufferSize(size)
	{		
	}

	void add(const T& element)
	{
		buffer.push_back(element);
		if (buffer.size() > bufferSize)
			buffer.pop_front();
	}

	T median() const
	{
		if (buffer.empty())
			return 0;
		std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
		std::sort(tmpBuffer.begin(), tmpBuffer.end());
		return tmpBuffer[tmpBuffer.size() / 2];
	}

	bool full() const
	{
		return (buffer.size() == bufferSize);
	}

	void clear()
	{
		buffer.clear();
	}

	size_t size() const
	{
		return buffer.size();
	}

private:
	size_t bufferSize;	
	std::deque<T> buffer;
	
};



#endif
