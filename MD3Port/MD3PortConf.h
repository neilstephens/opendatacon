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
 * MD3OutstationPortConf.h
 *
 *  Created on: 1/2/2018
 *      Author: Scott Ellis - derived from MD3 version
 */

#ifndef MD3OUTSTATIONPORTCONF_H_
#define MD3OUTSTATIONPORTCONF_H_

#include <opendatacon/DataPort.h>
#include "MD3PointConf.h"

typedef enum
{
	PERSISTENT,
	ONDEMAND,
	MANUAL
} server_type_t;

enum class SerialParity: char
{
	NONE='N',EVEN='E',ODD='O'
};

struct MD3AddrConf
{
	//Serial
	std::string SerialDevice;
	uint32_t BaudRate;
	SerialParity Parity;
	uint8_t DataBits;
	uint8_t StopBits;

	//IP
	std::string IP;
	uint16_t Port;

	//Common
	uint8_t OutstationAddr;
	server_type_t ServerType;

	// Default address values can minimally set SerialDevice or IP.
	MD3AddrConf():
		SerialDevice(""),
		BaudRate(115200),
		Parity(SerialParity::NONE),
		DataBits(8),
		StopBits(1),
		IP(""),
		Port(502),
		OutstationAddr(1),
		ServerType(ONDEMAND)
	{}
};

class MD3PortConf: public DataPortConf
{
public:
	MD3PortConf(std::string FileName):
		mAddrConf()
	{

	}

	MD3AddrConf mAddrConf;
};

#endif /* MD3OUTSTATIONPORTCONF_H_ */
