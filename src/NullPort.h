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
 * NullPort.h
 *
 *  Created on: 04/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef NULLPORT_H_
#define NULLPORT_H_

#include <opendatacon/DataPort.h>

/* The equivalent of /dev/null as a DataPort */
class NullPort : public DataPort
{
private:
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTimer;
public:
	NullPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
		DataPort(aName, aConfFilename, aConfOverrides)
	{};
	void Enable()
	{
		pTimer.reset(new Timer_t(*pIOS, std::chrono::seconds(3)));
		pTimer->async_wait(
			[this](asio::error_code err_code)
		{
			for (auto IOHandler_pair : Subscribers)
			{
				IOHandler_pair.second->Event(ConnectState::PORT_UP, 0, this->Name);
				IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
			}
		});
		return;
	};
	void Disable()
	{
		for (auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::PORT_DOWN, 0, this->Name);
			IOHandler_pair.second->Event(ConnectState::DISCONNECTED, 0, this->Name);
		}
	};
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL){};
	void ProcessElements(const Json::Value& JSONRoot){};

	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };

	std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };

	std::future<opendnp3::CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
	std::future<opendnp3::CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };

	virtual std::future<opendnp3::CommandStatus> Event(ConnectState state, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureSuccess(); };
};

#endif /* NULLPORT_H_ */
