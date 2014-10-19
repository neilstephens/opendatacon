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
#include <opendnp3/outstation/ICommandHandler.h>

#include "ModbusPort.h"

class ModbusOutstationPort: public ModbusPort, public opendnp3::ICommandHandler
{
public:
	ModbusOutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	void Enable();
	void Disable();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	opendnp3::CommandStatus Supports(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};

	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName);

	template<typename T> opendnp3::CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> opendnp3::CommandStatus PerformT(T& arCommand, uint16_t aIndex);
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);

	//ModbusOutstationType* pOutstation;
    
    void Connect();
    void Disconnect();

private:
	void StateListener(opendnp3::ChannelState state);
    modbus_t *mb;    
};

#endif /* ModbusSERVERPORT_H_ */
