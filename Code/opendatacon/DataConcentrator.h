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
#include <opendatacon/asio.h>
#include <opendatacon/LogHelpers.h>
#include <unordered_map>
#include <set>

std::vector<spdlog::sink_ptr> GetAllSinks(const std::unordered_map<std::string, spdlog::sink_ptr>& sinks);
void AddLogger(const std::string& name, const std::unordered_map<std::string, spdlog::sink_ptr>& sinks);
void ReloadLogSinks(const std::unordered_map<std::string, spdlog::sink_ptr>& sinks, size_t flush_period_s = 0);

class TestHook;
class DataConcentrator: public ConfigParser
{
	friend class TestHook;
public:
	DataConcentrator(const std::string& FileName);
	~DataConcentrator() override;

	void ProcessElements(const Json::Value& JSONRoot) override;
	void Build();
	void Run();
	bool ReloadConfig(const std::string& filename = "", const size_t disable_delay = reload_disable_delay_s);
	void Shutdown();
	bool isShuttingDown();
	bool isShutDown();

private:
	inline static LogHelpers Log{"opendatacon"};
	std::pair<spdlog::level::level_enum,spdlog::level::level_enum> ConfigureLogSinks(const Json::Value& JSONRoot);
	void ProcessPorts(const Json::Value& Ports);
	void ProcessConnectors(const Json::Value& Connectors);
	void ProcessPlugins(const Json::Value& Plugins);
	void EnableIOHandler(std::shared_ptr<IOHandler> ioh);
	void EnableIUI(std::shared_ptr<IUI> iui);
	void PrepInterface(std::shared_ptr<IUI> interface);
	void RefreshIUIResponders();
	Json::Value FindChangedConfs(const std::string& collection_name, std::unordered_map<std::string,std::shared_ptr<Json::Value>>& old_file_confs,
		const Json::Value &old_main_conf, const Json::Value &new_main_conf,
		std::set<std::string> &created, std::set<std::string> &changed, std::set<std::string> &deleted);

	DataPortCollection DataPorts;
	DataConnectorCollection DataConnectors;
	InterfaceCollection Interfaces;
	std::unordered_map<std::string,const IUIResponder*> RespondersMasterCopy;

	std::shared_ptr<odc::asio_service> pIOS;
	std::shared_ptr<asio::io_service::work> ios_working;
	std::atomic<size_t> starting_element_count = 0;
	std::once_flag shutdown_flag;
	std::atomic_bool shutting_down;
	std::atomic_bool shut_down;
	static size_t reload_disable_delay_s;

	//ostream for spdlog logging sink
	std::unordered_map<std::string, TCPstringbuf> TCPbufs;
	std::unordered_map<std::string, std::unique_ptr<std::ostream>> pTCPostreams;

	std::vector<std::string> ConfigLogSinkNames;
	std::unordered_map<std::string, spdlog::sink_ptr> LogSinks;
	inline void ListLogSinks(Json::Value& out);
	inline void ListLogLevels(Json::Value& out);
	Json::Value SetLogLevel(std::stringstream& ss);
	Json::Value SetLogFilter(std::stringstream& ss, bool isWhite);
	Json::Value RemoveLogFilter(std::stringstream& ss);
	Json::Value ListLogFilters();
	Json::Value AddLogSink(std::stringstream& ss, bool doReload = true);
	Json::Value DeleteLogSink(std::stringstream& ss);
	size_t log_flush_period = 0;

	void ParkThread();
	bool ParkThreads();
	std::atomic_bool parking = false;
	std::atomic<size_t> num_parked_threads = 0;
	std::vector<std::thread> threads;

	Json::Value SpoofEvent(std::stringstream& ss, SharedStatusCallback_t callback = std::make_shared<SharedStatusCallback_t::element_type>([](CommandStatus){}));
};

#endif /* DATACONCENTRATOR_H_ */
