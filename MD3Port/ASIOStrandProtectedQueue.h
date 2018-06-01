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
* ASIOStrandProtectedQueue.h
*
*  Created on: 01/06/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef ASIOSPC_H_
#define ASIOSPC_H_

#include <future>
#include <queue>
#include <opendatacon/asio.h>

// An asio strand protected queue. sync or async write, sync read.
// Does a coperative work task schedule (poll-one) if waiting for a result to allow other work to be done.
// Uses dispatch on the strand to do the strand protected section straight away in this thread if possible.
// If not - turns into a post and may activate the task switch/wait indicated above.

// Exceptions can even be passed to future via promise::set_exception(), supporting an elegant way to provide exceptions or errors to the caller.
template <class T>
class StrandProtectedQueue
{
public:

	StrandProtectedQueue(asio::io_service& _io_service, int _size)
		: io_service(_io_service),
		size(_size),
		internal_queue_strand(_io_service)
	{}

	// Try and pop a value, return true if success. We remove the item if successfull
	bool sync_pop(T &retval)
	{
		std::promise<T> promise;
		auto future = promise.get_future();	// You can only call get_future ONCE!!!! Otherwise throws an assert exception!
		bool success = false;

		// Dispatch will execute now - if we can, otherwise results in a post
		internal_queue_strand.dispatch([&]()
		{
			// This is only called from within the internal_queue_strand, so we are safe.
			if (!fifo.empty())
			{
				success = true;
			}
			T val = fifo.front();
			fifo.pop();
			promise.set_value(val);
		});

		// Synchronously wait for promise to be fulfilled - but we dont want to block the ASIO thread.
		while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		{
			if (io_service.stopped() == true)
			{
				// If we are closing get out of here. Dont worry about the result.
				return false;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			io_service.poll_one();
		}

		retval = future.get();
		return success;
	}

	void sync_push(T _value)
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
			if (io_service.stopped() == true)
			{
				// If we are closing get out of here.
				return;
			}
			// Our result is not ready, so let ASIO run one work handler. Kind of like a co-operative task switch
			io_service.poll_one();
		}
	}
	void async_push(T _value)
	{
		// Dispatch will execute now - if we can, otherwise results in a post
		internal_queue_strand.dispatch([&]()
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
	int size;
	asio::io_service& io_service;
	asio::strand internal_queue_strand;
};

#endif ASIOSPC_H_
