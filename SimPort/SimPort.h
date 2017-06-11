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
 * SimPort.h
 *
 *  Created on: 29/07/2015
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef SIMPORT_H
#define SIMPORT_H

#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <random>
#include "SimPortManager.h"

using namespace odc;

class SimPort: public DataPort, SyncIOManager<SimPortManager>
{
public:
	//Implement DataPort interface
	SimPort(std::shared_ptr<SimPortManager> Manager, std::string Name, std::string File, const Json::Value Overrides);
	void Enable() final;
	void Disable() final;
	void BuildOrRebuild() final;
	void ProcessElements(const Json::Value& JSONRoot) final;
	std::future<CommandStatus> Event(const ConnectState& state, uint16_t index, const std::string& SenderName) final;

	//Implement Event handlers from IOHandler

	// measurement events
	std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) final;

	// change of quality events
	std::future<CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) final;

	// control events
	std::future<CommandStatus> Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<CommandStatus> Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) final;

private:
	std::vector<Task> Tasks;
	void PortUp();
	void PortDown();

	bool enabled;
	std::mt19937 RandNumGenerator;
};

#endif // SIMPORT_H
