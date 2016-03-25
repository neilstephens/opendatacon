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
	//snmp->stop_poll_thread();
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
			IOHandler_pair.second->Event(opendnp3::BinaryQuality::COMM_LOST, Oid->index, this->Name);
		}
		for(auto Oid : pConf->pPointConf->AnalogIndicies)
		{
			IOHandler_pair.second->Event(opendnp3::AnalogQuality::COMM_LOST, Oid->index, this->Name);
		}
	}
}

void SNMPMasterPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	Snmp_pp::Snmp::socket_startup();  // Initialize socket subsystem
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	std::string log_id;
	log_id = "mast_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);
	
	// Get source session
	snmp = GetSesion(pConf->mAddrConf.SourcePort);
	if (snmp == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed.'";
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	// Create destination target
	Snmp_pp::UdpAddress address(pConf->mAddrConf.IP.c_str());      // make a SNMP++ Generic address
	address.set_port(pConf->mAddrConf.Port);
	if ( !address.valid() )
	{
		// check validity of address
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed, invalid IP address or hostname'";
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	// Get trap session
	snmp_trap = GetTrapSession(pConf->mAddrConf.TrapPort, pConf->mAddrConf.IP);
	if ( snmp_trap == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP trap stack creation failed.'";
		auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	/// SNMPv3 specific configuration
#ifdef _SNMPv3
	//---------[ init SnmpV3 ]--------------------------------------------
	int status;
	if (pConf->version == Snmp_pp::version3) {
		v3_MP = Getv3MP();

		Snmp_pp::USM *usm = v3_MP->get_usm();
		usm->add_usm_user(pConf->securityName,
						  pConf->authProtocol, pConf->privProtocol,
						  pConf->authPassword, pConf->privPassword);
	}
	else
	{
		// MUST create a dummy v3MP object if _SNMPv3 is enabled!
		v3_MP.reset(new Snmp_pp::v3MP("dummy", 0, status));
	}
	
	if (pConf->version == Snmp_pp::version3) {
		Snmp_pp::UTarget *utarget = new Snmp_pp::UTarget(address);             // make a target using the address
		utarget->set_security_model(SNMP_SECURITY_MODEL_USM);
		utarget->set_security_name(pConf->securityName);
		target.reset(utarget);
	}
	else {
#endif
		Snmp_pp::CTarget *ctarget = new Snmp_pp::CTarget(address);             // make a target using the address
		ctarget->set_readcommunity(pConf->readcommunity); // set the read community name (default public)
		ctarget->set_writecommunity(pConf->writecommunity); // set the write community name (default private)
		target.reset(ctarget);
#ifdef _SNMPv3
	}
#endif

	target->set_version(pConf->version);         // set the SNMP version (default version2c)
	target->set_retry(pConf->retries);           // set the number of auto retries (default 1)
	target->set_timeout(pConf->timeout);         // set timeout (default 1000ms)

	
	Snmp_pp::IpAddress from(address);
	auto destIP = from.get_printable();
	SourcePortMap[destIP] = this;
	
	//Snmp_pp::Snmp::socket_cleanup();  // Shut down socket subsystem
}


void SNMPMasterPort::SnmpCallback( int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	Snmp_pp::Vb vb;
	int pdu_error;
	
	// late async requests and traps could come in regardless of the port being enabled or not
	//if (!enabled) return;
	
	if ((reason != SNMP_CLASS_ASYNC_RESPONSE) && (reason != SNMP_CLASS_NOTIFICATION))
	{
		std::cout << "reason: " << reason << " msg: " << snmp->error_msg(reason) << std::endl;
		return;
	}
	
	pdu_error = pdu.get_error_status();
	if (pdu_error){
		std::cout << "Response contains error: " << snmp->error_msg(pdu_error)<< std::endl;
	}
	
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	for (int i = 0; i < pdu.get_vb_count(); i++)
	{
		pdu.get_vb(vb, i);
		
#ifdef _SNMPv3
		if (pdu.get_type() == REPORT_MSG) {
			std::cout << "Received a report pdu: " << snmp->error_msg(vb.get_printable_oid()) << std::endl;
		}
#endif
		
		if (!pConf->pPointConf->OidMap.count(vb.get_oid()))
		{
			std::cout << "Uknown oid received: " << vb.get_printable_oid() << std::endl;
			continue;
		}
		auto point = pConf->pPointConf->OidMap[vb.get_oid()];
		
		for(auto IOHandler_pair : Subscribers)
		{
			point->GenerateEvent(*IOHandler_pair.second, vb, this->Name);
		}
	}
	
	// SNMP informs require a response
	if (pdu.get_type() == sNMP_PDU_INFORM) {
		std::cout << "pdu type: " << pdu.get_type() << std::endl << "sending response to inform: " << std::endl;
		//vb.set_value("This is the response.");
		pdu.set_vb(vb, 0);
		snmp->response(pdu, target);
	}
}

void SNMPMasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;
	
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	/// build up SNMP++ object needed
	Snmp_pp::Pdu pdu;                               // construct a Pdu object
	Snmp_pp::Vb vb;                                 // construct a Vb object
	
#ifdef _SNMPv3
	pdu.set_security_level(pConf->securityLevel);
	pdu.set_context_name(pConf->contextName);
	pdu.set_context_engine_id(pConf->contextEngineID);
#endif
	
	/// make Oid object to retrieve
	for(auto& Oid : pConf->pPointConf->BinaryIndicies)
	{
		if (pollgroup && (Oid->pollgroup != pollgroup))
			continue;
		
		vb.set_oid(Oid->oid);                       // set the Oid portion of the Vb
		pdu += vb;                             // add the vb to the Pdu
	}
	for(auto& Oid : pConf->pPointConf->AnalogIndicies)
	{
		if (pollgroup && (Oid->pollgroup != pollgroup))
			continue;
		
		vb.set_oid(Oid->oid);                       // set the Oid portion of the Vb
		pdu += vb;                             // add the vb to the Pdu
	}

	/// Send a async SNMP-GET request
	int status = snmp->get(pdu, *target, SNMPPort::SnmpCallback, this);
	if (status != SNMP_CLASS_SUCCESS)
	{
		std::cout << "SNMP++ GetNext Error, " << snmp->error_msg( status) << " (" << status <<")" << std::endl ;
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



