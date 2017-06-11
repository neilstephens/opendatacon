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


#ifndef IOMANAGER_H
#define IOMANAGER_H

#include <asio.hpp>
#include "Task.h"
#include "Timestamp.h"

namespace odc {
	typedef std::function<void()> HandlerT;
	
	class IOManager {
	public:
		IOManager() {};
		IOManager(std::shared_ptr<odc::IOManager> pIOMgr) {};
		virtual ~IOManager() {};
		virtual asio::io_service& get_io_service() = 0;
		virtual std::shared_ptr<asio::strand> get_strand() = 0;
		virtual void dispatch(HandlerT handler) = 0;
		virtual void post(HandlerT handler) = 0;
		virtual void post(Task& task) = 0;
		virtual void stop(Task& task) = 0;
		virtual void yield() = 0;
		void run() {
			get_io_service().run();
		}
	};
	
	class AsyncIOManager : public IOManager {
	public:
		AsyncIOManager(std::shared_ptr<odc::IOManager> pIOMgr) :
		impl(pIOMgr) {
			
		}
		
		virtual asio::io_service& get_io_service() override
		{
			return impl->get_io_service();
		}
		
		virtual std::shared_ptr<asio::strand> get_strand() override
		{
			return impl->get_strand();
		}
		
		virtual void dispatch(HandlerT handler) override
		{
			impl->dispatch(handler);
		}
		
		virtual void post(HandlerT handler) override
		{
			impl->post(handler);
		}
		virtual void post(Task& task) override;
		virtual void stop(Task& task) override;
		
		virtual void yield() override
		{
			impl->yield();
		}
		
	private:
		std::shared_ptr<odc::IOManager> impl;
	};
	
	template<class T>
	class SyncIOManager : public T {
	public:
		SyncIOManager(std::shared_ptr<T> pIOMgr) :
		T(pIOMgr),
		impl(pIOMgr),
		pStrand(pIOMgr->get_strand()) {
			
		}
		
		virtual asio::io_service& get_io_service() override
		{
			return impl->get_io_service();
		}
		
		virtual std::shared_ptr<asio::strand> get_strand() override
		{
			return pStrand;
		}
		
		virtual void dispatch(HandlerT handler) override
		{
			pStrand->dispatch(handler);
		}
		
		virtual void post(HandlerT handler) override
		{
			pStrand->post(handler);
		}

		virtual void post(Task& task) override
		{
			task.schedule(this);
			impl->post(task);
		}
		
		virtual void stop(Task& task) override
		{
			impl->stop(task);
		}
		
		virtual void yield() override
		{
			impl->yield();
		}
	private:
		std::shared_ptr<odc::IOManager> impl;
		std::shared_ptr<asio::strand> pStrand;
	};
}

#endif
