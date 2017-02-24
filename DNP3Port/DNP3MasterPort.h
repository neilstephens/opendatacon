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

#include <unordered_map>
#include <opendnp3/master/ISOEHandler.h>
#include <opendnp3/app/parsing/ICollection.h>
#include "DNP3Port.h"
#include "DNP3PortConf.h"

using namespace opendnp3;

class DNP3MasterPort: public DNP3Port, public opendnp3::ISOEHandler, public opendnp3::IMasterApplication
{
public:
	DNP3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
		DNP3Port(aName, aConfFilename, aConfOverrides),
		pMaster(nullptr),
		stack_enabled(false),
		assign_class_sent(false)
	{}
	~DNP3MasterPort();

protected:
	/// Implement ODC::DataPort
	void Enable() override;
	void Disable() override;
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL) override;
	const Json::Value GetStatistics() const override;

	// Implement DNP3Port
	void OnLinkDown() override;
	TCPClientServer ClientOrServer() override;
    
	/// Implement some ODC::IOHandler - parent DNP3Port implements the rest to return NOT_SUPPORTED
	std::future<CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) override;

	std::future<CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) override;

	/// Implement opendnp3::ISOEHandler
	void Start() override {}
	void End() override {}

	void Process(const HeaderInfo& info, const ICollection<Indexed<Binary> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<DoubleBitBinary> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<Analog> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<Counter> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<FrozenCounter> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<BinaryOutputStatus> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<AnalogOutputStatus> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<OctetString> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<TimeAndInterval> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<BinaryCommandEvent> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<AnalogCommandEvent> >& meas) override;
	void Process(const HeaderInfo& info, const ICollection<Indexed<SecurityStat> >& meas) override;

	/// Implement opendnp3::IMasterApplication
	virtual void OnReceiveIIN(const opendnp3::IINField& iin) override final {}
	virtual void OnTaskStart(opendnp3::MasterTaskType type, opendnp3::TaskId id) override final {}
	virtual void OnTaskComplete(const opendnp3::TaskInfo& info) override final {}
	virtual bool AssignClassDuringStartup() override final { return false; }
	virtual void ConfigureAssignClassRequest(const opendnp3::WriteHeaderFunT& fun) override final {}
	virtual openpal::UTCTimestamp Now() override final
	{
		auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return openpal::UTCTimestamp(time);
	}

	// Called when a the reset/unreset status of the link layer changes (and on link up)
	void OnStateChange(opendnp3::LinkStatus status) override;
	// Called when a keep alive message (request link status) receives no response
	void OnKeepAliveFailure() override;
	// Called when a keep alive message receives a valid response
	void OnKeepAliveSuccess() override;
    
private:
	asiodnp3::IMaster* pMaster;

	bool stack_enabled;
	bool assign_class_sent;
	opendnp3::MasterScan IntegrityScan;
	void SendAssignClass(std::promise<CommandStatus> cmd_promise);
	void LinkStatusListener(opendnp3::LinkStatus status);
	template<typename T>
	inline void DoOverrideControlCode(T& arCommand){}
	void PortUp();
	void PortDown();
	inline void EnableStack()
	{
		pMaster->Enable();
		stack_enabled = true;
		//TODO: this scan isn't needed if we remember quality on PortDown() and reinstate in PortUp();
		IntegrityScan.Demand();
	}
	inline void DisableStack()
	{
		stack_enabled = false;
		pMaster->Disable();
	}
	inline void DoOverrideControlCode(opendnp3::ControlRelayOutputBlock& arCommand)
	{
		DNP3PortConf* pConf = static_cast<DNP3PortConf*>(this->pConf.get());
		if(pConf->pPointConf->OverrideControlCode != opendnp3::ControlCode::UNDEFINED)
		{
			arCommand.functionCode = pConf->pPointConf->OverrideControlCode;
			arCommand.rawCode = opendnp3::ControlCodeToType(arCommand.functionCode);
		}

	}
    
	template<typename T> std::future<CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);
	template<typename T> void LoadT(const ICollection<Indexed<T> >& meas);
};

#endif /* DNP3CLIENTPORT_H_ */
