/*	opendatacon
*
*	Copyright (c) 2018:
*
*		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
*		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License.
*/
/*
* ProducerConsumerQueue.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef PRODUCERCONSUMERQUEUE_H_
#define PRODUCERCONSUMERQUEUE_H_

#include <queue>
#include <mutex>
#include <shared_mutex>

// Threadsafe , size limited producer/consumer queue
template <class T>
class ProducerConsumerQueue
{
private:
	size_t MaxSize;
	std::queue<T> Queue;
	std::shared_timed_mutex m;

public:
	ProducerConsumerQueue(size_t _MaxSize=100): MaxSize(_MaxSize)
	{}

	// Mutex cannot be copied for obvious reasons.
	ProducerConsumerQueue& operator=(const ProducerConsumerQueue& src)
	{
		Queue = src.Queue;
		MaxSize = src.MaxSize;
		return *this;
	}

	void SetSize(size_t _MaxSize)
	{
		MaxSize = _MaxSize;
	}

	bool Peek(T& item)
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		if (Queue.empty())
		{
			return false;
		}
		item = Queue.front();
		return true;
	}
	bool Pop(T& item)
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
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
		std::shared_lock<std::shared_timed_mutex> lck(m);
		if (Queue.empty())
		{
			return false;
		}
		Queue.pop();
		return true;
	}

	bool Push(const T& item)
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);

		// Limit the number of elements...
		if (Queue.size() >= MaxSize)
			return false;

		Queue.push(item);
		return true;
	}

	bool IsEmpty()
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		return Queue.empty();
	}
	size_t Size()
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		return Queue.size();
	}
	bool IsFull()
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		return (Queue.size >= MaxSize);
	}
};
#endif
