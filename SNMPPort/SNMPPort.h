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
	
protected:
	bool stack_enabled;
	std::shared_ptr<Snmp_pp::Snmp> snmp;
	std::shared_ptr<Snmp_pp::Snmp> snmp_trap;

	static std::shared_ptr<Snmp_pp::Snmp> GetSesion(uint16_t UdpPort);
	static std::shared_ptr<Snmp_pp::Snmp> GetTrapSession(uint16_t UdpPort, const std::string& SourceAddr);
	
	static std::shared_ptr<Snmp_pp::v3MP> Getv3MP()
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
	
	virtual void SnmpCallback(int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target) = 0;
	static void SnmpCallback(int reason, Snmp_pp::Snmp *snmp, Snmp_pp::Pdu &pdu, Snmp_pp::SnmpTarget &target, void *obj)
	{
		//TODO: use C++11 managed pointers as this is vulnerable to the pointers being invalid
		Snmp_pp::GenAddress addr;
		target.get_address(addr);
		Snmp_pp::IpAddress from(addr);
		auto destIP = from.get_printable();
		auto dest_ptr = static_cast<SNMPPort*>(obj);
		if(obj != nullptr)
		{
			SourcePortMap[destIP] = dest_ptr;
		}
		else if(SourcePortMap.count(destIP))
		{
			dest_ptr = SourcePortMap[destIP];
		}
		else
		{
			std::cout << "Trap received from unknown source: " << from.get_printable() << std::endl;
			return;
		}
		if (PortSet.count(dest_ptr))
		{
			dest_ptr->SnmpCallback(reason, snmp, pdu, target);
		}
		else
		{
			std::cout << "Trap received from deleted source: " << from.get_printable() << std::endl;
			return;
		}
	}

protected:
	static std::unordered_map<std::string, SNMPPort*> SourcePortMap;
	
private:
	static std::shared_ptr<Snmp_pp::v3MP> v3mpPtr;
	static std::unordered_map<uint16_t, std::weak_ptr<Snmp_pp::Snmp>> SnmpSessions;
	static std::unordered_map<uint16_t, std::weak_ptr<Snmp_pp::Snmp>> SnmpTrapSessions;
	static std::unordered_set<SNMPPort*> PortSet;
};

#endif /* SNMPPORT_H_ */
