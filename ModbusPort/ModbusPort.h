/*	opendatacon
 *
 *	Copyright (c) 2014:
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
 * ModbusPort.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef ModbusPORT_H_
#define ModbusPORT_H_
#include "ModbusPortConf.h"
#include <modbus/modbus.h>
#include <opendatacon/DataPort.h>

using namespace odc;

//class to synchronise access to libmodbus object (libmodbus is not re-entrant)
class ModbusExecutor
{
public:
	ModbusExecutor(modbus_t* mb_,odc::asio_service& ios):
		mb(mb_),
		sync(ios.make_strand())
	{}
	~ModbusExecutor()
	{
		if (mb != nullptr)
			modbus_free(mb);
	}
	void Execute(std::function<void(modbus_t*)>&& f)
	{
		sync->dispatch([this,f = std::move(f)](){f(mb);});
	}
	bool isNull()
	{
		return (mb == nullptr);
	}
private:
	modbus_t* mb;
	std::unique_ptr<asio::io_service::strand> sync;
};

class ModbusPort: public DataPort
{
public:
	ModbusPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);

	void Enable() override =0;
	void Disable() override =0;
	void Build() override =0;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void ProcessElements(const Json::Value& JSONRoot) override;

protected:
	std::unique_ptr<ModbusExecutor> MBSync;
	std::atomic_bool stack_enabled;
};

#endif /* ModbusPORT_H_ */
