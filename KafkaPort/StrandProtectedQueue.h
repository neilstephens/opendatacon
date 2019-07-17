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

	StrandProtectedQueue(odc::asio_service& _io_context, unsigned int _size, std::function<void()> _queuehandler)
		: size(_size),
		queue_io_context(_io_context),
		queuehandler(_queuehandler),
		internal_queue_strand(_io_context.make_strand())
	{}

	void async_push(const T &_value)
	{
		// Just do a post. Need to copy the _value into the Lambda as it will go out of scope when async_push exits
		internal_queue_strand->dispatch([&,_value]()
			{
				LOGDEBUG("Push Async Dispatch Method Called");
				// This is only called from within the internal_queue_strand, so we are safe.
				// Only push if we have space
				if (fifo.size() < size)
				{
				      fifo.push(_value);
				}
				else
					LOGDEBUG("Strand Protected Queue Overflowed - dumping pushed Item..");

				// Check to see if we need to schedule the queuehandler - to process what we have put in the queue.
				if (queuehandler && !ProcessingEvents)
				{
				      ProcessingEvents = true; // Protected by strand
				      queue_io_context.post([&]()
						{
							queuehandler();
						});
				}
			});
	}
	// Pass in the function to call which we call with all the events currently in the queue
	void async_pop_all(const ProcessAllEventsCallbackFnPtr &eventscallback)
	{
		// Dispatch will execute now - if we can, otherwise results in a post.
		// Need to copy the eventscallback into the Lambda as it will possibly go out of scope
		internal_queue_strand->dispatch([&, eventscallback]()
			{
				LOGDEBUG("POP All Dispatch Method Called");
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
					internal_queue_strand->dispatch([&,Events, eventscallback]() // Need to copy the vector as it goes out of scope - do a move - the compiler probably does it for us?
						{
							(*eventscallback)(Events);
						});

			});
	}

	// Indicate that we have finished the pop-all call, and we can call the handler again.
	void finished_pop_all()
	{
		// Dispatch will execute now - if we can, otherwise results in a post.
		internal_queue_strand->dispatch([&]()
			{
				LOGDEBUG("Finished POP All Callback");
				ProcessingEvents = false; // We are now finished processing the list of events, we can start another. Protected by strand
			});
	}

private:
	std::queue<T> fifo;
	unsigned int size;
	odc::asio_service & queue_io_context;
	std::unique_ptr<asio::io_context::strand> internal_queue_strand;
	std::function<void()> queuehandler = nullptr;
	bool ProcessingEvents = false;
};

#endif
