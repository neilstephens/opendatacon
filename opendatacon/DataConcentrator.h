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
 * DataConcentrator.h
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DATACONCENTRATOR_H_
#define DATACONCENTRATOR_H_
#include "DataConnector.h"
#include "DataConnectorCollection.h"
#include "DataConnector.h"
#include <opendatacon/asio.h>
#include <unordered_map>
#include <opendatacon/DataPort.h>
#include <opendatacon/DataPortCollection.h>
#include <opendatacon/InterfaceCollection.h>
#include <opendatacon/Platform.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/ConfigParser.h>
#include <opendatacon/TCPstringbuf.h>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <opendatacon/IUI.h>

class DataConcentrator: public ConfigParser, public IUIResponder
{
public:
	DataConcentrator(const std::string& FileName);
	~DataConcentrator() override;

	void ProcessElements(const Json::Value& JSONRoot) override;
	void Build();
	void Run();
	void Shutdown();
	bool isShuttingDown();
	bool isShutDown();

private:
	DataPortCollection DataPorts;
	DataConnectorCollection DataConnectors;
	InterfaceCollection Interfaces;

	std::shared_ptr<odc::asio_service> pIOS;
	std::shared_ptr<asio::io_service::work> ios_working;
	std::once_flag shutdown_flag;
	std::atomic_bool shutting_down;
	std::atomic_bool shut_down;

	//ostream for spdlog logging sink
	std::unordered_map<std::string, TCPstringbuf> TCPbufs;
	std::unordered_map<std::string, std::unique_ptr<std::ostream>> pTCPostreams;

	std::unordered_map<std::string, spdlog::sink_ptr> LogSinks;
	inline void ListLogSinks();
	inline void ListLogLevels();
	void SetLogLevel(std::stringstream& ss);
	void AddLogSink(std::stringstream& ss);
	void DeleteLogSink(std::stringstream& ss);

	std::vector<std::thread> threads;
};

#endif /* DATACONCENTRATOR_H_ */
