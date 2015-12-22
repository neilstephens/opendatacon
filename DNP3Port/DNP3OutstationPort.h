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
 * DNP3ServerPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3SERVERPORT_H_
#define DNP3SERVERPORT_H_

#include <unordered_map>
#include <opendnp3/outstation/ICommandHandler.h>

#include "DNP3Port.h"

class DNP3OutstationPort: public DNP3Port, public opendnp3::ICommandHandler
{
public:
	DNP3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	void Enable();
	void Disable();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	//Override DataPort functions for UI
	const Json::Value GetCurrentState() const override;
	const Json::Value GetStatistics() const override;

	//Impl. ILinkListener
	// Called when a the reset/unreset status of the link layer changes (and on link up)
	void OnStateChange(opendnp3::LinkStatus status);
	// Called when a keep alive message (request link status) receives no response
	void OnKeepAliveFailure();
	// Called when a keep alive message receives a valid response
	void OnKeepAliveSuccess();

	//Implement ICommandHandler, whicher version of the API
	opendnp3::CommandStatus Select(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex){ return SupportsT(arCommand, aIndex); };
	opendnp3::CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex,opendnp3::OperateType op_type){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex,opendnp3::OperateType op_type){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex,opendnp3::OperateType op_type){return PerformT(arCommand,aIndex);};

	opendnp3::CommandStatus Supports(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex){ return SupportsT(arCommand, aIndex); };
	opendnp3::CommandStatus Perform(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};
	opendnp3::CommandStatus Supports(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex){return SupportsT(arCommand,aIndex);};
	opendnp3::CommandStatus Perform(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex){return PerformT(arCommand,aIndex);};

	//Impl. ITransactable
	void Start(){}
	void End(){}

	template<typename T> opendnp3::CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> opendnp3::CommandStatus PerformT(T& arCommand, uint16_t aIndex);

	//Implement some IOHandler - parent DNP3Port implements the rest to return NOT_SUPPORTED
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);
	template<typename T, typename Q> std::future<opendnp3::CommandStatus> EventQ(Q& meas, uint16_t index, const std::string& SenderName);

	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName);

	std::future<opendnp3::CommandStatus> Event(const ::BinaryQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::AnalogQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::CounterQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::FrozenCounterQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const ::AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName);

	std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName);

private:
	asiodnp3::IOutstation* pOutstation;
	void LinkStatusListener(opendnp3::LinkStatus status);
};

extern "C" DNP3OutstationPort* new_DNP3OutstationPort(std::string Name, std::string File, const Json::Value Overrides);

#endif /* DNP3SERVERPORT_H_ */
