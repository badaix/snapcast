/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

#pragma once


// standard headers
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>


/// Size limited queue
/**
 * Size limited queue with basic statistic functions:
 * median, mean, percentile
 */
template <class T>
class DoubleBuffer
{
public:
    /// c'tor
    explicit DoubleBuffer(size_t size = 10) : bufferSize(size)
    {
    }

    /// Add @p element, pop last, if buffer is full
    inline void add(const T& element)
    {
        buffer.push_back(element);
        if (buffer.size() > bufferSize)
            buffer.pop_front();
    }

    /// Add @p element, pop last, if buffer is full
    inline void add(T&& element)
    {
        buffer.push_back(std::move(element));
        if (buffer.size() > bufferSize)
            buffer.pop_front();
    }

    /// @return median as mean over N values around the median
    T median(uint16_t mean = 1) const
    {
        if (buffer.empty())
            return 0;
        std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
        std::sort(tmpBuffer.begin(), tmpBuffer.end());
        if ((mean <= 1) || (tmpBuffer.size() < mean))
            return tmpBuffer[tmpBuffer.size() / 2];
        else
        {
            uint16_t low = static_cast<uint16_t>(tmpBuffer.size()) / 2;
            uint16_t high = low;
            low -= mean / 2;
            high += mean / 2;
            T result((T)0);
            for (uint16_t i = low; i <= high; ++i)
            {
                result += tmpBuffer[i];
            }
            return result / mean;
        }
    }

    /// @return mean value
    double mean() const
    {
        if (buffer.empty())
            return 0;
        double mean = 0.;
        for (size_t n = 0; n < buffer.size(); ++n)
            mean += (float)buffer[n] / (float)buffer.size();
        return mean;
    }

    /// @return @p percentile percentile
    T percentile(unsigned int percentile) const
    {
        if (buffer.empty())
            return 0;
        std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
        std::sort(tmpBuffer.begin(), tmpBuffer.end());
        return tmpBuffer[(size_t)((tmpBuffer.size() - 1) * ((float)percentile / (float)100))];
    }

    /// @return array of different percentiles
    template <std::size_t Size>
    std::array<T, Size> percentiles(std::array<uint8_t, Size> percentiles) const
    {
        std::array<T, Size> result;
        result.fill(0);
        if (buffer.empty())
            return result;
        std::deque<T> tmpBuffer(buffer.begin(), buffer.end());
        std::sort(tmpBuffer.begin(), tmpBuffer.end());
        for (std::size_t i = 0; i < Size; ++i)
            result[i] = tmpBuffer[(size_t)((tmpBuffer.size() - 1) * ((float)percentiles[i] / (float)100))];

        return result;
    }

    /// @return if the buffer is full
    inline bool full() const
    {
        return (buffer.size() == bufferSize);
    }

    /// Clear the buffer
    inline void clear()
    {
        buffer.clear();
    }

    /// @return current size of the buffer
    inline size_t size() const
    {
        return buffer.size();
    }

    /// @return if the buffer is empty
    inline bool empty() const
    {
        return buffer.empty();
    }

    /// Set size of the buffer
    void setSize(size_t size)
    {
        bufferSize = size;
    }

    /// @return the raw buffer
    const std::deque<T>& getBuffer() const
    {
        return buffer;
    }

private:
    size_t bufferSize;
    std::deque<T> buffer;
};
