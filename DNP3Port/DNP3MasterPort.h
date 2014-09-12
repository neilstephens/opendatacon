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
#include <opendnp3/master/IPollListener.h>
#include <opendnp3/master/CommandResponse.h>
#include <opendnp3/app/IterableBuffer.h>

#include "DNP3Port.h"

using namespace opendnp3;

class DNP3MasterPort: public DNP3Port, public opendnp3::ISOEHandler, public opendnp3::IPollListener
{
public:
	DNP3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
		DNP3Port(aName, aConfFilename, aConfOverrides),
		stack_enabled(false),
		assign_class_sent(false)
	{};

	void Enable();
	void Disable();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	//implement ISOEHandler
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<Binary, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<DoubleBitBinary, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<Analog, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<Counter, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<FrozenCounter, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<BinaryOutputStatus, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<AnalogOutputStatus, uint16_t>>& meas);
	void OnReceiveHeader(const HeaderRecord& header, TimestampMode tsmode, const IterableBuffer<IndexedValue<OctetString, uint16_t>>& meas);
	template<typename T> void LoadT(const IterableBuffer<IndexedValue<T, uint16_t>>& meas);

    ///
    Json::Value GetStatistics(const ParamCollection& params) const override;
    
	//Implement some IOHandler - parent DNP3Port implements the rest to return NOT_SUPPORTED
	std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(bool connected, uint16_t index, const std::string& SenderName);
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& arCommand, uint16_t index, const std::string& SenderName);

protected:
	//implement IPollListener
	void OnStateChange(opendnp3::PollState state);
	//implement transactable
	void Start() override final {}
	void End() override final {}

private:
	asiodnp3::IMaster* pMaster;
    
	bool stack_enabled;
	bool assign_class_sent;
	opendnp3::MasterScan IntegrityScan;
	void SendAssignClass(std::promise<opendnp3::CommandStatus> cmd_promise);
	void StateListener(opendnp3::ChannelState state);
};

#endif /* DNP3CLIENTPORT_H_ */
