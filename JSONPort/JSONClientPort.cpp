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
 * JSONClientDataPort.cpp
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendnp3/LogLevels.h>
#include "JSONClientPort.h"

JSONClientPort::JSONClientPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	JSONPort(aName, aConfFilename, aConfOverrides)
{}

void JSONClientPort::Enable()
{
	if(enabled) return;
	JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
	try
	{
		asio::ip::tcp::resolver resolver(*pIOS);
		asio::ip::tcp::resolver::query query(pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port));
		auto endpoint_iterator = resolver.resolve(query);
		pSock.reset(new asio::ip::tcp::socket(*pIOS));
		asio::async_connect(*pSock.get(), endpoint_iterator,std::bind(&JSONClientPort::ConnectCompletionHandler,this,std::placeholders::_1));
	}
	catch(std::exception& e)
	{
		std::string msg = "Problem opening connection: "+Name+": "+e.what();
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		return;
	}
}
void JSONClientPort::ConnectCompletionHandler(asio::error_code err_code)
{
	if(err_code)
	{
		std::string msg = Name+": Connect error: '"+err_code.message()+"'";
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::INFO,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		//try again later
		JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
		pTCPRetryTimer.reset(new Timer_t(*pIOS, std::chrono::milliseconds(pConf->retry_time_ms)));
		pTCPRetryTimer->async_wait(
		      [this](asio::error_code err_code)
		      {
		            if(err_code != asio::error::operation_aborted)
					Enable();
			});
		return;
	}
	std::string msg = Name+": Connect success!";
	auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::INFO,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	enabled = true;
	for (auto IOHandler_pair: Subscribers)
	{
		IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
	}
	Read();
}
void JSONClientPort::Disable()
{
	//cancel the retry timer (otherwise it would tie up the io_service on shutdown)
	if(pTCPRetryTimer.get() != nullptr)
		pTCPRetryTimer->cancel();
	//shutdown and close socket by using destructor
	pSock.reset(nullptr);
	enabled = false;
}


