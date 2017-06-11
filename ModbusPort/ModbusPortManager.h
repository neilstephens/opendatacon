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
 * ModbusPortManager.h
 *
 *  Created on: 09/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef opendatacon_suite_ModbusPortManager_h
#define opendatacon_suite_ModbusPortManager_h

#include <opendatacon/IOManager.h>
#include <asio.hpp>
#include <unordered_map>

#include "ModbusPortConf.h"
#include <asiodnp3/DNP3Manager.h>
#include <opendnp3/LogLevels.h>
#include <opendatacon/DataPortFactory.h>

class ModbusPortManager : public odc::AsyncIOManager
{
public:
	ModbusPortManager(const std::shared_ptr<odc::IOManager>& pIOMgr);
	~ModbusPortManager();

private:
	ModbusPortManager(const ModbusPortManager &other) = delete;
};

#endif
