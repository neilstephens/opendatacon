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
#include "AppIINFlags.h"
#include <unordered_map>
#include <opendnp3/outstation/ICommandHandler.h>
#include <opendnp3/outstation/ApplicationIIN.h>
#include <opendnp3/util/UTCTimestamp.h>

class DNP3OutstationPortCollection;

class DNP3OutstationPort: public DNP3Port, public opendnp3::ICommandHandler, public opendnp3::IOutstationApplication
{
	friend class DNP3OutstationPortCollection;
public:
	DNP3OutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~DNP3OutstationPort() override;

protected:
	/// Implement ODC::DataPort
	void Enable() override;
	void Disable() override;
	void Build() override;

	// Implement DNP3Port
	void ExtendCurrentState(Json::Value& state) const override;
	TCPClientServer ClientOrServer() override;
	void LinkDeadnessChange(LinkDeadness from, LinkDeadness to) override;
	void ChannelWatchdogTrigger(bool on) override;
	std::atomic<msSinceEpoch_t> last_link_down_time = msSinceEpoch();

	std::pair<std::string,const IUIResponder*> GetUIResponder() final;

	/// Implement ODC::DataPort functions for UI
	const Json::Value GetStatistics() const override;

	/// Implement opendnp3::IOutstationApplication
	// Called when a the reset/unreset status of the link layer changes (and on link up)
	void OnStateChange(opendnp3::LinkStatus status) override;
	// Called when a keep alive message (request link status) receives no response
	void OnKeepAliveFailure() override;
	// Called when a keep alive message receives a valid response
	void OnKeepAliveSuccess() override;
	// Support for master setting time reference for events
	inline bool SupportsWriteAbsoluteTime() override
	{
		return true;
	}
	bool WriteAbsoluteTime(const opendnp3::UTCTimestamp& timestamp) override;
	opendnp3::ApplicationIIN GetApplicationIIN() const override;

	void LinkUpCheck();
	std::shared_ptr<asio::steady_timer> pLinkUpCheckTimer = pIOS->make_steady_timer();

	/// Implement opendnp3::ICommandHandler
	void Begin() override {}
	void End() override {}

	opendnp3::CommandStatus Select(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex) override { return SupportsT(arCommand, aIndex); }
	opendnp3::CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt16& arCommand, uint16_t aIndex, opendnp3::IUpdateHandler& handler,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt32& arCommand, uint16_t aIndex, opendnp3::IUpdateHandler& handler,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t aIndex, opendnp3::IUpdateHandler& handler,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}
	opendnp3::CommandStatus Select(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex) override {return SupportsT(arCommand,aIndex);}
	opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t aIndex, opendnp3::IUpdateHandler& handler,opendnp3::OperateType op_type) override {return PerformT(arCommand,aIndex);}

	//Implement IOHandler
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

private:
	std::shared_ptr<opendnp3::IOutstation> pOutstation;
	std::atomic<int64_t> master_time_offset;
	mutable std::atomic<AppIINFlags> IINFlags;
	std::atomic<msSinceEpoch_t> last_time_sync;
	std::shared_ptr<DNP3OutstationPortCollection> PeerCollection;
	void LinkStatusListener(opendnp3::LinkStatus status);

	void UpdateQuality(const EventType event_type, const uint16_t index, const QualityFlags qual);
	template<typename T> void EventT(T meas, uint16_t index);
	template<> void EventT<opendnp3::OctetString>(opendnp3::OctetString meas, uint16_t index);
	template<typename T> void EventT(T qual, uint16_t index, opendnp3::FlagsType FT);

	template<typename T> opendnp3::CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> opendnp3::CommandStatus PerformT(T& arCommand, uint16_t aIndex);

	void SetIINFlags(const AppIINFlags& flags) const;
	void SetIINFlags(const std::string& flags) const;
	void ClearIINFlags(const AppIINFlags& flags) const;
	void ClearIINFlags(const std::string& flags) const;
};

#endif /* DNP3SERVERPORT_H_ */
