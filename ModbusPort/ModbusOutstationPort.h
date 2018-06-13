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
 * ModbusOutstationPort.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef ModbusSERVERPORT_H_
#define ModbusSERVERPORT_H_

#include <unordered_map>

#include <modbus/modbus.h>
#include "ModbusPort.h"

class ModbusOutstationPort: public ModbusPort
{
public:
	ModbusOutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~ModbusOutstationPort() override;

	void Enable() override;
	void Disable() override;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	template<typename T> void EventT(T& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback);

	void Connect();
	void Disconnect();

private:
	void StateListener(ChannelState state);
	modbus_t *mb;
	modbus_mapping_t *mb_mapping;
};

#endif /* ModbusSERVERPORT_H_ */
