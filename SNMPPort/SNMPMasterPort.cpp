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
 * SNMPClientPort.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <opendnp3/LogLevels.h>
#include <thread>
#include <chrono>
#include <stdlib.h>     /* atof */

#include <opendnp3/app/MeasurementTypes.h>
#include "SNMPMasterPort.h"
#include <array>

SNMPMasterPort::~SNMPMasterPort()
{
	Disable();
}

void SNMPMasterPort::Enable()
{
	if(enabled) return;
	enabled = true;

	pTCPRetryTimer.reset(new Timer_t(*pIOS));
	PollScheduler.reset(new ASIOScheduler(*pIOS));

	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());

	// Only change stack state if it is a persistent server
	if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
	{
		this->Connect();
	}
}

void SNMPMasterPort::Connect()
{
	if(!enabled) return;
	if (stack_enabled) return;

	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());


	/* set up SNMP */

	PollScheduler->Clear();
	for(auto pg : pConf->pPointConf->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=](){
			this->DoPoll(id);
		};
		PollScheduler->Add(pg.second.pollrate, action);
	}

	PollScheduler->Start();
}

void SNMPMasterPort::Disable()
{
	Disconnect();
	enabled = false;
}

void SNMPMasterPort::Disconnect()
{
	if (!stack_enabled) return;
	stack_enabled = false;

	//cancel the timers (otherwise it would tie up the io_service on shutdown)
	pTCPRetryTimer->cancel();
	PollScheduler->Stop();

	/* tear down SNMP */

	//Update the quality of point
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	for(auto IOHandler_pair : Subscribers)
	{
		// Iterate over all Oid and issue COMM_LOST
		for(auto Oid : pConf->pPointConf->BinaryIndicies)
		{
			IOHandler_pair.second->Event(opendnp3::BinaryQuality::COMM_LOST, Oid.index, this->Name);
		}
		for(auto Oid : pConf->pPointConf->AnalogIndicies)
		{
			IOHandler_pair.second->Event(opendnp3::AnalogQuality::COMM_LOST, Oid.index, this->Name);
		}
	}
}

void SNMPMasterPort::HandleError(int errnum, const std::string& source)
{
	std::string msg = Name + ": " + source + " error: '" + "'";
	auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);
	
	// If not a SNMP error, tear down the connection?
}

void SNMPMasterPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());

	std::string log_id;
	
	log_id = "mast_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);
	
	Snmp_pp::UdpAddress address(pConf->mAddrConf.IP.c_str());      // make a SNMP++ Generic address
	address.set_port(pConf->mAddrConf.Port);
	if ( !address.valid())
	{
		// check validity of address
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed, invalid IP address or hostname'";
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	// SNMP_PP Options
	Snmp_pp::snmp_version version=Snmp_pp::version1;         // default is v1
	int retries=1;                                  // default retries is 1
	int timeout=100;                                // default is 1 second
	u_short port=161;                               // default snmp port is 161
	Snmp_pp::OctetStr community("public");          // community name
	
	/*
	 #ifdef _SNMPv3
	 OctetStr privPassword("");
	 OctetStr authPassword("");
	 OctetStr securityName("");
	 int securityModel = SNMP_SECURITY_MODEL_USM;
	 int securityLevel = SNMP_SECURITY_LEVEL_AUTH_PRIV;
	 OctetStr contextName("");
	 OctetStr contextEngineID("");
	 long authProtocol = SNMP_AUTHPROTOCOL_NONE;
	 long privProtocol = SNMP_PRIVPROTOCOL_NONE;
	 v3MP *v3_MP;
	 #endif
	 */
	
	// Create session
	int status;
	snmp = new Snmp_pp::Snmp(status, 0, (address.get_ip_version() == Snmp_pp::Address::version_ipv6));

	if ( status != SNMP_CLASS_SUCCESS) {
		std::cout << "SNMP++ Session Create Fail, " << snmp->error_msg(status) << std::endl;
		return;
	}
	
	/*
	 //---------[ init SnmpV3 ]--------------------------------------------
	 #ifdef _SNMPv3
	 if (version == version3) {
	 const char *engineId = "snmpGet";
	 const char *filename = "snmpv3_boot_counter";
	 unsigned int snmpEngineBoots = 0;
	 int status;
	 
	 status = getBootCounter(filename, engineId, snmpEngineBoots);
	 if ((status != SNMPv3_OK) && (status < SNMPv3_FILEOPEN_ERROR))
	 {
	 cout << "Error loading snmpEngineBoots counter: " << status << endl;
	 return 1;
	 }
	 snmpEngineBoots++;
	 status = saveBootCounter(filename, engineId, snmpEngineBoots);
	 if (status != SNMPv3_OK)
	 {
	 cout << "Error saving snmpEngineBoots counter: " << status << endl;
	 return 1;
	 }
	 
	 int construct_status;
	 v3_MP = new v3MP(engineId, snmpEngineBoots, construct_status);
	 if (construct_status != SNMPv3_MP_OK)
	 {
	 cout << "Error initializing v3MP: " << construct_status << endl;
	 return 1;
	 }
	 
	 USM *usm = v3_MP->get_usm();
	 usm->add_usm_user(securityName,
	 authProtocol, privProtocol,
	 authPassword, privPassword);
	 }
	 else
	 {
	 // MUST create a dummy v3MP object if _SNMPv3 is enabled!
	 int construct_status;
	 v3_MP = new v3MP("dummy", 0, construct_status);
	 }
	 #endif
	*/
	
	
	address.set_port(port);
	/*
	 #ifdef _SNMPv3
	 Snmp_pp::UTarget utarget( address);
	 
	 if (version == version3) {
		utarget.set_version( version);          // set the SNMP version SNMPV1 or V2 or V3
		utarget.set_retry( retries);            // set the number of auto retries
		utarget.set_timeout( timeout);          // set timeout
		utarget.set_security_model( securityModel);
		utarget.set_security_name( securityName);
		pdu.set_security_level( securityLevel);
		pdu.set_context_name (contextName);
		pdu.set_context_engine_id(contextEngineID);
	 }
	 else {
	 #endif
	 */
	Snmp_pp::CTarget *ctarget =  new Snmp_pp::CTarget(address);             // make a target using the address
	ctarget->set_version( version);         // set the SNMP version SNMPV1 or V2
	ctarget->set_retry( retries);           // set the number of auto retries
	ctarget->set_timeout( timeout);         // set timeout
	ctarget->set_readcommunity( community); // set the read community name
	
	/*
	 #ifdef _SNMPv3
	 if (version == Snmp_pp::version3)
	 target = &utarget;
	 else
	 #endif
	 */
	target = ctarget;
}

void SNMPMasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;

	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	//--------[ build up SNMP++ object needed ]-------------------------------
	Snmp_pp::Pdu pdu;                               // construct a Pdu object
	Snmp_pp::Vb vb;                                 // construct a Vb object
	
	//---------[ make Oid object to retrieve ]-------------------------------
	std::vector<OidToEvent*> indexes;
	for(auto& Oid : pConf->pPointConf->BinaryIndicies)
	{
		if (pollgroup && (Oid.pollgroup != pollgroup))
			continue;
		
		vb.set_oid(Oid.oid);                       // set the Oid portion of the Vb
		pdu += vb;                             // add the vb to the Pdu
		indexes.push_back(&Oid);          // add backreference to index
	}
	for(auto& Oid : pConf->pPointConf->AnalogIndicies)
	{
		if (pollgroup && (Oid.pollgroup != pollgroup))
			continue;
		
		vb.set_oid(Oid.oid);                       // set the Oid portion of the Vb
		pdu += vb;                             // add the vb to the Pdu
		indexes.push_back(&Oid);          // add backreference to index
	}

	/*
	 if debug log level
	 #ifdef _SNMPv3
	 if (version == version3)
	 cout << endl
	 << "securityName= " << securityName.get_printable()
	 << ", securityLevel= " << securityLevel
	 << ", securityModel= " << securityModel << endl
	 << "contextName= " << contextName.get_printable()
	 << ", contextEngineID= " << contextEngineID.get_printable()
	 << endl;
	 else
	 #endif*/
	
	
	int status = snmp->get( pdu, *target);
	
	if (status == SNMP_CLASS_SUCCESS)
	{
		for (int i = 0; i < pdu.get_vb_count(); i++)
		{
			pdu.get_vb(vb, i);
			/*
			 if debug log level
			 #ifdef _SNMPv3
			 if (pdu.get_type() == REPORT_MSG) {
			 cout << "Received a report pdu: " << snmp.error_msg(vb.get_printable_oid()) << endl;
			 }
			 #endif
			 
			 std::cout << "Oid = " << vb.get_printable_oid() << " Value = " << vb.get_printable_value() << " Syntax = " << vb.get_syntax() << std::endl;
			 */
			
			for(auto IOHandler_pair : Subscribers)
			{
				indexes.at(i)->GenerateEvent(*IOHandler_pair.second, vb, this->Name);
			}
		}
	}
	else
	{
		std::cout << "SNMP++ Get Error, " << snmp->error_msg( status) << " (" << status <<")" << std::endl;
	}
}

//Implement some IOHandler - parent SNMPPort implements the rest to return NOT_SUPPORTED
std::future<opendnp3::CommandStatus> SNMPMasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());

	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	auto cmd_future = cmd_promise.get_future();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_future;
	}

	//something upstream has connected
	if(state == ConnectState::CONNECTED)
	{
		// Only change stack state if it is an on demand server
		if (pConf->mAddrConf.ServerType == server_type_t::ONDEMAND)
		{
			this->Connect();
		}
	}

	cmd_promise.set_value(opendnp3::CommandStatus::SUCCESS);
	return cmd_future;
}



