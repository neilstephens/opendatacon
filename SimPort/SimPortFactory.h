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
 * SimPortFactory.h
 *
 *  Created on: 09/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef opendatacon_suite_SimPortFactory_h
#define opendatacon_suite_SimPortFactory_h

#include <opendatacon/DataPortFactory.h>
#include "SimPortManager.h"

class SimPortFactory : public odc::DataPortFactory
{
public:
	
	static SimPortFactory* Get(std::shared_ptr<odc::IOManager> pIOMgr) {
		return new SimPortFactory(pIOMgr);
	}

	virtual std::shared_ptr<odc::IOManager> GetManager() {
		return Manager;
	}

	virtual odc::DataPort* CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides);
	
private:
	SimPortFactory(std::shared_ptr<odc::IOManager> pIOMgr) : Manager(new SimPortManager(pIOMgr)) { }
	~SimPortFactory() { std::cout << "Destructing SimPortFactory" << std::endl; }

	SimPortFactory(const SimPortFactory &that) = delete;
	SimPortFactory &operator=(const SimPortFactory &) { return *this; }
	
	std::shared_ptr<SimPortManager> Manager;
};


#endif
