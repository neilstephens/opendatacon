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
 * LogToTCP.cpp
 *
 *  Created on: 2018-01-24
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */


#include "LogToTCP.h"
#define GetMessage() GetMessage()


LogToTCP::LogToTCP():
	pSockMan(nullptr)
{}

void LogToTCP::Startup(std::string IP, std::string Port, asio::io_service* pIOS, bool isClient)
{
	pSockMan.reset(new odc::TCPSocketManager<std::string>(pIOS,!isClient,IP,Port,
			  std::bind(&LogToTCP::ReadCallback,this,std::placeholders::_1),
			  std::bind(&LogToTCP::StateCallback,this,std::placeholders::_1),
			  true));
	pSockMan->Open();
}

void LogToTCP::ReadCallback(odc::buf_t& buf)
{
	buf.consume(buf.size());
}
void LogToTCP::StateCallback(bool connected)
{}

void LogToTCP::Log( const openpal::LogEntry& arEntry )
{
	if(pSockMan.get() == nullptr)
		return;

	std::string time_str = "TODO: timestamp"; //move platform specific timestamp code from LogToFile into Platform.h or util.h
	std::string filter_str = "TODO: level"; //FilterToString(arEntry.GetFilters()); move from LogToFile into Platform.h or util.h

	std::stringstream SS;
	SS <<time_str<<" - "<< filter_str<<" - "<<arEntry.GetAlias();
	if(!arEntry.GetLocation())
		SS << " - " << arEntry.GetLocation();
//FIXME: Clash with windows #define
#define GetMessage GetMessage
	SS << " - " << arEntry.GetMessage();
	if(arEntry.GetErrorCode() != -1)
		SS << " - " << arEntry.GetErrorCode();
	SS <<std::endl;

	pSockMan->Write(SS.str());
}
void LogToTCP::Shutdown()
{
	if(pSockMan.get() == nullptr)
		return;
	pSockMan->Close();
}
