/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 * IOManager.h
 *
 *  Created on: 28/02/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef ODCMANAGER_H
#define ODCMANAGER_H

#include <asio.hpp>
#include <thread>
#include <asiodnp3/DNP3Manager.h>
#include "IOManager.h"
#include "TaskScheduler.h"

namespace odc {
	class ODCManager : public IOManager {
	public:
		ODCManager(
				  uint32_t concurrencyHint,
				  openpal::ICryptoProvider* crypto = nullptr,
				  std::function<void()> onThreadStart = []() {},
				  std::function<void()> onThreadExit = []() {}
				   ) ;
		virtual ~ODCManager()
		{
			Shutdown();
		}
		
		virtual asio::io_service& get_io_service() override
		{
			return IOS;
		}
		
		virtual std::shared_ptr<asio::strand> get_strand() override
		{
			return std::shared_ptr<asio::strand>(new asio::strand(IOS));
		}
		
		virtual void dispatch(HandlerT handler) override
		{
			IOS.dispatch(handler);
		}
		
		virtual void post(HandlerT handler) override
		{
			IOS.post(handler);
		}
		
		virtual void post(Task& task) override
		{
			scheduler.Schedule(task);
		}		

		virtual void stop(Task& task) override
		{
			scheduler.Unschedule(task);
		}
		
		virtual void yield() override
		{
			IOS.poll_one();
		}

		virtual void Shutdown() override
		{
			scheduler.Shutdown();
			ios_working.reset();
			IOS.run();
		}

	private:
		asio::io_service IOS;
		std::unique_ptr<asio::io_service::work> ios_working;
		TaskScheduler scheduler;
	};
}

#endif
