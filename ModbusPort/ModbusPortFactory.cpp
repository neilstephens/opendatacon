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
 * ModbusPortFactory.cpp
 *
 *  Created on: 09/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#include "ModbusPortFactory.h"
#include "ModbusOutstationPort.h"
#include "ModbusMasterPort.h"

ModbusPortFactory::ModbusPortFactory(std::shared_ptr<odc::IOManager> pIOMgr) : Manager_(new ModbusPortManager(pIOMgr)) {
	std::cout << "Constructing ModbusPortFactory" << std::endl;
}

ModbusPortFactory::~ModbusPortFactory() {
	std::cout << "Destructing ModbusPortFactory" << std::endl;
}

ModbusPortFactory* ModbusPortFactory::Get(std::shared_ptr<odc::IOManager> pIOMgr) {
	return new ModbusPortFactory(pIOMgr);
}

DataPort* ModbusPortFactory::CreateDataPort(const std::string& Type, const std::string& Name, const std::string& File, const Json::Value& Overrides) {
	if (Type.compare("ModbusOutstation") == 0)
		return new ModbusOutstationPort(Manager_, Name, File, Overrides);
	if (Type.compare("ModbusMaster") == 0)
		return new ModbusMasterPort(Manager_, Name, File, Overrides);
	return nullptr;
}
