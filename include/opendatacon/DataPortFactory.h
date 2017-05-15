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
 * DataPortFactory.h
 *
 *  Created on: 06/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef opendatacon_suite_DataPortFactory_h
#define opendatacon_suite_DataPortFactory_h

#include <asio.hpp>
#include "DataPort.h"
#include "IOManager.h"

namespace odc {
	
	class DataPortManager
	{
	public:
		virtual void Shutdown() = 0;
	};
	
	class DataPortFactory
	{
	public:
		DataPortFactory() {};
		virtual DataPort* CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides) = 0;
		virtual std::shared_ptr<DataPortManager> GetManager() = 0;
	private:
		DataPortFactory(const DataPortFactory &that) = delete;
		DataPortFactory &operator=(const DataPortFactory &) = delete;
		
	};
	
}

#endif
