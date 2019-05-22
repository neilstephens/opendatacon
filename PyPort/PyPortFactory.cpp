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
 * PyPortFactory.cpp
 *
 *  Created on: 25/04/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#include "PyPortFactory.h"
#include "PyPortManager.h"
#include "PyPort.h"
#include <Python.h>
#include <time.h>
#include <datetime.h>

PyPortFactory::PyPortFactory(std::shared_ptr<odc::IOManager> pIOMgr): Manager(new PyPortManager(pIOMgr)) {}

PyPortFactory::~PyPortFactory()
{
	std::cout << "Destructing PyPortFactory" << std::endl;
}

DataPort* PyPortFactory::CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	if (Type.compare("Py") == 0)
		return new PyPort(Manager, Name, File, Overrides);
	return nullptr;
}

std::shared_ptr<odc::IOManager> PyPortFactory::GetManager()
{
	return Manager;
}
