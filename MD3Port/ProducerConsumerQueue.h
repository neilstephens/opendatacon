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
#endif