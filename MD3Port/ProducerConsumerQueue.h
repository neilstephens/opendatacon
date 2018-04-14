#pragma once

#include <queue>
#include <mutex>

// Threadsafe , size limited producer/consumer queue
template <class T>
class ProducerConsumerQueue
{
private:
	const uint32_t Size = 255;	// MD3 maximum event queue size
	std::queue<T> Queue;
	std::mutex Mut;

public:
	bool Peek(T& item)
	{
		std::unique_lock<std::mutex> lck(Mut);
		if (Queue.empty())
		{
			return false;
		}
		item = Queue.front();
		return true;
	}
	bool Pop(T& item)
	{
		std::unique_lock<std::mutex> lck(Mut);
		if (Queue.empty())
		{
			return false;
		}
		item = Queue.front();
		Queue.pop();
		return true;
	}
	bool Pop()
	{
		std::unique_lock<std::mutex> lck(Mut);
		if (Queue.empty())
		{
			return false;
		}
		Queue.pop();
		return true;
	}

	bool Push(const T& item)
	{
		std::unique_lock<std::mutex> lck(Mut);

		// Limit the number of elements...
		if (Queue.size() >= Size)
			return false;

		Queue.push(item);
		return true;
	}

	bool IsEmpty()
	{
		std::unique_lock<std::mutex> lck(Mut);
		return Queue.empty();
	}

	bool IsFull()
	{
		std::unique_lock<std::mutex> lck(Mut);
		return (Queue.size >= Size);
	}
};
