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
 * DataConcentrator.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <thread>
#include <opendatacon/asio.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <opendatacon/asio_syslog_spdlog_sink.h>

#include <opendatacon/util.h>
#include <opendatacon/Version.h>
#include "DataConcentrator.h"
#include "NullPort.h"

DataConcentrator::DataConcentrator(std::string FileName):
	ConfigParser(FileName),
	pIOS(std::make_shared<odc::asio_service>(std::thread::hardware_concurrency()+1)),
	ios_working(pIOS->make_work()),
	shutting_down(false),
	shut_down(false),
	pTCPostream(nullptr)
{
	// Enable loading of libraries
	InitLibaryLoading();

	//Version
	this->AddCommand("version", [](const ParamCollection &params) //"Print version information"
		{
			Json::Value result;
			result["version"] = ODC_VERSION_STRING;
			return result;
		},"Return the version information of opendatacon.");

	//Parse the configs and create all user interfaces, ports and connections
	ProcessFile();

	if(Interfaces.empty() && DataPorts.empty() && DataConnectors.empty())
		throw std::runtime_error("No objects to manage");

	for(auto& conn : DataConnectors)
		conn.second->SetIOS(pIOS);

	std::unordered_map<std::string,std::shared_ptr<IUIResponder>> PortResponders;
	for(auto& port : DataPorts)
	{
		port.second->SetIOS(pIOS);
		auto ResponderPair = port.second->GetUIResponder();
		//if it's a different, valid responder pair, store it
		if(ResponderPair.second && PortResponders.count(ResponderPair.first) == 0)
			PortResponders.insert(ResponderPair);
	}

	for(auto& interface : Interfaces)
	{
		interface.second->AddCommand("shutdown",[this](std::stringstream& ss)
			{
				std::thread([this](){this->Shutdown();}).detach();
			}
			,"Shutdown opendatacon");
		interface.second->AddCommand("version",[] (std::stringstream& ss)
			{
				std::cout<<"Release " << ODC_VERSION_STRING <<std::endl
				         <<"Submodules:"<<std::endl
				         <<"\t"<<ODC_VERSION_SUBMODULES<<std::endl;

			},"Print version information");
		interface.second->AddCommand("set_loglevel",[this] (std::stringstream& ss)
			{
				this->SetLogLevel(ss);
			},"Set the threshold for logging");

		interface.second->AddResponder("OpenDataCon", *this);
		interface.second->AddResponder("DataPorts", DataPorts);
		interface.second->AddResponder("DataConnectors", DataConnectors);
		interface.second->AddResponder("Plugins", Interfaces);
		for(auto& ResponderPair : PortResponders)
			interface.second->AddResponder(ResponderPair.first, *ResponderPair.second);
	}
}

DataConcentrator::~DataConcentrator()
{
	//In case of exception - ie. if we're destructed while still running
	//dump (detach) any threads
	//the threads would be already joined and cleared on normal shutdown
	for (auto& thread : threads)
		thread.detach();
	threads.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
	{
		//if there's a tcp sink, we need to destroy it
		//	because ostream will be destroyed
		//same for syslog, because asio::io_service will be destroyed
		for(const char* logger : {"tcp","syslog"})
		{
			if(LogSinksMap.count(logger))
			{
				//This doesn't look thread safe
				//	but we're on the main thread at this point
				//	the only other threads should be spdlog threads
				//	so if we flush first this should be safe...
				//BUT flush doesn't wait for the async Qs :-(
				//	it only flushes the sinks
				//	only thing to do is give some time for the Qs to empty
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				log->flush();
				auto tcp_sink_pos = std::find(log->sinks().begin(),log->sinks().end(),LogSinksMap[logger]);
				if(tcp_sink_pos != log->sinks().end())
				{
					log->sinks().erase(tcp_sink_pos);
				}
			}
		}
	}
}

void DataConcentrator::SetLogLevel(std::stringstream& ss)
{
	std::string sinkname;
	std::string level_str;
	if(ss>>sinkname && ss>>level_str)
	{
		for(auto& sink : LogSinksMap)
		{
			if(sink.first == sinkname)
			{
				auto new_level = spdlog::level::from_str(level_str);
				if(new_level == spdlog::level::off && level_str != "off")
				{
					std::cout << "Invalid log level. Options are:" << std::endl;
					for(uint8_t i = 0; i < 7; i++)
						std::cout << spdlog::level::level_string_views[i].data() << std::endl;
					return;
				}
				else
				{
					sink.second->set_level(new_level);
					return;
				}
			}
		}
	}
	std::cout << "Usage: set loglevel <sinkname> <level>" << std::endl;
	std::cout << "Sinks:" << std::endl;
	for(auto& sink : LogSinksMap)
		std::cout << sink.first << std::endl;
	std::cout << std::endl << "Levels:" << std::endl;
	for(uint8_t i = 0; i < 7; i++)
		std::cout << spdlog::level::level_string_views[i].data() << std::endl;
}

void DataConcentrator::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject())
		throw std::runtime_error("No valid JSON config object");

	//setup log sinks
	auto log_size_kb = JSONRoot.isMember("LogFileSizekB") ? JSONRoot["LogFileSizekB"].asUInt() : 5*1024;
	auto log_num = JSONRoot.isMember("NumLogFiles") ? JSONRoot["NumLogFiles"].asUInt() : 5;
	auto log_name = JSONRoot.isMember("LogName") ? JSONRoot["LogName"].asString() : "opendatacon_log";

	//TODO: document these config options
	auto log_level_name = JSONRoot.isMember("LogLevel") ? JSONRoot["LogLevel"].asString() : "info";
	auto console_level_name = JSONRoot.isMember("ConsoleLevel") ? JSONRoot["ConsoleLevel"].asString() : "err";

	//these return level::off if no match
	auto log_level = spdlog::level::from_str(log_level_name);
	auto console_level = spdlog::level::from_str(console_level_name);

	//check for no match and set defaults
	if(log_level == spdlog::level::off && log_level_name != "off")
		log_level = spdlog::level::info;
	if(console_level == spdlog::level::off && console_level_name != "off")
		console_level = spdlog::level::err;
	try
	{
		auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_name, log_size_kb*1024, log_num);
		auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

		file->set_level(log_level);
		console->set_level(console_level);

		LogSinksMap["file"] = file;
		LogSinksVec.push_back(file);
		LogSinksMap["console"] = console;
		LogSinksVec.push_back(console);

		//TODO: document these config options
		if(JSONRoot.isMember("SyslogLog"))
		{
			auto temp_logger = std::make_shared<spdlog::logger>("init", begin(LogSinksVec), end(LogSinksVec));

			auto SyslogJSON = JSONRoot["SyslogLog"];
			if(!SyslogJSON.isMember("Host"))
			{
				temp_logger->error("Invalid SyslogLog config: need at least 'Host': \n'{}\n' : ignoring", SyslogJSON.toStyledString());
			}
			else
			{
				auto host = SyslogJSON["Host"].asString();
				auto port = SyslogJSON.isMember("Port") ? SyslogJSON["Port"].asString() : "514";
				auto local_host = SyslogJSON.isMember("LocalHost") ? SyslogJSON["LocalHost"].asString() : "-";
				auto app = SyslogJSON.isMember("AppName") ? SyslogJSON["AppName"].asString() : "opendatacon";
				auto category = SyslogJSON.isMember("MsgCategory") ? SyslogJSON["MsgCategory"].asString() : "-";

				auto syslog_sink = std::make_shared<odc::asio_syslog_spdlog_sink>(
					*pIOS,host,port,1,local_host,app,category);
				syslog_sink->set_level(log_level);
				LogSinksMap["syslog"] = syslog_sink;
				LogSinksVec.push_back(syslog_sink);
			}
		}

		//TODO: document these config options
		if(JSONRoot.isMember("TCPLog"))
		{
			auto temp_logger = std::make_shared<spdlog::logger>("init", begin(LogSinksVec), end(LogSinksVec));

			auto TCPLogJSON = JSONRoot["TCPLog"];
			if(!TCPLogJSON.isMember("IP") || !TCPLogJSON.isMember("Port") || !TCPLogJSON.isMember("TCPClientServer"))
			{
				temp_logger->error("Invalid TCPLog config: need at least IP, Port, TCPClientServer: \n'{}\n' : ignoring", TCPLogJSON.toStyledString());
			}
			else
			{
				bool isServer = true;

				if(TCPLogJSON["TCPClientServer"].asString() == "CLIENT")
					isServer = false;
				else if(TCPLogJSON["TCPClientServer"].asString() != "SERVER")
					temp_logger->error("Invalid TCPLog TCPClientServer setting '{}'. Choose CLIENT or SERVER. Defaulting to SERVER.", TCPLogJSON["TCPClientServer"].asString());

				TCPbuf.Init(pIOS,isServer,TCPLogJSON["IP"].asString(),TCPLogJSON["Port"].asString());
				pTCPostream = std::make_unique<std::ostream>(&TCPbuf);
			}
		}

		if(pTCPostream)
		{
			auto tcp = std::make_shared<spdlog::sinks::ostream_sink_mt>(*pTCPostream.get(), true);
			tcp->set_level(log_level);
			LogSinksMap["tcp"] = tcp;
			LogSinksVec.push_back(tcp);
		}
		odc::spdlog_init_thread_pool(4096,3);
		auto pMainLogger = std::make_shared<spdlog::async_logger>("opendatacon", begin(LogSinksVec), end(LogSinksVec),
			odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
		pMainLogger->set_level(spdlog::level::trace);
		odc::spdlog_register_logger(pMainLogger);
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		throw std::runtime_error("Main logger initialization failed: " + std::string(ex.what()));
	}

	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

	log->critical("This is opendatacon version '{}'", ODC_VERSION_STRING);
	log->critical("Log level set to {}", spdlog::level::level_string_views[log_level]);
	log->critical("Console level set to {}", spdlog::level::level_string_views[console_level]);
	log->info("Loading configuration... ");

	//Configure the user interface
	if(JSONRoot.isMember("Plugins"))
	{
		log->info("Loading Plugins... ");

		const Json::Value Plugins = JSONRoot["Plugins"];

		for(Json::Value::ArrayIndex n = 0; n < Plugins.size(); ++n)
		{
			if(!Plugins[n].isMember("Type") || !Plugins[n].isMember("Name") || !Plugins[n].isMember("ConfFilename"))
			{
				log->error("Invalid plugin config: need at least Type, Name, ConfFilename: \n'{}\n' : ignoring", Plugins[n].toStyledString());
				continue;
			}

			auto PluginName = Plugins[n]["Name"].asString();
			if(Interfaces.count(PluginName) > 0)
			{
				log->error("Ignoring duplicate plugin name '{}'.", PluginName);
				continue;
			}

			//Looks for a specific library (for libs that implement more than one class)
			std::string libname;
			if(Plugins[n].isMember("Library"))
			{
				libname = Plugins[n]["Library"].asString();
			}
			//Otherwise use the naming convention lib<Type>Plugin.so to find the default lib that implements a type of plugin
			else
			{
				libname = Plugins[n]["Type"].asString();
			}
			std::string libfilename = GetLibFileName(libname);

			//try to load the lib
			auto* pluginlib = LoadModule(libfilename);

			if(pluginlib == nullptr)
			{
				log->error("{}",LastSystemError());
				log->error("{} : Dynamic library '{}' load failed skipping plugin...", PluginName, libfilename);
				continue;
			}

			//Our API says the library should export a creation function: IUI* new_<Type>Plugin(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of IUI
			std::string new_funcname = "new_"+Plugins[n]["Type"].asString()+"Plugin";
			auto new_plugin_func = (IUI*(*)(std::string, std::string, const Json::Value))LoadSymbol(pluginlib, new_funcname);

			std::string delete_funcname = "delete_"+Plugins[n]["Type"].asString()+"Plugin";
			auto delete_plugin_func = (void (*)(IUI*))LoadSymbol(pluginlib, delete_funcname);

			if(new_plugin_func == nullptr)
				log->info("{} : Failed to load symbol '{}' from library '{}' - {}", PluginName, new_funcname, libfilename, LastSystemError());
			if(delete_plugin_func == nullptr)
				log->info("{} : Failed to load symbol '{}' from library '{}' - {}", PluginName, delete_funcname, libfilename, LastSystemError());
			if(new_plugin_func == nullptr || delete_plugin_func == nullptr)
			{
				log->error("{} : Failed to load plugin, skipping...", PluginName);
				continue;
			}
			//Create a logger if we haven't already
			if(!odc::spdlog_get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::async_logger>(libname, begin(LogSinksVec), end(LogSinksVec),
					odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
				pLibLogger->set_level(spdlog::level::trace);
				odc::spdlog_register_logger(pLibLogger);
			}

			auto plugin_cleanup = [=](IUI* plugin)
						    {
							    delete_plugin_func(plugin);
							    UnLoadModule(pluginlib);
						    };

			//call the creation function and wrap the returned pointer to a new plugin
			Interfaces.emplace(PluginName, std::unique_ptr<IUI,decltype(plugin_cleanup)>(new_plugin_func(PluginName, Plugins[n]["ConfFilename"].asString(), Plugins[n]["ConfOverrides"]), plugin_cleanup));
		}
	}

	if(JSONRoot.isMember("Ports"))
	{
		const Json::Value Ports = JSONRoot["Ports"];

		for(Json::Value::ArrayIndex n = 0; n < Ports.size(); ++n)
		{
			if(!Ports[n].isMember("Type") || !Ports[n].isMember("Name") || !Ports[n].isMember("ConfFilename"))
			{
				log->error("Invalid port config: need at least Type, Name, ConfFilename: \n'{}\n' : ignoring", Ports[n].toStyledString());
				continue;
			}
			if(DataPorts.count(Ports[n]["Name"].asString()))
			{
				log->error("Duplicate Port Name; ignoring:\n'{}\n'", Ports[n].toStyledString());
				continue;
			}

			std::function<void (IOHandler*)> set_init_mode;
			if(Ports[n].isMember("InitState"))
			{
				if(Ports[n]["InitState"].asString() == "ENABLED")
				{
					set_init_mode = [](IOHandler* aIOH)
							    {
								    aIOH->InitState = InitState_t::ENABLED;
							    };
				}
				else if(Ports[n]["InitState"].asString() == "DISABLED")
				{
					set_init_mode = [](IOHandler* aIOH)
							    {
								    aIOH->InitState = InitState_t::DISABLED;
							    };
				}
				else if(Ports[n]["InitState"].asString() == "DELAYED")
				{
					uint16_t delay = 0;
					if(Ports[n].isMember("EnableDelayms"))
					{
						delay = Ports[n]["EnableDelayms"].asUInt();
					}
					else
					{
						log->error("Invalid Port config: Missing 'EnableDelayms':'\n{}\n' : defaulting to 0", Ports[n].toStyledString());
					}
					set_init_mode = [=](IOHandler* aIOH)
							    {
								    aIOH->InitState = InitState_t::DELAYED;
								    aIOH->EnableDelayms = delay;
							    };
				}
			}
			else
			{
				set_init_mode = [](IOHandler* aIOH){};
			}

			if(Ports[n]["Type"].asString() == "Null")
			{
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void (*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
				continue;
			}

			//Looks for a specific library (for libs that implement more than one class)
			std::string libname;
			if(Ports[n].isMember("Library"))
			{
				libname = Ports[n]["Library"].asString();
			}
			//Otherwise use the naming convention lib<Type>Port.so to find the default lib that implements a type of port
			else
			{
				libname = Ports[n]["Type"].asString()+"Port";
			}
			std::string libfilename(GetLibFileName(libname));

			//try to load the lib
			auto* portlib = LoadModule(libfilename);

			if(portlib == nullptr)
			{
				log->error("{}",LastSystemError());
				log->error("Failed to load library '{}' mapping {} to NullPort...", libfilename, Ports[n]["Name"].asString());
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void (*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
				continue;
			}

			//Create a logger if we haven't already
			if(!odc::spdlog_get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::async_logger>(libname, begin(LogSinksVec), end(LogSinksVec),
					odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
				pLibLogger->set_level(spdlog::level::trace);
				odc::spdlog_register_logger(pLibLogger);
			}

			//Our API says the library should export a creation function: DataPort* new_<Type>Port(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of DataPort
			std::string new_funcname = "new_"+Ports[n]["Type"].asString()+"Port";
			auto new_port_func = (DataPort*(*)(const std::string&, const std::string&, const Json::Value&))LoadSymbol(portlib, new_funcname);

			std::string delete_funcname = "delete_"+Ports[n]["Type"].asString()+"Port";
			auto delete_port_func = (void (*)(DataPort*))LoadSymbol(portlib, delete_funcname);

			if(new_port_func == nullptr)
				log->info("{} : Failed to load symbol '{}' from library '{}' - {}", Ports[n]["Name"].asString(), new_funcname, libfilename, LastSystemError());
			if(delete_port_func == nullptr)
				log->info("{} : Failed to load symbol '{}' from library '{}' - {}", Ports[n]["Name"].asString(), delete_funcname, libfilename, LastSystemError());

			if(new_port_func == nullptr || delete_port_func == nullptr)
			{
				log->error("{} : Failed to load port, mapping to NullPort...", Ports[n]["Name"].asString());
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void (*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
				continue;
			}

			auto port_cleanup = [=](DataPort* port)
						  {
							  delete_port_func(port);
							  UnLoadModule(portlib);
						  };

			//call the creation function and wrap the returned pointer to a new port
			DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,decltype(port_cleanup)>(new_port_func(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]), port_cleanup));
			set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
		}
	}

	if(JSONRoot.isMember("Connectors"))
	{
		const Json::Value Connectors = JSONRoot["Connectors"];

		//make a logger for use by Connectors
		auto pConnLogger = std::make_shared<spdlog::async_logger>("Connectors", begin(LogSinksVec), end(LogSinksVec),
			odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
		pConnLogger->set_level(spdlog::level::trace);
		odc::spdlog_register_logger(pConnLogger);

		for(Json::Value::ArrayIndex n = 0; n < Connectors.size(); ++n)
		{
			if(!Connectors[n].isMember("Name") || !Connectors[n].isMember("ConfFilename"))
			{
				log->error("Invalid Connector config: need at least Name, ConfFilename: \n'{}\n' : ignoring", Connectors[n].toStyledString());
				continue;
			}
			if(DataConnectors.count(Connectors[n]["Name"].asString()))
			{
				log->error("Duplicate Connector Name; ignoring:\n'{}\n'", Connectors[n].toStyledString());
				continue;
			}
			DataConnectors.emplace(Connectors[n]["Name"].asString(), std::unique_ptr<DataConnector,void (*)(DataConnector*)>(new DataConnector(Connectors[n]["Name"].asString(), Connectors[n]["ConfFilename"].asString(), Connectors[n]["ConfOverrides"]),[](DataConnector* pDC){delete pDC;}));
			if(Connectors[n].isMember("InitState"))
			{
				if(Connectors[n]["InitState"].asString() == "ENABLED")
				{
					DataConnectors.at(Connectors[n]["Name"].asString())->InitState = InitState_t::ENABLED;
				}
				else if(Connectors[n]["InitState"].asString() == "DISABLED")
				{
					DataConnectors.at(Connectors[n]["Name"].asString())->InitState = InitState_t::DISABLED;
				}
				else if(Connectors[n]["InitState"].asString() == "DELAYED")
				{
					uint16_t delay = 0;
					if(Connectors[n].isMember("EnableDelayms"))
					{
						delay = Connectors[n]["EnableDelayms"].asUInt();
					}
					else
					{
						log->error("Invalid Connector config: Missing 'EnableDelayms':'\n{}\n' : defaulting to 0", Connectors[n].toStyledString());
					}
					DataConnectors.at(Connectors[n]["Name"].asString())->InitState = InitState_t::DELAYED;
					DataConnectors.at(Connectors[n]["Name"].asString())->EnableDelayms = delay;
				}
			}
		}
	}
}
void DataConcentrator::Build()
{
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Initialising Interfaces...");
	for(auto& Name_n_UI : Interfaces)
	{
		Name_n_UI.second->Build();
	}
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Initialising DataPorts...");
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->Build();
	}
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Initialising DataConnectors...");
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->Build();
	}
}
void DataConcentrator::Run()
{
	if (auto log = odc::spdlog_get("opendatacon"))
		log->info("Starting worker threads...");
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		threads.emplace_back([&]() {pIOS->run(); });

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling DataConnectors...");
	for(auto& Name_n_Conn : DataConnectors)
	{
		if(Name_n_Conn.second->InitState == InitState_t::ENABLED)
		{
			pIOS->post([&]()
				{
					Name_n_Conn.second->Enable();
				});
		}
		else if(Name_n_Conn.second->InitState == InitState_t::DELAYED)
		{
			std::shared_ptr<asio::steady_timer> pTimer = pIOS->make_steady_timer();
			pTimer->expires_from_now(std::chrono::milliseconds(Name_n_Conn.second->EnableDelayms));
			pTimer->async_wait([pTimer,&Name_n_Conn](asio::error_code err_code)
				{
					//FIXME: check err_code?
					Name_n_Conn.second->Enable();
				});
		}
	}
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling DataPorts...");
	for(auto& Name_n_Port : DataPorts)
	{
		if(Name_n_Port.second->InitState == InitState_t::ENABLED)
		{
			pIOS->post([&]()
				{
					Name_n_Port.second->Enable();
				});
		}
		else if(Name_n_Port.second->InitState == InitState_t::DELAYED)
		{
			std::shared_ptr<asio::steady_timer> pTimer = pIOS->make_steady_timer();
			pTimer->expires_from_now(std::chrono::milliseconds(Name_n_Port.second->EnableDelayms));
			pTimer->async_wait([pTimer,&Name_n_Port](asio::error_code err_code)
				{
					//FIXME: check err_code?
					Name_n_Port.second->Enable();
				});
		}
	}
	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling Interfaces...");
	for(auto& Name_n_UI : Interfaces)
	{
		pIOS->post([&]()
			{
				Name_n_UI.second->Enable();
			});
	}

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Up and running.");

	pIOS->run();
	for(auto& thread : threads)
		thread.join();
	threads.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Destoying Interfaces...");
	Interfaces.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Destoying DataConnectors...");
	DataConnectors.clear();

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Destoying DataPorts...");
	DataPorts.clear();
	shut_down = true;
}

void DataConcentrator::Shutdown()
{
	//Shutdown gets called from various places (signal handling, user interface(s))
	//ensure we only act once
	std::call_once(shutdown_flag, [this]()
		{
			shutting_down = true;
			if(auto log = odc::spdlog_get("opendatacon"))
			{
			      log->critical("Shutting Down...");
			      log->info("Disabling Interfaces...");
			}
			for(auto& Name_n_UI : Interfaces)
			{
			      Name_n_UI.second->Disable();
			}
			if(auto log = odc::spdlog_get("opendatacon"))
				log->info("Disabling DataConnectors...");
			for(auto& Name_n_Conn : DataConnectors)
			{
			      Name_n_Conn.second->Disable();
			}
			if(auto log = odc::spdlog_get("opendatacon"))
				log->info("Disabling DataPorts...");
			for(auto& Name_n_Port : DataPorts)
			{
			      Name_n_Port.second->Disable();
			}

			if(auto log = odc::spdlog_get("opendatacon"))
			{
			      log->info("Finishing asynchronous tasks...");
			      log->flush(); //for the benefit of tcp logger shutdown
			}

			//shutdown tcp logger so it doesn't keep the io_service going
			TCPbuf.DeInit();
			if(LogSinksMap.count("tcp"))
				LogSinksMap["tcp"]->set_level(spdlog::level::off);

			ios_working.reset();
		});
}

bool DataConcentrator::isShuttingDown()
{
	return shutting_down;
}
bool DataConcentrator::isShutDown()
{
	return shut_down;
}
