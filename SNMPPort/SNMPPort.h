/*	opendatacon
 *
 *	Copyright (c) 2016:
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
 * SNMPPort.h
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */

#ifndef SNMPPORT_H_
#define SNMPPORT_H_

#include <opendatacon/DataPort.h>
#include <libsnmp.h>
#include <snmp_pp/snmp_pp.h>
#include <unordered_set>
#include <unordered_map>

#include "SNMPPortConf.h"

class SNMPPort: public DataPort
{
public:
	SNMPPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~SNMPPort();

	virtual void Enable()=0;
	virtual void Disable()=0;
	virtual void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)=0;

	using DataPort::Event;
	
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	virtual std::future<opendnp3::CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const CounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }
	virtual std::future<opendnp3::CommandStatus> Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName) { return IOHandler::CommandFutureNotSupported(); }

	void ProcessElements(const Json::Value& JSONRoot);
	
	virtual void SnmpCallback(int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target) = 0;

protected:
	bool stack_enabled;
	std::shared_ptr<Snmp_pp::Snmp> snmp;
	std::shared_ptr<Snmp_pp::Snmp> snmp_trap;
};

#endif /* SNMPPORT_H_ */
