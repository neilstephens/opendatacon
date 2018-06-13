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
 * ModbusPort.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef ModbusPORT_H_
#define ModbusPORT_H_

#include <opendatacon/DataPort.h>
#include "ModbusPortConf.h"

using namespace odc;

class ModbusPort: public DataPort
{
public:
	ModbusPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);

	void Enable() override =0;
	void Disable() override =0;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override =0;

	//so the compiler won't warn we're hiding the base class overload we still want to use
	using DataPort::Event;

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void ConnectionEvent(ConnectState state, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }
	void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, std::shared_ptr<std::function<void (CommandStatus status)>> pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	void ProcessElements(const Json::Value& JSONRoot) override;

	static std::unordered_map<std::string, asiodnp3::IChannel*> TCPChannels;
protected:
	bool stack_enabled;
};

#endif /* ModbusPORT_H_ */
