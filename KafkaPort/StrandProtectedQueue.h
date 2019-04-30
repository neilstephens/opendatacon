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

	typedef std::function<void (std::vector<T> Events)> ProcessAllEventsCallbackFn;
	typedef std::shared_ptr <ProcessAllEventsCallbackFn> ProcessAllEventsCallbackFnPtr;

	StrandProtectedQueue(asio::io_service& _io_service, unsigned int _size, std::function<void()> _queuehandler)
		: size(_size),
		io_service(_io_service),
		queuehandler(_queuehandler),
		internal_queue_strand(_io_service)
	{}

	void async_push(const T &_value)
	{
		// Just do a post. Need to copy the _value into the Lambda as it will go out of scope when async_push exits
		internal_queue_strand.dispatch([&,_value]()
			{
				// This is only called from within the internal_queue_strand, so we are safe.
				// Only push if we have space
				if (fifo.size() < size)
				{
				      fifo.push(_value);
				}
				else
					LOGDEBUG("Strand Protected Queue Overflowed - dumping pushed Item..");

				// Check to see if we need to schedule the queuehandler - to process what we have put in the queue. The bool flag is strand protected...
				if (NoQueuedHandler)
				{
				      NoQueuedHandler = false;
				      if (queuehandler)
						io_service.post([&]()
							{
								queuehandler();
							});
				}
			});
	}
	// Pass in the function to call with all the events currently in the queue
	void async_pop_all(const ProcessAllEventsCallbackFnPtr &eventscallback)
	{
		// Dispatch will execute now - if we can, otherwise results in a post.
		// Need to copy the eventscallback into the Lambda as it will possibly go out of scope
		internal_queue_strand.dispatch([&, eventscallback]()
			{
				// This is only called from within the internal_queue_strand, so we are thread safe.
				auto totalevents = fifo.size();
				std::vector<T> Events(totalevents);

				//Move all the queued items into the vector for processing
				for (int i = 0; i < totalevents; i++)
				{
				      Events[i] = fifo.front();
				      fifo.pop();
				}

				// Post the callback with the Events vector - so we dont lock up the strand for extended periods
				if (eventscallback != nullptr)
					io_service.post([Events, eventscallback]() // Need to copy the vector as it goes out of scope - do a move - the compiler probably does it for us?
						{
							(*eventscallback)(Events);
						});

				// If we have new data, schedule the queue handler again to process the items - which will call us again..
				if (fifo.size() != 0)
				{
				      if (queuehandler)
				      {
				            io_service.post([&]()
							{
								queuehandler();
							});
					}
				}
				else
				{
				// No queued handler, so when we next push an item, queue the handler.
				      NoQueuedHandler = true;
				}
			});
	}

private:
	std::queue<T> fifo;
	unsigned int size;
	asio::io_service& io_service;
	asio::strand internal_queue_strand;
	std::function<void()> queuehandler = nullptr;
	bool NoQueuedHandler = true;
};

#endif
