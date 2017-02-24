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
	ModbusOutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~ModbusOutstationPort();

	void Enable();
	void Disable();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName);

	template<typename T> CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> CommandStatus PerformT(T& arCommand, uint16_t aIndex);
	template<typename T> std::future<CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);


	void Connect();
	void Disconnect();

private:
	void StateListener(ChannelState state);
	modbus_t *mb;
	modbus_mapping_t *mb_mapping;
};

#endif /* ModbusSERVERPORT_H_ */
