/*	opendatacon
 *
 *	Copyright (c) 2018:
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
 * MD3Port.h
 *
 *  Created on: 1/2/2018
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - derived from MD3 version
 */

#ifndef MD3PORT_H_
#define MD3PORT_H_

#include <opendatacon/DataPort.h>
#include <opendnp3/gen/LinkStatus.h>
#include <opendnp3/outstation/ICommandHandler.h>
#include "MD3PortConf.h"

using namespace odc;

class MD3Port: public DataPort
{
public:
	MD3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	void Enable() override =0;
	void Disable() override =0;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override =0;

	//Override DataPort for UI
	//const Json::Value GetStatus() const override;

	//so the compiler won't warn we're hiding the base class overload we still want to use
	using DataPort::Event;

	std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }

	std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }

	std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }
	std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) override { return IOHandler::CommandFutureNotSupported(); }

	void ProcessElements(const Json::Value& JSONRoot) override;

	int Limit(int val, int max);

protected:
	bool isServer;
	std::unique_ptr<TCPSocketManager<std::string>> pSockMan;
	TCPClientServer ClientOrServer();
};

#endif /* MD3PORT_H_ */
