/*	opendatacon
*
*	Copyright (c) 2018:
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
* CBPort.cpp
*
*  Created on: 12/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "CBPort.h"
#include "CBPortConf.h"
#include <iostream>
#include <utility>

CBPort::CBPort(const std::string &aName, const std::string & aConfFilename, const Json::Value & aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides),
	Name(aName)
{
	SOEBufferOverflowFlag = std::make_shared<protected_bool>(false); // Only really needed for OutStation

	//the creation of a new CBPortConf will get the point details
	pConf = std::make_unique<CBPortConf>(ConfFilename, ConfOverrides);

	// Just to save a lot of dereferencing..
	MyConf = static_cast<CBPortConf*>(this->pConf.get());
	MyPointConf = MyConf->pPointConf;

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

TCPClientServer CBPort::ClientOrServer()
{
	// SERVER is DEFAULT
	return MyConf->mAddrConf.ClientServer;
}
bool CBPort::IsServer()
{
	return (TCPClientServer::SERVER == ClientOrServer());
}

void CBPort::ProcessElements(const Json::Value& JSONRoot)
{
	// The points are handled by the CBPointConf class.
	// This is all the port specific stuff.

	if (!JSONRoot.isObject()) return;

	if (JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		LOGERROR("Warning: CB port serial device AND IP address specified - IP overrides");

	if (JSONRoot.isMember("IP"))
	{
		static_cast<CBPortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
	}

	if (JSONRoot.isMember("Port"))
		static_cast<CBPortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asString();

	if (JSONRoot.isMember("TCPClientServer"))
	{
		if (JSONRoot["TCPClientServer"].asString() == "CLIENT")
			static_cast<CBPortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::CLIENT;
		else if (JSONRoot["TCPClientServer"].asString() == "SERVER")
			static_cast<CBPortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::SERVER;
		else if (JSONRoot["TCPClientServer"].asString() == "DEFAULT")
			static_cast<CBPortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::SERVER;
		else
			LOGERROR("Warning: Invalid TCP client/server type, it should be CLIENT, SERVER, or DEFAULT(SERVER) : "+ JSONRoot["TCPClientServer"].asString());
	}

	if (JSONRoot.isMember("OutstationAddr"))
		static_cast<CBPortConf*>(pConf.get())->mAddrConf.OutstationAddr = numeric_cast<uint8_t>(JSONRoot["OutstationAddr"].asUInt());

	if (JSONRoot.isMember("TCPConnectRetryPeriodms"))
		static_cast<CBPortConf*>(pConf.get())->mAddrConf.TCPConnectRetryPeriodms = numeric_cast<uint16_t>(JSONRoot["TCPConnectRetryPeriodms"].asUInt());

}

int CBPort::Limit(int val, int max)
{
	return val > max ? max : val;
}
uint8_t CBPort::Limit(uint8_t val, uint8_t max)
{
	return val > max ? max : val;
}

void CBPort::SetSendTCPDataFn(std::function<void(std::string)> Send)
{
	if (!enabled.load()) return; // Port Disabled so dont process
	CBConnection::SetSendTCPDataFn(pConnection,std::move(Send));
}

// Test only method for simulating input from the TCP Connection.
void CBPort::InjectSimulatedTCPMessage(buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	if (!enabled.load()) return; // Port Disabled so dont process
	CBConnection::InjectSimulatedTCPMessage(pConnection,readbuf);
}

// The only method that sends to the TCP Socket
void CBPort::SendCBMessage(const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
		return;
	}
	if (!enabled.load()) return; // Port Disabled so dont process

	CBConnection::Write(pConnection,CompleteCBMessage);
}


