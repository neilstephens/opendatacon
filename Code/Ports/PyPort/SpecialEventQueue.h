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
* SpecialEventQueue.h
*
*  Created on: 17/9/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef SPECIALEVENTQUEUE_H_
#define SPECIALEVENTQUEUE_H_

#include <atomic>
#include <opendatacon/asio.h>

// This is a unique, multi producer with strand protection on the production queue - with a single thread consumer.
// It does not try to "fully" empty the queue, as it is based on the fact that there will be a continuous
// stream of data. As a result of this, the worst case is that it there might be an item in "limbo" but only
// until the next new item is pushed.
// Also the fullness of the queue is a bit flexible, but should never be achived under normal operation.
template <class T>
class SpecialEventQueue
{
private:
	class node
	{
public:
		std::shared_ptr<node> next; // Points "up" the queue from tail to head
		std::shared_ptr<T> data;
		node(std::shared_ptr<T> _data): next(nullptr), data(_data)
		{}
	};

	std::shared_ptr<node> head;
	std::shared_ptr<node> tail;
	std::atomic_bool enabled_flag;
	std::atomic<size_t> size;
	size_t maxsize;
	std::shared_ptr<odc::asio_service> pIOS;
	std::unique_ptr<asio::io_context::strand> internal_queue_strand;
	std::mutex QueueMutex;	// Hack to see if we have a sync problem with this queue....

public:
	SpecialEventQueue(std::shared_ptr<odc::asio_service> _pIOS, size_t _maxsize)
		: head(nullptr),
		tail(nullptr),
		size(0),
		maxsize(_maxsize),
		pIOS(_pIOS),
		internal_queue_strand(pIOS->make_strand())
	{
		enabled_flag.store(true);
	}

	// Is enabled at startup, but allow us to enable it again, if for some reason we do a Disable/Enable cycle on the port...
	void Enable(bool en)
	{
		enabled_flag.store(en);
		// Empty the queue if Disabled....
		if (en == false)
		{
			internal_queue_strand->dispatch([this]()
				{
					std::unique_lock<std::mutex> lck(QueueMutex);
					while (tail != nullptr)
					{
						tail = tail->next;
						size--;
					}
				});
		}
	}

	size_t Size() { return size.load(); }

	// Will return false if the queue is likely full - bit fuzzy due to threadedness, but should not happen during normal operation
	// and if we get close enough to trigger a failed push - it should be flagged as an error anyway.
	// The push is a threadsafe multi producer using ASIO strand protection.
	bool async_push(const T& _value)
	{
		// If too big, or disabled dont do anything!
		if ((size.load() >= maxsize) || (enabled_flag.load() == false))
		{
			return false;
		}

		// Do the memory allocation for the copy of the data and the node, so if it fails we can catch it.
		try
		{
			auto data = std::make_shared<T>(_value);
			auto nodeptr = std::make_shared<node>(data);

			// Dispatch will execute now - if we can, otherwise results in a post.
			internal_queue_strand->dispatch([this, nodeptr]()
				{
					if (enabled_flag.load() == false) return;

					std::unique_lock<std::mutex> lck(QueueMutex);
					// This is only called from within the internal_queue_strand, so we are safe.
					// Always add here, we have already checked
					size++;
					if (head == nullptr)
					{
					      head = nodeptr;
					      tail = head;
					}
					else
					{
					      head->next = nodeptr;
					      head = nodeptr;
					}
				});
		}
		catch (const std::exception&)
		{
			// Must be a memory allocation failure.
			return false;
		}

		return true;
	}

	// This is a single thread consumer only method. Cannot be called from mutilple threads. Will cause problems for SURE!
	std::shared_ptr<T> pop()
	{
		if (enabled_flag.load() == false) return nullptr;

		std::unique_lock<std::mutex> lck(QueueMutex);
		if ((tail == nullptr) || (tail == head))
		{
			// Dont try and change head and tail pointers at the same time - we have to be much more careful with thread safety if we do.
			// We effectively leave one element in the queue - but we have a continuous stream of data, so not really a problem
			// Comparing the pointers is threadsafe! Changing the head pointer here is not!
			return nullptr;
		}
		else
		{
			std::shared_ptr<T> data = tail->data;
			tail = tail->next;

			// Only variable we need to be thread safe between producer and consumer in this implementation is the size.
			// It is only used to limit total size of the queue.
			size--;
			return data;
		}
	}
};
#endif
