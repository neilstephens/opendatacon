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
 * JSONPortFactory.h
 *
 *  Created on: 09/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef opendatacon_suite_JSONPortFactory_h
#define opendatacon_suite_JSONPortFactory_h

#include <opendatacon/DataPortFactory.h>
#include "JSONPortManager.h"

class JSONPortFactory : public odc::DataPortFactory
{
public:
	
	static JSONPortFactory* Get(std::shared_ptr<odc::IOManager> pIOMgr) {
		return new JSONPortFactory(pIOMgr);
	}

	virtual std::shared_ptr<odc::IOManager> GetManager() {
		return Manager;
	}

	virtual odc::DataPort* CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides);
	
private:
	JSONPortFactory(std::shared_ptr<odc::IOManager> pIOMgr) : Manager(new JSONPortManager(pIOMgr)) { }
	~JSONPortFactory() { std::cout << "Destructing JSONPortFactory" << std::endl; }

	JSONPortFactory(const JSONPortFactory &that) = delete;
	JSONPortFactory &operator=(const JSONPortFactory &) { return *this; }
	
	std::shared_ptr<JSONPortManager> Manager;
};


#endif
