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
 * SNMPPort.cpp
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */
#include "SNMPPort.h"
#include "SNMPSingleton.h"
#include <opendnp3/LogLevels.h>

SNMPPort::SNMPPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	stack_enabled(false),
	snmp_outbound(nullptr),
	snmp_inbound(nullptr),
	v3_MP(nullptr),
	target(nullptr)
{
	//the creation of a new SNMPPortConf will get the point details
	pConf.reset(new SNMPPortConf(ConfFilename));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

SNMPPort::~SNMPPort()
{
	SNMPSingleton::GetInstance().UnregisterPort(this);
}

void SNMPPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;
	auto portConf = static_cast<SNMPPortConf*>(pConf.get());

	if(!JSONRoot["IP"].isNull())
		portConf->mAddrConf.IP = JSONRoot["IP"].asString();

	if(!JSONRoot["Port"].isNull())
		portConf->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if(!JSONRoot["SourcePort"].isNull())
		portConf->mAddrConf.SourcePort = JSONRoot["SourcePort"].asUInt();

	if(!JSONRoot["TrapPort"].isNull())
		portConf->mAddrConf.TrapPort = JSONRoot["TrapPort"].asUInt();
	
	if(!JSONRoot["version"].isNull())
	{
		auto version = JSONRoot["version"].asString();
		if (version=="1") portConf->version = Snmp_pp::snmp_version::version1;
		else if (version=="2") portConf->version = Snmp_pp::snmp_version::version2c;
		else if (version=="2stern") portConf->version = Snmp_pp::snmp_version::version2stern;
		else if (version=="3") portConf->version = Snmp_pp::snmp_version::version3;
		else {
			std::string msg = "SNMPPort configuration error: Bad SNMP version: ";
			msg += JSONRoot["version"].toStyledString();
			//auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			//pLoggers->Log(log_entry);
			throw std::runtime_error(msg);
		}
	}
	
	if(!JSONRoot["retries"].isNull())
		portConf->retries = JSONRoot["retries"].asUInt();

	if(!JSONRoot["timeout"].isNull())
		portConf->timeout = JSONRoot["timeout"].asUInt();

	if(!JSONRoot["readcommunity"].isNull())
		portConf->readcommunity = JSONRoot["readcommunity"].asCString();

	if(!JSONRoot["writecommunity"].isNull())
		portConf->writecommunity = JSONRoot["writecommunity"].asCString();
	
	if(!JSONRoot["ServerType"].isNull())
	{
		if (JSONRoot["ServerType"].asString()=="PERSISTENT")
			portConf->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if (JSONRoot["ServerType"].asString()=="ONDEMAND")
			portConf->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else //if (JSONRoot["ServerType"].asString()=="MANUAL")
			portConf->mAddrConf.ServerType = server_type_t::MANUAL;
	}
	
	if(!JSONRoot["SecurityName"].isNull())
	{
		portConf->securityName = JSONRoot["SecurityName"].asCString();
	}
	if(!JSONRoot["AuthPassword"].isNull())
	{
		portConf->authPassword = JSONRoot["AuthPassword"].asCString();
	}
	if(!JSONRoot["PrivPassword"].isNull())
	{
		portConf->privPassword = JSONRoot["PrivPassword"].asCString();
	}
	if(!JSONRoot["AuthProtocol"].isNull())
	{
		if (JSONRoot["AuthProtocol"].asString()=="MD5")
			portConf->authProtocol = SNMP_AUTHPROTOCOL_HMACMD5;
		else if (JSONRoot["AuthProtocol"].asString()=="SHA")
			portConf->authProtocol = SNMP_AUTHPROTOCOL_HMACSHA;
		else //if (JSONRoot["AuthProtocol"].asString()=="NONE")
			portConf->authProtocol = SNMP_AUTHPROTOCOL_NONE;
	}
	if(!JSONRoot["PrivProtocol"].isNull())
	{
		if (JSONRoot["PrivProtocol"].asString()=="DES")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_DES;
		else if (JSONRoot["PrivProtocol"].asString()=="3DESEDE")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_3DESEDE;
		else if (JSONRoot["PrivProtocol"].asString()=="IDEA")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_IDEA;
		else if (JSONRoot["PrivProtocol"].asString()=="AES128")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES128;
		else if (JSONRoot["PrivProtocol"].asString()=="AES128W3DESKEYEXT")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES128W3DESKEYEXT;
		else if (JSONRoot["PrivProtocol"].asString()=="AES192")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES192;
		else if (JSONRoot["PrivProtocol"].asString()=="AES192W3DESKEYEXT")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES192W3DESKEYEXT;
		else if (JSONRoot["PrivProtocol"].asString()=="AES256")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES256;
		else if (JSONRoot["PrivProtocol"].asString()=="AES256W3DESKEYEXT")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_AES256W3DESKEYEXT;
		else //if (JSONRoot["PrivProtocol"].asString()=="NONE")
			portConf->privProtocol = SNMP_PRIVPROTOCOL_NONE;
	}
	if(!JSONRoot["ContextName"].isNull())
	{
		portConf->contextName = JSONRoot["ContextName"].asCString();
	}
	if(!JSONRoot["ContextEngineID"].isNull())
	{
		portConf->contextEngineID = JSONRoot["ContextEngineID"].asCString();
	}
	{ /// Implicitly define securityLevel
		if((portConf->authProtocol != SNMP_AUTHPROTOCOL_NONE) && (portConf->privProtocol != SNMP_PRIVPROTOCOL_NONE))
		{
			portConf->securityLevel = SNMP_SECURITY_LEVEL_AUTH_PRIV;
		}
		if((portConf->authProtocol != SNMP_AUTHPROTOCOL_NONE) && (portConf->privProtocol == SNMP_PRIVPROTOCOL_NONE))
		{
			portConf->securityLevel = SNMP_SECURITY_LEVEL_AUTH_NOPRIV;
		}
		if((portConf->authProtocol == SNMP_AUTHPROTOCOL_NONE) && (portConf->privProtocol != SNMP_PRIVPROTOCOL_NONE))
		{
			std::string msg = "SNMPPort configuration error: PrivProtocol provided but no AuthProtocol: ";
			msg += JSONRoot["PrivProtocol"].toStyledString();
			//auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			//pLoggers->Log(log_entry);
			throw std::runtime_error(msg);
		}
	}
}

void SNMPPort::SnmpCallback( int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	Snmp_pp::Vb vb;
	int pdu_error;
	
	// late async requests and traps could come in regardless of the port being enabled or not
	//if (!enabled) return;
	
	if ((reason != SNMP_CLASS_ASYNC_RESPONSE) && (reason != SNMP_CLASS_NOTIFICATION))
	{
		std::string msg = Name + ": " + snmp->error_msg(reason);
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
	
	pdu_error = pdu.get_error_status();
	if (pdu_error){
		std::string msg = Name + ": Response contains error: " + snmp->error_msg(pdu_error);
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
	
	auto pdu_type = pdu.get_type();
	switch (pdu_type)
	{
		case sNMP_PDU_RESPONSE:
			ProcessResponse(snmp, pdu, target);
			break;
			
		case sNMP_PDU_GET:
		case sNMP_PDU_GETNEXT:
		case sNMP_PDU_GETBULK:
			ProcessGet(snmp, pdu, target);
			break;
			
		case sNMP_PDU_V1TRAP:
		case sNMP_PDU_TRAP:
		case sNMP_PDU_SET:
		case sNMP_PDU_INFORM:
		{/// Either solicited or unsolicited data
			ProcessTrap(snmp, pdu, target);
			
			/// SNMP informs require a response, just echo what was received back to acknowledge what we received
			if (pdu_type == sNMP_PDU_INFORM) {
				std::string msg = Name + ": sending response to inform";
				auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::INFO,"", msg.c_str(), -1);
				pLoggers->Log(log_entry);
				snmp->response(pdu, target);
			}
		}
			break;
			
		case sNMP_PDU_REPORT:
		{/// A REPORT operation is an indication of some core SNMP communication error or bootstrapping response.
			ProcessReport(snmp, pdu, target);
		}
			break;
			
		default:
		{
			std::string msg = Name + ": Unknown pdu type received: " + vb.get_printable_oid();
			auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
	}
}

