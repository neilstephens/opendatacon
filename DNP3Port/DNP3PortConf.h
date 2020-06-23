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
 * DNP3OutstationPortConf.h
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3OUTSTATIONPORTCONF_H_
#define DNP3OUTSTATIONPORTCONF_H_
#include "DNP3PointConf.h"
#include <opendnp3/LogLevels.h>
#include <opendatacon/DataPort.h>
#include <asiopal/SerialTypes.h>
#include <openpal/logging/LogFilters.h>

enum class TCPClientServer {CLIENT,SERVER,DEFAULT};
enum class server_type_t {ONDEMAND,PERSISTENT,MANUAL};
struct DNP3AddrConf
{
	//Serial
	asiopal::SerialSettings SerialSettings;

	//IP
	std::string IP;
	uint16_t Port;
	TCPClientServer ClientServer;

	//Common
	uint16_t OutstationAddr;
	uint16_t MasterAddr;
	server_type_t ServerType;

	DNP3AddrConf():
		SerialSettings(),
		IP("127.0.0.1"),
		Port(20000),
		ClientServer(TCPClientServer::DEFAULT),
		OutstationAddr(1),
		MasterAddr(0),
		ServerType(server_type_t::ONDEMAND)
	{}
};

class DNP3PortConf: public DataPortConf
{
public:
	DNP3PortConf(const std::string& FileName, const Json::Value& ConfOverrides):
		LOG_LEVEL(opendnp3::levels::NORMAL)
	{
		pPointConf = std::make_unique<DNP3PointConf>(FileName, ConfOverrides);
	}

	std::unique_ptr<DNP3PointConf> pPointConf;
	DNP3AddrConf mAddrConf;
	openpal::LogFilters LOG_LEVEL;
};

#endif /* DNP3OUTSTATIONPORTCONF_H_ */
