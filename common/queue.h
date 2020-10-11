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

#ifndef QUEUE_H
#define QUEUE_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

template <typename T>
class Queue
{
public:
    T pop()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
            cond_.wait(mlock);

        //		std::lock_guard<std::mutex> lock(mutex_);
        auto val = queue_.front();
        queue_.pop_front();
        return val;
    }

    void abort_wait()
    {
        {
            std::lock_guard<std::mutex> mlock(mutex_);
            abort_ = true;
        }
        cond_.notify_one();
    }

    bool wait_for(const std::chrono::microseconds& timeout) const
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        abort_ = false;
        if (!cond_.wait_for(mlock, timeout, [this] { return (!queue_.empty() || abort_); }))
            return false;

        return !queue_.empty() && !abort_;
    }

    bool try_pop(T& item, const std::chrono::microseconds& timeout = std::chrono::microseconds(0))
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        abort_ = false;
        if (timeout.count() > 0)
        {
            if (!cond_.wait_for(mlock, timeout, [this] { return (!queue_.empty() || abort_); }))
                return false;
        }

        if (queue_.empty() || abort_)
            return false;

        item = std::move(queue_.front());
        queue_.pop_front();

        return true;
    }

    void pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
            cond_.wait(mlock);

        item = queue_.front();
        queue_.pop_front();
    }

    void push_front(const T& item)
    {
        {
            std::lock_guard<std::mutex> mlock(mutex_);
            queue_.push_front(item);
        }
        cond_.notify_one();
    }

    bool back_copy(T& copy)
    {
        std::lock_guard<std::mutex> mlock(mutex_);
        if (queue_.empty())
            return false;
        copy = queue_.back();
        return true;
    }

    bool front_copy(T& copy)
    {
        std::lock_guard<std::mutex> mlock(mutex_);
        if (queue_.empty())
            return false;
        copy = queue_.front();
        return true;
    }

    void push_front(T&& item)
    {
        {
            std::lock_guard<std::mutex> mlock(mutex_);
            queue_.push_front(std::move(item));
        }
        cond_.notify_one();
    }

    void push(const T& item)
    {
        {
            std::lock_guard<std::mutex> mlock(mutex_);
            queue_.push_back(item);
        }
        cond_.notify_one();
    }

    void push(T&& item)
    {
        {
            std::lock_guard<std::mutex> mlock(mutex_);
            queue_.push_back(std::move(item));
        }
        cond_.notify_one();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> mlock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        return (size() == 0);
    }

    Queue() = default;
    Queue(const Queue&) = delete;            // disable copying
    Queue& operator=(const Queue&) = delete; // disable assignment

private:
    std::deque<T> queue_;
    mutable std::atomic<bool> abort_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
};


#endif
