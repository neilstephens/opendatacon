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


#include <iostream>
#include "CBPort.h"
#include "CBPortConf.h"

CBPort::CBPort(const std::string &aName, const std::string & aConfFilename, const Json::Value & aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	//TODO: Do we have to create our own logger instance? Or just assume will be run on linux...
	// logger = spdlog::get("CBPort"); // Only gets the opendatacon logger in Linux at the moment!

	//the creation of a new CBPortConf will get the point details
	pConf.reset(new CBPortConf(ConfFilename, ConfOverrides));

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
	CBConnection::SetSendTCPDataFn(MyConf->mAddrConf.ChannelID(),Send);
}

// Test only method for simulating input from the TCP Connection.
void CBPort::InjectSimulatedTCPMessage(buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	CBConnection::InjectSimulatedTCPMessage(MyConf->mAddrConf.ChannelID(),readbuf);
}

// The only method that sends to the TCP Socket
void CBPort::SendCBMessage(const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
		return;
	}
	CBConnection::Write(MyConf->mAddrConf.ChannelID(),CompleteCBMessage);
}
// This message is constructed by the Master and the RTU in response.
void CBPort::BuildUpdateTimeMessage(uint8_t StationAddress, CBTime cbtime, CBMessage_t & CompleteCBMessage)
{
	uint8_t hh;
	uint8_t mm;
	uint8_t ss;
	uint16_t msec;

	to_hhmmssmmfromCBtime(cbtime, hh, mm, ss, msec);

	uint16_t FirstDataB = ((hh >> 2) & 0x07) | 0x20; // The 2 is the number of blocks.	// Top 3 bits of hh
	auto firstblock = CBBlockData(StationAddress, MASTER_SUB_FUNC_SEND_TIME_UPDATES, FUNC_MASTER_STATION_REQUEST, FirstDataB, false);

	uint16_t DataA = ShiftLeftResult16Bits(hh & 0x03, 10) | ShiftLeftResult16Bits(mm & 0x3F, 4) | ((ss >> 2) & 0x0F); // Bottom 2 bits of hh, 6 bits of mm, top 4 bits of ss
	uint16_t DataB = ShiftLeftResult16Bits(ss & 0x03, 10) | (msec & 0x3ff);                                           // bottom 2 bits of ss, 10 bits of msec

	auto secondblock = CBBlockData(DataA, DataB, true);

	CompleteCBMessage.push_back(firstblock);
	CompleteCBMessage.push_back(secondblock);
}

