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
	if(!enabled) return;
	if (stack_enabled) return;
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
	if(!enabled)
		return;

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
	log_id = "mast_" + pConf->mAddrConf.IP + ":" + std::to_string(pConf->mAddrConf.Port);
	
	// Get a poll session to poll the SNMP outstation with
	snmp = SNMPSingleton::GetInstance().GetPollSession(pConf->mAddrConf.SourcePort);
	if (snmp == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP stack creation failed.'";
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
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
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		throw std::runtime_error(msg);
	}
	
	// Get a listening session for outstations to send SNMP traps to
	snmp_trap = SNMPSingleton::GetInstance().GetListenSession(pConf->mAddrConf.TrapPort);
	if ( snmp_trap == nullptr)
	{
		std::string msg = Name + ": Stack error: 'SNMP trap stack creation failed.'";
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
	
	SNMPSingleton::GetInstance().RegisterPort(Snmp_pp::IpAddress(address).get_printable(),this);
}

void SNMPOutstationPort::SnmpCallback( int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target)
{
	Snmp_pp::Vb vb;
	int pdu_error;
	
	// late async requests and traps could come in regardless of the port being enabled or not
	//if (!enabled) return;
	
	if ((reason != SNMP_CLASS_ASYNC_RESPONSE) && (reason != SNMP_CLASS_NOTIFICATION))
	{
		std::string msg = Name + ": " + snmp->error_msg(reason);
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
	
	pdu_error = pdu.get_error_status();
	if (pdu_error){
		std::string msg = Name + ": Response contains error: " + snmp->error_msg(pdu_error);
		auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
	
	auto pConf = static_cast<SNMPPortConf*>(this->pConf.get());
	
	auto pdu_type = pdu.get_type();
	
	/*
	 #define sNMP_PDU_GET	    (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x0)
	 #define sNMP_PDU_GETNEXT    (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x1)
	 #define sNMP_PDU_RESPONSE   (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x2)
	 #define sNMP_PDU_SET	    (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x3)
	 #define sNMP_PDU_V1TRAP     (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x4)
	 #define sNMP_PDU_GETBULK    (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x5)
	 #define sNMP_PDU_INFORM     (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x6)
	 #define sNMP_PDU_TRAP       (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x7)
	 #define sNMP_PDU_REPORT     (aSN_CONTEXT | aSN_CONSTRUCTOR | 0x8)
	 */
	
	switch (pdu_type)
	{
		case sNMP_PDU_RESPONSE:
		{/// Response to an inform acknowledging data was received
		}
			break;
			
		case sNMP_PDU_GET:
		case sNMP_PDU_GETNEXT:
		case sNMP_PDU_GETBULK:
		{/// SNMP informs require a response, just echo what was received back to acknowledge what we received
			for (int i = 0; i < pdu.get_vb_count(); i++)
			{
				pdu.get_vb(vb, i);
				
				auto point_pair = pConf->pPointConf->OidMap.equal_range(vb.get_oid());
				if (point_pair.first == point_pair.second)
				{
					std::string msg = Name + ": Uknown oid received: " + vb.get_printable_oid();
					auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
					pLoggers->Log(log_entry);
					continue;
				}
				
				// Set the vb value to the database value
				point_pair.first->second->GetValue(vb);
				pdu.set_vb(vb, i);
			}
			snmp->response(pdu, target);
		}
			break;
			
		case sNMP_PDU_V1TRAP:
		case sNMP_PDU_TRAP:
		case sNMP_PDU_SET:
		case sNMP_PDU_INFORM:
		{/// Either solicited or unsolicited data
			for (int i = 0; i < pdu.get_vb_count(); i++)
			{
				pdu.get_vb(vb, i);
				
				auto point_pair = pConf->pPointConf->OidMap.equal_range(vb.get_oid());
				if (point_pair.first == point_pair.second)
				{
					std::string msg = Name + ": Uknown oid received: " + vb.get_printable_oid();
					auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
					pLoggers->Log(log_entry);
					continue;
				}
				
				for(auto it=point_pair.first; it!=point_pair.second; ++it)
				{
					for(auto IOHandler_pair : Subscribers)
					{
						it->second->GenerateEvent(*IOHandler_pair.second, vb, this->Name);
					}
				}
			}
			
			/// SNMP informs require a response, just echo what was received back to acknowledge what we received
			if (pdu_type == sNMP_PDU_INFORM) {
				std::string msg = Name + ": sending response to inform";
				auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::INFO,"", msg.c_str(), -1);
				pLoggers->Log(log_entry);
				snmp->response(pdu, target);
			}
		}
			break;
			
		case sNMP_PDU_REPORT:
		{/// A REPORT operation is an indication of some core SNMP communication error or bootstrapping response.
			std::string msg = Name + ": Received a report pdu: " + vb.get_printable_oid();
			auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
			break;
			
		default:
		{
			std::string msg = Name + ": Invalid pdu type received: " + vb.get_printable_oid();
			auto log_entry = openpal::LogEntry("SNMPOutstationPort", openpal::logflags::WARN,"", msg.c_str(), -1);
			pLoggers->Log(log_entry);
		}
	}
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

std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }
std::future<opendnp3::CommandStatus> SNMPOutstationPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName){ return EventT(meas, index, SenderName); }

template<typename T>
inline std::future<opendnp3::CommandStatus> SNMPOutstationPort::EventT(T& meas, uint16_t index, const std::string& SenderName)
{
	auto cmd_promise = std::promise<opendnp3::CommandStatus>();

	if(!enabled)
	{
		cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
		return cmd_promise.get_future();
	}

	//SNMPPortConf* pConf = static_cast<SNMPPortConf*>(this->pConf.get());

	//TODO: finishme
	
	if(std::is_same<T,opendnp3::Analog>::value)
	{
//		int map_index = find_index(pConf->pPointConf->AnalogIndicies, index);
//		if(map_index >= 0)
//			*(mb_mapping->tab_input_registers + map_index) = (uint16_t)meas.value;
	}
	else if(std::is_same<T,opendnp3::Binary>::value)
	{
//		int map_index = find_index(pConf->pPointConf->BinaryIndicies, index);
//		if(map_index >= 0)
//			*(mb_mapping->tab_input_bits + index) = (uint8_t)meas.value;
	}
	//TODO: impl other types

	cmd_promise.set_value(opendnp3::CommandStatus::UNDEFINED);
	return cmd_promise.get_future();
}

