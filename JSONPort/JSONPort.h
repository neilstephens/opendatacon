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
 * JSONDataPort.h
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONDATAPORT_H_
#define JSONDATAPORT_H_

#include <unordered_map>
#include <opendatacon/DataPort.h>
#include <opendatacon/TCPSocketManager.h>
#include "JSONPortConf.h"

using namespace odc;

class JSONPort: public DataPort
{
public:
	JSONPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool aisServer);

	void ProcessElements(const Json::Value& JSONRoot) override;

	void Enable() override;
	void Disable() override;

	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	template<typename T> void EventT(const T& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback);
	template<typename T> void EventQ(const T& qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback);

	//so the compiler won't warn we're hiding the base class overload we still want to use
	using DataPort::Event;

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	void Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void ConnectionEvent(ConnectState state, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

	void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;
	void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override;

private:
	bool isServer;
	std::unique_ptr<TCPSocketManager<std::string>> pSockMan;
	void SocketStateHandler(bool state);
	void ReadCompletionHandler(buf_t& readbuf);
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	void ProcessBraced(const std::string& braced);
	template<typename T> void LoadT(T meas, uint16_t index, Json::Value timestamp_val);
};

#endif /* JSONDATAPORT_H_ */
