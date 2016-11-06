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
 * JSONServerPort.cpp
 *
 *  Created on: 02/11/2016
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendnp3/LogLevels.h>
#include "JSONServerPort.h"

JSONServerPort::JSONServerPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	JSONPort(aName, aConfFilename, aConfOverrides),
	pEnableDisableSync(nullptr)
{
}

void JSONServerPort::Enable()
{
	if(pEnableDisableSync.get() == nullptr)
		pEnableDisableSync.reset(new asio::strand(*pIOS));
	pEnableDisableSync->post([&]()
					 {
						 if(!enabled)
						 {
							 enabled = true;
							 PortUp();
						 }
					 });
}
void JSONServerPort::Disable()
{
	if(pEnableDisableSync.get() == nullptr)
		pEnableDisableSync.reset(new asio::strand(*pIOS));
	pEnableDisableSync->post([&]()
					 {
						 if(enabled)
						 {
							 enabled = false;
							 PortDown();
						 }
					 });
}

void JSONServerPort::PortUp()
{
	JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
	try
	{
		asio::ip::tcp::resolver resolver(*pIOS);
		asio::ip::tcp::resolver::query query(pConf->mAddrConf.IP, std::to_string(pConf->mAddrConf.Port));
		auto endpoint_iterator = resolver.resolve(query);
		pSock.reset(new asio::ip::tcp::socket(*pIOS));
		pAcceptor.reset(new asio::ip::tcp::acceptor(*pIOS, *endpoint_iterator));
		pAcceptor->async_accept(*pSock.get(),pEnableDisableSync->wrap(
						[this](asio::error_code err_code)
						{
							ConnectCompletionHandler(err_code);
						}));
	}
	catch(std::exception& e)
	{
		std::string msg = "Problem opening connection: "+Name+": "+e.what();
		auto log_entry = openpal::LogEntry("JSONClientPort", openpal::logflags::ERR,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		PortUpRetry(pConf->retry_time_ms);
		return;
	}
}

void JSONServerPort::PortDown()
{
	pAcceptor.reset(nullptr);
	//cancel the retry timer (otherwise it would tie up the io_service on shutdown)
	if(pTCPRetryTimer.get() != nullptr)
		pTCPRetryTimer->cancel();
	//shutdown and close socket by using destructor
	pSock.reset(nullptr);
	for (auto IOHandler_pair: Subscribers)
	{
		IOHandler_pair.second->Event(ConnectState::DISCONNECTED, 0, this->Name);
	}
}

void JSONServerPort::ConnectCompletionHandler(asio::error_code err_code)
{
	if(!enabled) return;
	pAcceptor.reset(nullptr);
	if(err_code)
	{
		std::string msg = Name+": Connection error: '"+err_code.message()+"'";
		auto log_entry = openpal::LogEntry("JSONServerPort", openpal::logflags::INFO,"", msg.c_str(), -1);
		pLoggers->Log(log_entry);
		JSONPortConf* pConf = static_cast<JSONPortConf*>(this->pConf.get());
		PortUpRetry(pConf->retry_time_ms);
		return;
	}
	std::string msg = Name+": Connect success!";
	auto log_entry = openpal::LogEntry("JSONServerPort", openpal::logflags::INFO,"", msg.c_str(), -1);
	pLoggers->Log(log_entry);

	for (auto IOHandler_pair: Subscribers)
	{
		IOHandler_pair.second->Event(ConnectState::CONNECTED, 0, this->Name);
	}
	Read();
}
