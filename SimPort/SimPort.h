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

class SimPort: public DataPort
{
public:
	//Implement DataPort interface
	SimPort(std::string Name, std::string File, const Json::Value Overrides);
	void Enable() final;
	void Disable() final;
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL) final;
	void ProcessElements(const Json::Value& JSONRoot) final;
	std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) final;

	//Implement Event handlers from IOHandler

	// measurement events
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) final;

	// change of quality events
	std::future<opendnp3::CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) final;

	// control events
	std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) final;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) final;

private:
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	typedef std::shared_ptr<Timer_t> pTimer_t;
	std::vector<pTimer_t> Timers;
	void SpawnEvent(std::shared_ptr<opendnp3::Analog> pMean, double std_dev, unsigned int interval, size_t index, pTimer_t pTimer, rand_t seed);
	void PortUp();
	void PortDown();

	std::unique_ptr<asio::strand> pEnableDisableSync;
	bool enabled;
};

#endif // SIMPORT_H
