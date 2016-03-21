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
 * SNMPOutstationPortConf.h
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
	//IP
	std::string IP;
	uint16_t Port;
	uint16_t SourcePort;
	uint16_t TrapPort;

	//Common
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
		mAddrConf()
	{
		pPointConf.reset(new SNMPPointConf(FileName));
	}

	std::unique_ptr<SNMPPointConf> pPointConf;
	SNMPAddrConf mAddrConf;
};

#endif /* SNMPOUTSTATIONPORTCONF_H_ */
