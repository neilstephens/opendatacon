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
 * MD3Port.cpp
 *
 *  Created on:
 *      Author: Scott Ellis
 */
#include "MD3Port.h"
#include <iostream>
#include "MD3PortConf.h"
#include <openpal/logging/LogLevels.h>


MD3Port::MD3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	DataPort(aName, aConfFilename, aConfOverrides),
	pSockMan(nullptr)
{
	//the creation of a new MD3PortConf will get the point details
	pConf.reset(new MD3PortConf(ConfFilename, ConfOverrides));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

TCPClientServer MD3Port::ClientOrServer()
{
	MD3PortConf* pConf = static_cast<MD3PortConf*>(this->pConf.get());
	if (pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::SERVER;
	return pConf->mAddrConf.ClientServer;
}

//DataPort function for UI
/*
const Json::Value MD3Port::GetStatus() const
{
	auto ret_val = Json::Value();

	if (!enabled)
		ret_val["Result"] = "Port disabled";
	else if (link_dead)
		ret_val["Result"] = "Port enabled - link down";
	else if (status == opendnp3::LinkStatus::RESET)
		ret_val["Result"] = "Port enabled - link up (reset)";
	else if (status == opendnp3::LinkStatus::UNRESET)
		ret_val["Result"] = "Port enabled - link up (unreset)";
	else
		ret_val["Result"] = "Port enabled - link status unknown";

	return ret_val;
}
*/

void MD3Port::ProcessElements(const Json::Value& JSONRoot)
{
	// The points are handled by the MD3PointConf class.
	// This is all the port specfic stuff.

	if (!JSONRoot.isObject()) return;

	if (JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		std::cout << "Warning: MD3 port serial device AND IP address specified - IP overrides" << std::endl;

	if (JSONRoot.isMember("SerialDevice"))
	{
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = JSONRoot["SerialDevice"].asString();
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.IP = "";

		if (JSONRoot.isMember("BaudRate"))
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.baud = JSONRoot["BaudRate"].asUInt();
		if (JSONRoot.isMember("Parity"))
		{
			if (JSONRoot["Parity"].asString() == "EVEN")
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::EVEN;
			else if (JSONRoot["Parity"].asString() == "ODD")
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::ODD;
			else if (JSONRoot["Parity"].asString() == "NONE")
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.parity = asiopal::ParityType::NONE;
			else
				std::cout << "Warning: Invalid serial parity: " << JSONRoot["Parity"].asString() << ", should be EVEN, ODD, or NONE." << std::endl;
		}
		if (JSONRoot.isMember("DataBits"))
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.dataBits = JSONRoot["DataBits"].asUInt();
		if (JSONRoot.isMember("StopBits"))
		{
			if (JSONRoot["StopBits"].asFloat() == 0)
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::NONE;
			else if (JSONRoot["StopBits"].asFloat() == 1)
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::ONE;
			else if (JSONRoot["StopBits"].asFloat() == 1.5)
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::ONE_POINT_FIVE;
			else if (JSONRoot["StopBits"].asFloat() == 2)
				static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.stopBits = asiopal::StopBits::TWO;
			else
				std::cout << "Warning: Invalid serial stop bits: " << JSONRoot["StopBits"].asFloat() << ", should be 0, 1, 1.5, or 2" << std::endl;
		}
	}

	if (JSONRoot.isMember("IP"))
	{
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = "";
	}

	if (JSONRoot.isMember("Port"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if (JSONRoot.isMember("TCPClientServer"))
	{
		if (JSONRoot["TCPClientServer"].asString() == "CLIENT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::CLIENT;
		else if (JSONRoot["TCPClientServer"].asString() == "SERVER")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::SERVER;
		else if (JSONRoot["TCPClientServer"].asString() == "DEFAULT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::DEFAULT;
		else
			std::cout << "Warning: Invalid TCP client/server type: " << JSONRoot["TCPClientServer"].asString() << ", should be CLIENT, SERVER, or DEFAULT." << std::endl;
	}

	if (JSONRoot.isMember("OutstationAddr"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if (JSONRoot.isMember("MasterAddr"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.MasterAddr = JSONRoot["MasterAddr"].asUInt();

	if (JSONRoot.isMember("ServerType"))
	{
		if (JSONRoot["ServerType"].asString() == "ONDEMAND")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::ONDEMAND;
		else if (JSONRoot["ServerType"].asString() == "PERSISTENT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::PERSISTENT;
		else if (JSONRoot["ServerType"].asString() == "MANUAL")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ServerType = server_type_t::MANUAL;
		else
			std::cout << "Invalid MD3 Port server type: '" << JSONRoot["ServerType"].asString() << "'." << std::endl;
	}
}

int MD3Port::Limit(int val, int max)
{
	return val > max ? max : val;
}