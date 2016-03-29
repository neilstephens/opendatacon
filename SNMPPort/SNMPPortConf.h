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
 * SNMPPortConf.h
 *
 *  Created on: 26/02/2016
 *      Author: Alan Murray
 */

#ifndef SNMPOUTSTATIONPORTCONF_H_
#define SNMPOUTSTATIONPORTCONF_H_

#include <opendatacon/DataPort.h>
#include "SNMPPointConf.h"

typedef enum
{
	PERSISTENT,
	ONDEMAND,
	MANUAL
} server_type_t;

struct SNMPAddrConf
{
	/// IP
	std::string IP;
	uint16_t Port;
	uint16_t SourcePort;
	uint16_t TrapPort;

	/// Common
	server_type_t ServerType;
	
	SNMPAddrConf():
		IP("127.0.0.1"),
		Port(161),
		SourcePort(0),
		TrapPort(162),
		ServerType(ONDEMAND)
	{}
};

class SNMPPortConf: public DataPortConf
{
public:
	SNMPPortConf(std::string FileName):
		mAddrConf(),
		version(Snmp_pp::snmp_version::version2c),
		retries(1),
		timeout(1000),
		readcommunity("public"),
		writecommunity("private"),
		privPassword(""),
		authPassword(""),
		securityName(""),
		authProtocol(SNMP_AUTHPROTOCOL_NONE),
		privProtocol(SNMP_PRIVPROTOCOL_NONE),
		securityLevel(SNMP_SECURITY_LEVEL_NOAUTH_NOPRIV),
		contextName(""),
		contextEngineID("")
	{
		pPointConf.reset(new SNMPPointConf(FileName));
	}

	std::unique_ptr<SNMPPointConf> pPointConf;
	SNMPAddrConf mAddrConf;
	
	/// SNMP options
	Snmp_pp::snmp_version version;
	int retries;
	int timeout;
	Snmp_pp::OctetStr readcommunity;
	Snmp_pp::OctetStr writecommunity;
	
	/// SNMP v3 options
	Snmp_pp::OctetStr privPassword;
	Snmp_pp::OctetStr authPassword;
	Snmp_pp::OctetStr securityName;
	long authProtocol;
	long privProtocol;
	
	int securityLevel;
	Snmp_pp::OctetStr contextName;
	Snmp_pp::OctetStr contextEngineID;
};

#endif /* SNMPOUTSTATIONPORTCONF_H_ */
