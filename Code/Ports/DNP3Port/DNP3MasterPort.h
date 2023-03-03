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
 * DNP3ClientPort.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3CLIENTPORT_H_
#define DNP3CLIENTPORT_H_
#include "DNP3Port.h"
#include "DNP3PortConf.h"
#include "CommsRideThroughTimer.h"
#include <unordered_map>
#include <opendnp3/master/ISOEHandler.h>
#include <opendnp3/master/IMasterApplication.h>
#include <opendnp3/app/parsing/ICollection.h>

class DNP3MasterPort: public DNP3Port, public opendnp3::ISOEHandler, public opendnp3::IMasterApplication
{
public:
	DNP3MasterPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
		DNP3Port(aName, aConfFilename, aConfOverrides),
		pMaster(nullptr),
		stack_enabled(false),
		assign_class_sent(false),
		IntegrityScan(nullptr),
		pCommsRideThroughTimer(nullptr)
	{}
	~DNP3MasterPort() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

protected:
	/// Implement ODC::DataPort
	void Enable() override;
	void Disable() override;
	void Build() override;
	const Json::Value GetStatistics() const override;

	// Implement DNP3Port
	TCPClientServer ClientOrServer() override;
	void LinkDeadnessChange(LinkDeadness from, LinkDeadness to) override;
	void ChannelWatchdogTrigger(bool on) override;
	bool channel_stayed_up = false; //keep track of the case where the link goes down and back up without the channel going down

	void BeginFragment(const opendnp3::ResponseInfo& info) override {}
	void EndFragment(const opendnp3::ResponseInfo& info) override {}

	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::DoubleBitBinary> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Counter> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::FrozenCounter> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryOutputStatus> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogOutputStatus> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent> >& meas) override;
	void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values) override;

	/// Implement opendnp3::IMasterApplication
	void OnTaskStart(opendnp3::MasterTaskType type, opendnp3::TaskId id) final
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: OnTaskStart(Type {}, ID {}) called.", Name, opendnp3::MasterTaskTypeSpec::to_human_string(type), id.GetId());
	}
	void OnTaskComplete(const opendnp3::TaskInfo& info) final
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: OnTaskComplete(Type {}, ID {}, Res {}) called.", Name, opendnp3::MasterTaskTypeSpec::to_human_string(info.type), info.id.GetId(), opendnp3::TaskCompletionSpec::to_human_string(info.result));
	}
	bool AssignClassDuringStartup() final
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->trace("{}: AssignClassDuringStartup() called.", Name);
		return false;
	}
	void ConfigureAssignClassRequest(const opendnp3::WriteHeaderFunT& fun) final
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: ConfigureAssignClassRequest() called.", Name);
	}
	opendnp3::UTCTimestamp Now() final
	{
		auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return opendnp3::UTCTimestamp(time);
	}

	// Called when a the reset/unreset status of the link layer changes (and on link up)
	void OnStateChange(opendnp3::LinkStatus status) override;
	// Called when a keep alive message (request link status) receives no response
	void OnKeepAliveFailure() override;
	// Called when a keep alive message receives a valid response
	void OnKeepAliveSuccess() override;
	// Called every application header receipt
	void OnReceiveIIN(const opendnp3::IINField& iin) override;

private:
	std::shared_ptr<opendnp3::IMaster> pMaster;

	std::atomic_bool stack_enabled;
	bool assign_class_sent;
	std::shared_ptr<opendnp3::IMasterScan> IntegrityScan;
	std::shared_ptr<CommsRideThroughTimer> pCommsRideThroughTimer;

	void UpdateCommsPoint(bool isFailed);
	void SetCommsGood();
	void SetCommsFailed();
	void CommsHeartBeat(bool isFailed);
	void LinkStatusListener(opendnp3::LinkStatus status);
	template<typename T>
	inline void DoOverrideControlCode(T& arCommand){}
	void PortUp();
	void PortDown();
	inline void EnableStack()
	{
		pMaster->Enable();
		stack_enabled = true;

		auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		//An integrity scan will happen on link up, unless quality doesn't need refreshing
		//in which case, make sure it happens on start
		if(pConf->pPointConf->SetQualityOnLinkStatus == false && IntegrityScan)
		{
			IntegrityScan->Demand();
		}
	}
	inline void DisableStack()
	{
		stack_enabled = false;
		pMaster->Disable();
	}
	inline void DoOverrideControlCode(opendnp3::ControlRelayOutputBlock& arCommand)
	{
		DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if(pConf->pPointConf->OverrideControlCode.first != opendnp3::OperationType::Undefined)
		{
			arCommand.opType = pConf->pPointConf->OverrideControlCode.first;
			arCommand.tcc = pConf->pPointConf->OverrideControlCode.second;
			arCommand.rawCode = opendnp3::OperationTypeSpec::to_type(arCommand.opType);
		}

	}

	template<typename T> void LoadT(const opendnp3::ICollection<opendnp3::Indexed<T> >& meas);
};

template<> void DNP3MasterPort::LoadT<opendnp3::OctetString>(const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString> >& meas);

#endif /* DNP3CLIENTPORT_H_ */
