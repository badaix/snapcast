#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T>
class Queue
{
public:

	T pop()
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		{
		  cond_.wait(mlock);
		}
		auto val = queue_.front();
		queue_.pop();
		return val;
	}

	T front()
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		  cond_.wait(mlock);

		return queue_.front();
	}

	bool try_pop(T& item, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> mlock(mutex_);

		if(!cond_.wait_for(mlock, timeout, [this] { return !queue_.empty(); }))
		    return false;

		item = std::move(queue_.front());
		queue_.pop();

		return true;    
	}

	void pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		{
			cond_.wait(mlock);
		}
		item = queue_.front();
		queue_.pop();
	}

	void push(const T item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		queue_.push(item);
		mlock.unlock();
		cond_.notify_one();
	}

	void push(T&& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		queue_.push(std::move(item));
		mlock.unlock();
		cond_.notify_one();
	}

	Queue()=default;
	Queue(const Queue&) = delete;            // disable copying
	Queue& operator=(const Queue&) = delete; // disable assignment

	private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
};



