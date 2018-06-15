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
 * LogToTCP.h
 *
 *  Created on: 2018-01-24
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#ifndef LOGTOTCP_H
#define LOGTOTCP_H

#include <openpal/logging/ILogHandler.h>
#include <chrono>
#include <opendatacon/TCPSocketManager.h>

class LogToTCP: public openpal::ILogHandler
{
public:
	LogToTCP();
	void Startup(std::string IP, std::string Port, asio::io_service* pIOS, bool isClient = false);
	void Log( const openpal::LogEntry& arEntry ) override;
	void Shutdown();
private:
	std::unique_ptr<odc::TCPSocketManager<std::string>> pSockMan;

	void ReadCallback(odc::buf_t& buf);
	void StateCallback(bool connected);
};

#endif // LOGTOTCP_H
