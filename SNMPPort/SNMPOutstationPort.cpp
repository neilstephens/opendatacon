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
 * SNMPOutstationPort.cpp
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */

#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "SNMPOutstationPort.h"
#include "SNMPSingleton.h"

#include <opendnp3/LogLevels.h>

SNMPOutstationPort::SNMPOutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	SNMPPort(aName, aConfFilename, aConfOverrides)
{}

SNMPOutstationPort::~SNMPOutstationPort()
{
	Disable();
}

void SNMPOutstationPort::Enable()
{
	if(enabled) return;
	enabled = true;
		
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	// Only change stack state if it is a persistent server
	if (pConf->mAddrConf.ServerType == server_type_t::PERSISTENT)
	{
		this->Connect();
	}
}

void SNMPOutstationPort::Connect()
{
	if (!enabled) return;
	stack_enabled = true;
}

void SNMPOutstationPort::Disable()
{
	Disconnect();
	enabled = false;
}

void SNMPOutstationPort::Disconnect()
{
	if (!stack_enabled) return;
	stack_enabled = false;
}

void SNMPOutstationPort::StateListener(opendnp3::ChannelState state)
{
	if (!enabled) return;

	if(state == opendnp3::ChannelState::OPEN)
	{
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
		}
	}
	else
	{
		for(auto IOHandler_pair : Subscribers)
		{
			IOHandler_pair.second->Event(ConnectState::DISCONNECTED, 0, this->Name);
		}
	}
}
void SNMPOutstationPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	std::string log_id;
	log_id = "outs_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);
	
	/// Get a listening session for receiving Get requests
	snmp_inbound = SNMPSingleton::GetInstance().GetListenSession(pConf->mAddrConf.Port);
	if (snmp_inbound == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP trap stack creation failed.'";
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	/// Get a poll session to send send traps with
	snmp_outbound = SNMPSingleton::GetInstance().GetPollSession(pConf->mAddrConf.SourcePort);
	if (snmp_outbound == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed.'";
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	/// Create destination IP address for sending traps to
	Snmp_pp::UdpAddress address(pConf->mAddrConf.IP.c_str());      // make a SNMP++ Generic address
	address.set_port(pConf->mAddrConf.TrapPort);
	if ( !address.valid() )
	{
		// check validity of address
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed, invalid IP address or hostname'";
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	/// SNMPv3 specific configuration
#ifdef _SNMPv3
	//---------[ init SnmpV3 ]--------------------------------------------
	int status;
	if (pConf->version == Snmp_pp::version3) {
		v3_MP = SNMPSingleton::GetInstance().Getv3MP();
		
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
		if (pConf->version == Snmp_pp::version3) {
			std::string msg = Name + ": SNMP++ not compiled with version 3 support.'";
			auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			throw std::runtime_error(msg);
		}
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
	
	SNMPSingleton::GetInstance().RegisterPort(Snmp_pp::IpAddress(address).get_printable(),pConf->mAddrConf.Port,this);
}

void SNMPOutstationPort::ProcessResponse(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	std::string msg = Name + ": received invalid PDU (response)";
	auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);
}

void SNMPOutstationPort::ProcessGet(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	if (!stack_enabled) return;
	
	Snmp_pp::Vb vb;
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	for (int i = 0; i < pdu.get_vb_count(); i++)
	{
		pdu.get_vb(vb, i);
		
		auto point_pair = pConf->pPointConf->OidMap.equal_range(vb.get_oid());
		if (point_pair.first == point_pair.second)
		{
			std::string msg = Name + ": Uknown oid received: " + vb.get_printable_oid();
			auto log_entry = openpal::LogEntry("SNMPMasterPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
			continue;
		}
		
		// Set the vb value to the database value
		point_pair.first->second->GetValue(vb);
		pdu.set_vb(vb, i);
	}
	snmp->response(pdu, target);
}

void SNMPOutstationPort::ProcessTrap(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	std::string msg = Name + ": received invalid PDU (trap)";
	auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);
}

void SNMPOutstationPort::ProcessReport(Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	std::string msg = Name + ": recevied invalid PDU (report)";
	auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);
}

template<typename T>
inline opendnp3::CommandStatus SNMPOutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//FIXME: this is meant to return if we support the type of command
	//at the moment we just return success if it's configured as a control
	/*
	    auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	    if(std::is_same<T,opendnp3::ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	    {
	                    for(auto index : pConf->pPointConf->ControlIndicies)
	                            if(index == aIndex)
	                                    return opendnp3::CommandStatus::SUCCESS;
	    }
	*/
	return opendnp3::CommandStatus::NOT_SUPPORTED;
}
template<typename T>
inline opendnp3::CommandStatus SNMPOutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	//container to store our async futures
	std::vector<std::future<opendnp3::CommandStatus> > future_results;

	for(auto IOHandler_pair : Subscribers)
	{
		future_results.push_back((IOHandler_pair.second->Event(arCommand, aIndex, this->Name)));
	}

	for(auto& future_result : future_results)
	{
		//if results aren't ready, we'll try to do some work instead of blocking
		while(future_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
		{
			//not ready - let's lend a hand to speed things up
			this->pIOS->poll_one();
		}
		//first one that isn't a success, we can return
		if(future_result.get() != opendnp3::CommandStatus::SUCCESS)
			return opendnp3::CommandStatus::UNDEFINED;
	}

	return opendnp3::CommandStatus::SUCCESS;
}

std::future<opendnp3::CommandStatus> SNMPOutstationPort::SendTrap(OidToEvent& point)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
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
	vb.set_oid(point.oid);                       // set the Oid portion of the Vb
	point.GetValue(vb);
	pdu += vb;                             // add the vb to the Pdu
	
	/// Send SNMP-TRAP
	int status = snmp_outbound->trap(pdu, *target);
	if (status != SNMP_CLASS_SUCCESS)
	{
		std::cout << "SNMP++ GetNext Error, " << snmp_outbound->error_msg( status) << " (" << status <<")" << std::endl;
		cmd_promise.set_value(opendnp3::CommandStatus::DOWNSTREAM_FAIL);
		return cmd_promise.get_future();
	}
	cmd_promise.set_value(opendnp3::CommandStatus::SUCCESS);
	return cmd_promise.get_future();
}

std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	
	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}
	if(!stack_enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::DOWNSTREAM_FAIL);
		return cmd_promise.get_future();
	}
	
	// Update database point value
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	auto& point = pConf->pPointConf->BinaryIndicies.at(index);
	point->value = meas;
	
	// Send Trap
	return SendTrap(*point);
}

std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	
	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}
	if(!stack_enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::DOWNSTREAM_FAIL);
		return cmd_promise.get_future();
	}
	
	// Update database point value
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	auto& point = pConf->pPointConf->AnalogIndicies.at(index);
	point->value = meas;
	
	// Send Trap
	return SendTrap(*point);
}

std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }

template<typename T>
inline std::future<opendnp3::CommandStatus> SNMPOutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();
	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

