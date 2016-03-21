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
 * SNMPPointConf.h
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */

#ifndef SNMPPOINTCONF_H_
#define SNMPPOINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>

#include <opendatacon/DataPort.h>
#include <libsnmp.h>
#include <snmp_pp/snmp_pp.h>

#include <chrono>

class SNMPPollGroup
{
public:
	SNMPPollGroup():
		ID(0),
		pollrate(0)
	{ }

	SNMPPollGroup(uint32_t ID_, uint32_t pollrate_):
		ID(ID_),
		pollrate(pollrate_)
	{ }

	uint32_t ID;
	uint32_t pollrate;
	Snmp_pp::Pdu pdu;
};

class OidToEvent
{
public:
	OidToEvent(Snmp_pp::Oid oid_) :
	oid(oid_)
	{}
	
	virtual void GenerateEvent(IOHandler& handler, const Snmp_pp::Vb& value, const std::string& name) = 0;
	Snmp_pp::Oid oid;
};

class OidToAnalogEvent : public OidToEvent
{
public:
	OidToAnalogEvent(Snmp_pp::Oid oid_, uint32_t index_, uint32_t pollgroup_, const opendnp3::Analog& startval_):
	OidToEvent(oid_),
	index(index_),
	pollgroup(pollgroup_),
	startval(startval_)
	{ }
	
	virtual void GenerateEvent(IOHandler& handler, const Snmp_pp::Vb& value, const std::string& name);
	
	uint32_t index;
	uint32_t pollgroup;
	opendnp3::Analog startval;
};

class OidToBinaryEvent : public OidToEvent
{
public:
	OidToBinaryEvent(Snmp_pp::Oid oid_, uint32_t index_, uint32_t pollgroup_, const opendnp3::Binary& startval_, const std::string& valueOn_, const std::string& valueOff_ ):
	OidToEvent(oid_),
	index(index_),
	pollgroup(pollgroup_),
	startval(startval_),
	valueOn(valueOn_),
	valueOff(valueOff_)
	{ }
	
	virtual void GenerateEvent(IOHandler& handler, const Snmp_pp::Vb& value, const std::string& name);
	
	uint32_t index;
	uint32_t pollgroup;
	opendnp3::Binary startval;
	
	const std::string valueOn;
	const std::string valueOff;
};


template<class T>
class SNMPOidCollection: public std::vector< T >
{
public:

	
};

class SNMPPointConf: public ConfigParser
{
public:
	SNMPPointConf(std::string FileName);

	void ProcessElements(const Json::Value& JSONRoot);
	uint8_t GetUnsolClassMask();

	std::pair<opendnp3::Binary,size_t> mCommsPoint;

	std::vector<OidToBinaryEvent> BinaryIndicies;
	std::vector<OidToAnalogEvent> AnalogIndicies;
	
	std::map<Snmp_pp::Oid, OidToEvent*> OidMap;

	std::map<uint32_t, SNMPPollGroup> PollGroups;

private:
	template<class T>
	void ProcessReadGroup(const Json::Value& Ranges, std::vector<T>& ReadGroup);
};

#endif /* SNMPPOINTCONF_H_ */
