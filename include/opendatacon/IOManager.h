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
#include <asiodnp3/DNP3Manager.h>

namespace odc {
	//typedef asiodnp3::DNP3Manager IOManager;
	
	class IOManager {
	public:
		virtual asio::io_service& get_io_service() = 0;
		
		/**
		 * Add a callback to receive log messages
		 * @param handler reference to a log handling interface
		 */
		virtual void AddLogSubscriber(openpal::ILogHandler& handler) = 0;
		
		/**
		 * Permanently shutdown the manager and all sub-objects that have been created. Stop
		 * the thead pool.
		 */
		virtual void Shutdown() = 0;
	};
	
	class ODCManager : public IOManager, public asiodnp3::DNP3Manager {
	public:
		ODCManager(
				  uint32_t concurrencyHint,
				  openpal::ICryptoProvider* crypto = nullptr,
				  std::function<void()> onThreadStart = []() {},
				  std::function<void()> onThreadExit = []() {}
				  ) : DNP3Manager(concurrencyHint, crypto, onThreadStart, onThreadExit), IOS(concurrencyHint) {
			
		};
		
		void AddLogSubscriber(openpal::ILogHandler& handler) {
			
			
		}
		
		void Shutdown() {
			asiodnp3::DNP3Manager::Shutdown();
		}
		
		asio::io_service& get_io_service()
		{
			return IOS;
		}
		
	private:
		asio::io_service IOS;
	};
}

#endif
