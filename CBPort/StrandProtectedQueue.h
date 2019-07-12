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
*
* StrandProtectedMPSCQueue.h
*
*  Created on: 01/06/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef ASIOSPC_H_
#define ASIOSPC_H_

#include <future>
#include <queue>
#include <opendatacon/asio.h>

// An asio strand protected queue. Multiproducer, Multiconsumer.
// sync or async write, sync read.
// Does a coperative work task schedule (poll-one) if waiting for a result to allow other work to be done.
// Uses dispatch on the strand to do the strand protected section straight away in this thread if possible.
// If not - turns into a post and may activate the task switch/wait indicated above.

// Exceptions can even be passed to future via promise::set_exception(), supporting an elegant way to provide exceptions or errors to the caller.
template <class T>
class StrandProtectedQueue
{
public:

	StrandProtectedQueue(asio::io_context& _io_context, unsigned int _size)
		: size(_size),
		queue_io_context(_io_context),
		internal_queue_strand(_io_context)
	{}
/*	StrandProtectedQueue()
            : io_context(nullptr),
            size(256),
            internal_queue_strand(nullptr)
      {}
*/

	// Return front of queue value
	bool sync_front(T &retval)
	{
		std::promise<T> promise;
		auto future = promise.get_future(); // You can only call get_future ONCE!!!! Otherwise throws an assert exception!
		bool success = false;

		// Dispatch will execute now - if we can, otherwise results in a post
		internal_queue_strand.dispatch([&]()
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				if (!fifo.empty())
				{
				      success = true;
				      T val = fifo.front();
				      promise.set_value(val);
				}
				else
				{
				      T val;
				      promise.set_value(val); // success is false,so this value should never be used
				}
			});

		// Synchronously wait for promise to be fulfilled - but we dont want to block the ASIO thread.
		while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		{
			if (queue_io_context.stopped() == true)
			{
				// If we are closing get out of here. Dont worry about the result.
				return false;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			queue_io_context.poll_one();
		}

		retval = future.get();
		return success;
	}

	// Try and pop a value, return true if success. We remove the item if successfull
	bool sync_pop()
	{
		std::promise<bool> promise;
		auto future = promise.get_future(); // You can only call get_future ONCE!!!! Otherwise throws an assert exception!

		// Dispatch will execute now - if we can, otherwise results in a post
		internal_queue_strand.dispatch([&]()
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				if (!fifo.empty())
				{
				      fifo.pop();
				      promise.set_value(true);
				}
				else
				{
				      promise.set_value(false);
				}
			});

		// Synchronously wait for promise to be fulfilled - but we dont want to block the ASIO thread.
		while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		{
			if (queue_io_context.stopped() == true)
			{
				// If we are closing get out of here. Dont worry about the result.
				return false;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			queue_io_context.poll_one();
		}

		return future.get();
	}
	bool sync_empty()
	{
		std::promise<bool> promise;
		auto future = promise.get_future(); // You can only call get_future ONCE!!!! Otherwise throws an assert exception!

		internal_queue_strand.dispatch([&]() // Dispatch will execute now - if we can, otherwise results in a post
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				promise.set_value(fifo.empty());
			});

		// Synchronously wait for promise to be fulfilled - but we dont want to block the ASIO thread.
		while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		{
			if (queue_io_context.stopped() == true)
			{
				// If we are closing get out of here. Dont worry about the result.
				return false;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			queue_io_context.poll_one();
		}

		return future.get();
	}
	void sync_push(const T &_value)
	{
		std::promise<void> voidpromise;
		auto future = voidpromise.get_future();

		// Dispatch will execute now - if we can, otherwise results in a post
		internal_queue_strand.dispatch([&]()
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				// Only push if we have space
				if (fifo.size() < size)
				{
				      fifo.push(_value);
				}

				voidpromise.set_value();
			});

		// Synchronously wait for promise to be fulfilled - but we dont want to block the ASIO thread.
		while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		{
			if (queue_io_context.stopped() == true)
			{
				// If we are closing get out of here.
				return;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			queue_io_context.poll_one();
		}
	}
	void async_push(const T &_value)
	{
		// Dispatch will execute now - if we can, otherwise results in a post. Need to copy the _value into the Lambda as it will go out of scope when async_push exits
		internal_queue_strand.dispatch([&,_value]()
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				// Only push if we have space
				if (fifo.size() < size)
				{
				      fifo.push(_value);
				}
			});
	}

private:
	std::queue<T> fifo;
	unsigned int size;
	asio::io_context& queue_io_context;
	asio::io_context::strand internal_queue_strand;
};

#endif
