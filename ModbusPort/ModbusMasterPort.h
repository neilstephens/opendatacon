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
 * ModbusClientPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ModbusCLIENTPORT_H_
#define ModbusCLIENTPORT_H_

#include <queue>
#include <opendnp3/master/ISOEHandler.h>

#include <modbus/modbus.h>
#include "ModbusPort.h"

using namespace opendnp3;

#include <utility>

class ModbusMasterPort: public ModbusPort
{
public:
	ModbusMasterPort(std::shared_ptr<ModbusPortManager> Manager, std::string Name, std::string ConfFilename, const Json::Value ConfOverrides);
	~ModbusMasterPort();

	// Implement ModbusPort
	void Enable();
	void Disable();
	void Connect();
	void Disconnect();
	void BuildOrRebuild();

	// Implement some IOHandler - parent ModbusPort implements the rest to return NOT_SUPPORTED
	std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName);
	std::future<CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName);
	template<typename T> std::future<CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);


private:
	template<class T>
	CommandStatus WriteObject(const T& command, uint16_t index);

	void DoPoll(uint32_t pollgroup);

private:
	void HandleError(int errnum, const std::string& source);
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	ModbusReadGroup<Binary>* GetRange(uint16_t index);

	modbus_t *mb;
	void* modbus_read_buffer;
	size_t modbus_read_buffer_size;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	Task RetryConnectionTask;
	std::vector<Task> PollTasks;
};

#endif /* ModbusCLIENTPORT_H_ */
