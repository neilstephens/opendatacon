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
#include <opendnp3/logging/LogLevels.h>
#include <opendatacon/DataPort.h>
#include <opendnp3/channel/SerialSettings.h>

enum class TCPClientServer {CLIENT,SERVER,DEFAULT};
inline std::string to_string(const TCPClientServer CS)
{
	switch(CS)
	{
		case TCPClientServer::CLIENT:
			return "CLIENT";
		case TCPClientServer::SERVER:
			return "SERVER";
		case TCPClientServer::DEFAULT:
			return "DEFAULT";
	}
	return "UNKNOWN";
}
enum class IPTransport {TCP,UDP,TLS};
inline std::string to_string(const IPTransport IPT)
{
	switch(IPT)
	{
		case IPTransport::TCP:
			return "TCP";
		case IPTransport::UDP:
			return "UDP";
		case IPTransport::TLS:
			return "TLS";
	}
	return "UNKNOWN";
}
struct TLSFilesConf
{
	std::string PeerCertFile;
	std::string LocalCertFile;
	std::string PrivateKeyFile;
};
enum class server_type_t {ONDEMAND,PERSISTENT,MANUAL};
enum class WatchdogBark {ONFIRST,ONFINAL,NEVER,DEFAULT};
struct DNP3AddrConf
{
	//Serial
	opendnp3::SerialSettings SerialSettings;

	//IP
	std::string IP;
	uint16_t Port;
	uint16_t UDPListenPort;
	TCPClientServer ClientServer;
	IPTransport Transport;

	//TLS
	TLSFilesConf TLSFiles;

	//Common
	uint16_t OutstationAddr;
	uint16_t MasterAddr;
	server_type_t ServerType;
	WatchdogBark ChannelLinksWatchdogBark;

	DNP3AddrConf():
		SerialSettings(),
		IP("127.0.0.1"),
		Port(20000),
		UDPListenPort(0),
		ClientServer(TCPClientServer::DEFAULT),
		Transport(IPTransport::TCP),
		OutstationAddr(1),
		MasterAddr(0),
		ServerType(server_type_t::ONDEMAND),
		ChannelLinksWatchdogBark(WatchdogBark::DEFAULT)
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
	opendnp3::LogLevels LOG_LEVEL;
};

#endif /* DNP3OUTSTATIONPORTCONF_H_ */
