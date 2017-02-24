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

using namespace odc;

class DNP3Port: public DataPort
{
public:
	DNP3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	virtual ~DNP3Port(){}

	virtual void Enable()=0;
	virtual void Disable()=0;
	virtual void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)=0;

	void StateListener(ChannelState state);

	//Override DataPort for UI
	const Json::Value GetStatus() const override;

	virtual std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	virtual std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	/// Quality change events
	virtual std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	void ProcessElements(const Json::Value& JSONRoot);

protected:
	asiodnp3::IChannel* GetChannel(asiodnp3::DNP3Manager& DNP3Mgr);

	asiodnp3::IChannel* pChannel;
	static std::unordered_map<std::string, asiodnp3::IChannel*> Channels;
	opendnp3::LinkStatus status;
	bool link_dead;
	bool channel_dead;

	virtual void OnLinkDown()=0;
	virtual TCPClientServer ClientOrServer()=0;
};

#endif /* DNP3PORT_H_ */
