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

class DNP3OutstationPort: public DNP3Port, public opendnp3::ICommandHandler, public opendnp3::IOutstationApplication
{
public:
	DNP3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~DNP3OutstationPort();

protected:
	/// Implement ODC::DataPort
	void Enable() override;
	void Disable() override;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	// Implement DNP3Port
	void OnLinkDown() override;
	TCPClientServer ClientOrServer() override;

	/// Implement ODC::DataPort functions for UI
	const Json::Value GetCurrentState() const override;
	const Json::Value GetStatistics() const override;

	/// Implement some ODC::IOHandler - parent DNP3Port implements the rest to return NOT_SUPPORTED
	std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) override;

	std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) override;

	std::future<CommandStatus> Event(const ConnectState& state, uint16_t index, const std::string& SenderName) override;

	/// Implement opendnp3::IOutstationApplication
	// Called when a the reset/unreset status of the link layer changes (and on link up)
	void OnStateChange(opendnp3::LinkStatus status) override;
	// Called when a keep alive message (request link status) receives no response
	void OnKeepAliveFailure() override;
	// Called when a keep alive message receives a valid response
	void OnKeepAliveSuccess() override;

	/// Implement opendnp3::ICommandHandler
	void Start() override {}
	void End() override {}
	CommandStatus Select(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex) override { return SupportsT(arCommand, aIndex); }
	CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	CommandStatus Select(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	CommandStatus Operate(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	CommandStatus Select(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	CommandStatus Operate(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	CommandStatus Select(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	CommandStatus Operate(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	CommandStatus Select(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	CommandStatus Operate(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}

private:
	asiodnp3::IOutstation* pOutstation;
	void LinkStatusListener(opendnp3::LinkStatus status);

	template<typename T> std::future<CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);
	template<typename T, typename Q> std::future<CommandStatus> EventQ(Q& meas, uint16_t index, const std::string& SenderName);

	template<typename T> CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> CommandStatus PerformT(T& arCommand, uint16_t aIndex);
};

#endif /* DNP3SERVERPORT_H_ */
