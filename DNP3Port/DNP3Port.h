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
 * DNP3Port.h
 *
 *  Created on: 18/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3PORT_H_
#define DNP3PORT_H_

#include <opendatacon/DataPort.h>
#include <opendnp3/gen/LinkStatus.h>
#include "DNP3PortConf.h"
#include "DNP3Log2spdlog.h"

using namespace odc;

class DNP3Port: public DataPort
{
public:
	DNP3Port(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~DNP3Port() override {}

	void Enable() override =0;
	void Disable() override =0;
	void BuildOrRebuild() override =0;

	void StateListener(ChannelState state);

	//Override DataPort for UI
	const Json::Value GetStatus() const override;

	//so the compiler won't warn we're hiding the base class overload we still want to use
	using DataPort::Event;

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	/// Quality change events
	void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void ProcessElements(const Json::Value& JSONRoot) override;

protected:
	asiodnp3::IChannel* GetChannel();

	asiodnp3::IChannel* pChannel;
	opendnp3::LinkStatus status;
	bool link_dead;
	bool channel_dead;

	static std::unordered_map<std::string, asiodnp3::IChannel*> Channels;

	virtual void OnLinkDown()=0;
	virtual TCPClientServer ClientOrServer()=0;

private:
	static asiodnp3::DNP3Manager IOMgr;
	static DNP3Log2spdlog DNP3LogHandler;
	static std::atomic_flag log_subscribed;
};

#endif /* DNP3PORT_H_ */
