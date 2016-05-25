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
 * SNMPOutstationPort.h
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */

#ifndef SNMPSERVERPORT_H_
#define SNMPSERVERPORT_H_

#include <unordered_map>

#include "SNMPPort.h"

class SNMPOutstationPort: public SNMPPort
{
public:
	SNMPOutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~SNMPOutstationPort();

	void Enable();
	void Disable();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName);
	std::future<opendnp3::CommandStatus> Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName);

	template<typename T> opendnp3::CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> opendnp3::CommandStatus PerformT(T& arCommand, uint16_t aIndex);
	template<typename T> std::future<opendnp3::CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);

	void Connect();
	void Disconnect();

private:
	
	std::future<opendnp3::CommandStatus> SendTrap(OidToEvent& point);
	
	virtual void ProcessResponse(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessGet(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessTrap(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessReport(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	
	void StateListener(opendnp3::ChannelState state);
	
};

#endif /* SNMPSERVERPORT_H_ */
