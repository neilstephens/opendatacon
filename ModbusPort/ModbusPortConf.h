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
 * ModbusOutstationPortConf.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef ModbusOUTSTATIONPORTCONF_H_
#define ModbusOUTSTATIONPORTCONF_H_
#include "ModbusPointConf.h"
#include <opendatacon/DataPort.h>

enum class server_type_t
{
	PERSISTENT,
	ONDEMAND,
	MANUAL
};

enum class SerialParity: char
{
	NONE='N',EVEN='E',ODD='O'
};

struct ModbusAddrConf
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

	ModbusAddrConf():
		SerialDevice(""),
		BaudRate(115200),
		Parity(SerialParity::NONE),
		DataBits(8),
		StopBits(1),
		IP(""),
		Port(502),
		OutstationAddr(1),
		ServerType(server_type_t::ONDEMAND)
	{}
};

class ModbusPortConf: public DataPortConf
{
public:
	ModbusPortConf(const std::string& FileName):
		mAddrConf()
	{
		pPointConf = std::make_unique<ModbusPointConf>(FileName);
	}

	std::unique_ptr<ModbusPointConf> pPointConf;
	ModbusAddrConf mAddrConf;
};

#endif /* ModbusOUTSTATIONPORTCONF_H_ */
