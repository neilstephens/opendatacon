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
 * SNMPSingleton.cpp
 *
 *  Created on: 30/03/2016
 *      Author: Alan Murray
 */

#include "SNMPSingleton.h"

std::shared_ptr<Snmp_pp::v3MP> SNMPSingleton::Getv3MP()
{
	if (auto v3mp = v3mpPtr) return v3mp;
	
	const char *engineId = "opendatacon";
	const char *filename = "snmpv3_boot_counter";
	unsigned int snmpEngineBoots = 0;
	int status;
	
	status = Snmp_pp::getBootCounter(filename, engineId, snmpEngineBoots);
	if ((status != SNMPv3_OK) && (status < SNMPv3_FILEOPEN_ERROR))
	{
		std::cout << "Error loading snmpEngineBoots counter: " << status << std::endl;
		return nullptr;
	}
	snmpEngineBoots++;
	status = Snmp_pp::saveBootCounter(filename, engineId, snmpEngineBoots);
	if (status != SNMPv3_OK)
	{
		std::cout << "Error saving snmpEngineBoots counter: " << status << std::endl;
		return nullptr;
	}
	
	std::shared_ptr<Snmp_pp::v3MP> newv3mp(new Snmp_pp::v3MP(engineId, snmpEngineBoots, status));
	
	if (status != SNMPv3_MP_OK)
	{
		std::cout << "Error initializing v3MP: " << status << std::endl;
		return nullptr;
	}
	v3mpPtr = newv3mp;
	return newv3mp;
}

void SNMPSingleton::RegisterPort(const std::string& sourceIP, uint16_t destPort, SNMPPort* target)
{
	std::string destIPPort = sourceIP + ":" + std::to_string(destPort);
	PortSet.insert(target);
	SourcePortMap[destIPPort] = target;
}

void SNMPSingleton::UnregisterPort(SNMPPort* target)
{
	PortSet.erase(target);
	for(auto destIPPort_target_pair : SourcePortMap)
	{
		if (destIPPort_target_pair.second == target)
		{
			SourcePortMap.erase(destIPPort_target_pair.first);
		}
	}
}

std::shared_ptr<Snmp_pp::Snmp> SNMPSingleton::GetPollSession(uint16_t UdpPort)
{
	auto session = GetInstance().SnmpPollSessions[UdpPort].lock();
	//create a new session if one doesn't already exist
	if (session)
	{
		return session;
	}
	
	bool enable_ipv6 = false;
	int status;
	session.reset(new Snmp_pp::Snmp(status, UdpPort, enable_ipv6));
	if ( status != SNMP_CLASS_SUCCESS) {
		std::cout << "SNMP++ Session Create Fail, " << session->error_msg(status) << std::endl;
		return nullptr;
	}
	GetInstance().SnmpPollSessions[UdpPort] = session;
	
	session->start_poll_thread(10);
	return session;
}

std::shared_ptr<Snmp_pp::Snmp> SNMPSingleton::GetListenSession(uint16_t UdpPort)
{
	auto session = GetInstance().SnmpListenSessions[UdpPort].lock();
	//create a new session if one doesn't already exist
	if (session)
	{
		return session;
	}
	
	bool enable_ipv6 = false;
	int status;
	session.reset(new Snmp_pp::Snmp(status, 0, enable_ipv6));
	if ( status != SNMP_CLASS_SUCCESS) {
		std::cout << "SNMP++ Trap Session Create Fail, " << session->error_msg(status) << std::endl;
		return nullptr;
	}
	session->notify_set_listen_port(UdpPort);
	GetInstance().SnmpListenSessions[UdpPort] = session;
	session->start_poll_thread(10);
	
	Snmp_pp::OidCollection oidc;
	Snmp_pp::TargetCollection targetc;
	
	status = session->notify_register(oidc, targetc, SNMPSingleton::SnmpCallbackNotify, nullptr);
	if (status != SNMP_CLASS_SUCCESS)
	{
		std::string msg = "Stack error: 'SNMP stack trap configuration error'";
		msg += session->error_msg(status);
		//auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		//pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	return session;
}

void SNMPSingleton::SnmpCallbackImpl(int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target, SNMPPort *dest_ptr)
{
	if (GetInstance().PortSet.count(dest_ptr))
	{
		dest_ptr->SnmpCallback(reason, snmp, pdu, target);
	}
	else
	{
		Snmp_pp::UdpAddress from(target.get_address());
		std::cout << "Trap received from deleted source: " << from.get_printable() << std::endl;
		return;
	}
}