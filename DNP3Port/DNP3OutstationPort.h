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
#include "DNP3Port.h"
#include <unordered_map>
#include <opendnp3/outstation/ICommandHandler.h>

class DNP3OutstationPort: public DNP3Port, public opendnp3::ICommandHandler, public opendnp3::IOutstationApplication
{
public:
	DNP3OutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~DNP3OutstationPort() override;

protected:
	/// Implement ODC::DataPort
	void Enable() override;
	void Disable() override;
	void Build() override;

	// Implement DNP3Port
	void OnLinkDown() override;
	TCPClientServer ClientOrServer() override;

	/// Implement ODC::DataPort functions for UI
	const Json::Value GetCurrentState() const override;
	const Json::Value GetStatistics() const override;

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
	opendnp3::CommandStatus Select(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex) override { return SupportsT(arCommand, aIndex); }
	opendnp3::CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}

	//Implement IOHandler
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

private:
	Json::Value state;
	std::unique_ptr<asio::io_service::strand> pStateSync;
	std::shared_ptr<asiodnp3::IOutstation> pOutstation;
	void LinkStatusListener(opendnp3::LinkStatus status);

	template<typename T> void EventT(T meas, uint16_t index);
	template<typename T, typename Q> void EventQ(Q qual, uint16_t index, opendnp3::FlagsType FT);

	template<typename T> opendnp3::CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> opendnp3::CommandStatus PerformT(T& arCommand, uint16_t aIndex);

	void SetState(const std::string& type, const std::string& index, const std::string& payload);
};

#endif /* DNP3SERVERPORT_H_ */
