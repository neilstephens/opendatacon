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
 * PyPortFactory.h
 *
 *  Created on: 25/04/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef opendatacon_suite_PyPortFactory_h
#define opendatacon_suite_PyPortFactory_h

#include <Python.h>
#include <datetime.h>                   // needed for struct tm

#include <asio.hpp>
#include <opendatacon/DataPortFactory.h>
#include "PyPortManager.h"

class PyPortFactory : public odc::DataPortFactory
{
public:
	virtual ~PyPortFactory();
	
	static PyPortFactory* Get(std::shared_ptr<odc::IOManager> pIOMgr)
	{
		return new PyPortFactory(pIOMgr);
	}
	
	virtual std::shared_ptr<odc::IOManager> GetManager();

	virtual odc::DataPort* CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides);
	
private:
	PyPortFactory(std::shared_ptr<odc::IOManager> pIOMgr);
	PyPortFactory(const PyPortFactory &that) = delete;
	PyPortFactory &operator=(const PyPortFactory &)= delete;
	
	std::shared_ptr<PyPortManager> Manager;
};


#endif
