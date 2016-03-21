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
 * SNMPPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */
#include "SNMPPort.h"

std::unordered_map<uint16_t, std::weak_ptr<Snmp_pp::Snmp>> SNMPPort::SnmpSessions;
std::unordered_map<uint16_t, std::weak_ptr<Snmp_pp::Snmp>> SNMPPort::SnmpTrapSessions;
std::unordered_map<std::string, SNMPPort*> SNMPPort::SourcePortMap;
std::unordered_set<SNMPPort*> SNMPPort::PortSet;

SNMPPort::SNMPPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	stack_enabled(false)
{
	//the creation of a new SNMPPortConf will get the point details
	pConf.reset(new SNMPPortConf(ConfFilename));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
	
	PortSet.insert(this);
}

SNMPPort::~SNMPPort()
{
	PortSet.erase(this);
}

void SNMPPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	if(!JSONRoot["IP"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();

	if(!JSONRoot["Port"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(!JSONRoot["SourcePort"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.SourcePort = JSONRoot["SourcePort"].asUInt();

	if(!JSONRoot["TrapPort"].isNull())
		static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.TrapPort = JSONRoot["TrapPort"].asUInt();
	
	if(!JSONRoot["ServerType"].isNull())
	{
		if (JSONRoot["ServerType"].asString()=="PERSISTENT")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if (JSONRoot["ServerType"].asString()=="ONDEMAND")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else //if (JSONRoot["ServerType"].asString()=="MANUAL")
			static_cast<SNMPPortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::MANUAL;
	}
}

std::shared_ptr<Snmp_pp::Snmp> SNMPPort::GetSesion(uint16_t UdpPort)
{
	auto session = SnmpSessions[UdpPort].lock();
	//create a new session if one doesn't already exist
	if (session)
	{
		return session;
	}

	bool enable_ipv6 = true;
	int status;
	session.reset(new Snmp_pp::Snmp(status, UdpPort, enable_ipv6));
	if ( status != SNMP_CLASS_SUCCESS) {
		std::cout << "SNMP++ Session Create Fail, " << session->error_msg(status) << std::endl;
		return nullptr;
	}
	SnmpSessions[UdpPort] = session;
	Snmp_pp::DefaultLog::log()->set_profile("off");
	session->start_poll_thread(10);
	return session;
}

std::shared_ptr<Snmp_pp::Snmp> SNMPPort::GetTrapSession(uint16_t UdpPort, const std::string& SourceAddr )
{
	auto session = SnmpTrapSessions[UdpPort].lock();
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
	SnmpTrapSessions[UdpPort] = session;
	Snmp_pp::DefaultLog::log()->set_profile("off");
	session->start_poll_thread(10);
	
	Snmp_pp::OidCollection oidc;
	Snmp_pp::TargetCollection targetc;
	
	status = session->notify_register(oidc, targetc, SNMPPort::SnmpCallback, nullptr);
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
