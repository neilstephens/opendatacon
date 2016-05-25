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
 * SNMPMasterPort.h
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray <alan@atmurray.net>
 */

#ifndef SNMPCLIENTPORT_H_
#define SNMPCLIENTPORT_H_

#include <queue>
#include <opendnp3/master/ISOEHandler.h>

#include "SNMPPort.h"
#include <opendatacon/ASIOScheduler.h>

using namespace opendnp3;

#include <utility>

class SNMPMasterPort: public SNMPPort
{
public:
	SNMPMasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~SNMPMasterPort();

	// Implement SNMPPort
	void Enable();
	void Disable();
	void Connect();
	void Disconnect();
	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

	// Implement some IOHandler - parent SNMPPort implements the rest to return NOT_SUPPORTED
	std::future<opendnp3::CommandStatus> ConnectionEvent(ConnectState state, const std::string& SenderName);

private:
	template<class T>
	opendnp3::CommandStatus WriteObject(const T& command, uint16_t index);

	void DoPoll(uint32_t pollgroup);

private:
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	virtual void ProcessResponse(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessGet(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessTrap(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	virtual void ProcessReport(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target);
	
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTCPRetryTimer;
	std::unique_ptr<ASIOScheduler> PollScheduler;
};

#endif /* SNMPCLIENTPORT_H_ */

