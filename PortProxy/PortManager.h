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
 * PortManager.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef PORTMANAGER_H_
#define PORTMANAGER_H_

#include <opendatacon/asio.h>
#include <unordered_map>

#include <opendatacon/Platform.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/ConfigParser.h>
#include <opendatacon/TCPstringbuf.h>
#include <spdlog/spdlog.h>
#include <opendatacon/util.h>

#include "PortConnection.h"

class PortManager
{
public:
	PortManager(std::string FileName);
	~PortManager();

	void Build();
	void Run();
	void Shutdown();
	bool isShuttingDown();
	bool isShutDown();

private:
	void SetupLoggers();
	void ReCheckKeyboard();

	std::vector<std::shared_ptr<PortProxyConnection>> PortCollectionMap;

	std::shared_ptr<odc::asio_service> pIOS;
	std::shared_ptr<asio::io_service::work> ios_working;
	std::once_flag shutdown_flag;
	std::atomic_bool shutting_down;
	std::atomic_bool shut_down;

	//ostream for spdlog logging sink
	TCPstringbuf TCPbuf;
	std::unique_ptr<std::ostream> pTCPostream;

	std::map<std::string,spdlog::sink_ptr> LogSinksMap;
	std::vector<spdlog::sink_ptr> LogSinksVec;
	void SetLogLevel(std::stringstream& ss);

	std::vector<std::thread> threads;
};

#endif /* PORTMANAGER_H_ */
