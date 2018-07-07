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
#include <spdlog/sinks/ostream_sink.h>

#include <opendatacon/Version.h>
#include "DataConcentrator.h"
#include "NullPort.h"

DataConcentrator::DataConcentrator(std::string FileName):
	ConfigParser(FileName),
	IOS(std::thread::hardware_concurrency()),
	ios_working(new asio::io_service::work(IOS)),
	pTCPostream(nullptr)
{
	// Enable loading of libraries
	InitLibaryLoading();

	//Version
	this->AddCommand("version", [this](const ParamCollection &params) //"Print version information"
		{
			Json::Value result;
			result["version"] = ODC_VERSION_STRING;
			return result;
		},"Return the version information of opendatacon.");

	//fire up some worker threads
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		threads.emplace_back([&](){IOS.run();});

	//Parse the configs and create all user interfaces, ports and connections
	ProcessFile();

	if(Interfaces.empty() && DataPorts.empty() && DataConnectors.empty())
		throw std::runtime_error("No objects to manage");

	for(auto& interface : Interfaces)
	{
		interface.second->AddCommand("shutdown",[this](std::stringstream& ss)
			{
				std::thread([this](){this->Shutdown();}).detach();
			}
			,"Shutdown opendatacon");
		interface.second->AddCommand("version",[] (std::stringstream& ss)
			{
				std::cout<<"Release " << ODC_VERSION_STRING <<std::endl;
			},"Print version information");
		interface.second->AddCommand("set_loglevel",[this] (std::stringstream& ss)
			{
				this->SetLogLevel(ss);
			},"Set the threshold for logging");

		interface.second->AddResponder("OpenDataCon", *this);
		interface.second->AddResponder("DataPorts", DataPorts);
		interface.second->AddResponder("DataConnectors", DataConnectors);
		interface.second->AddResponder("Plugins", Interfaces);
	}
	for(auto& port : DataPorts)
		port.second->SetIOS(&IOS);
	for(auto& conn : DataConnectors)
		conn.second->SetIOS(&IOS);
}

DataConcentrator::~DataConcentrator()
{}

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
						std::cout << spdlog::level::level_names[i] << std::endl;
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
		std::cout << spdlog::level::level_names[i] << std::endl;
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
		auto console = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();

		file->set_level(log_level);
		console->set_level(console_level);

		LogSinksMap["file"] = file;
		LogSinksVec.push_back(file);
		LogSinksMap["console"] = console;
		LogSinksVec.push_back(console);

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

				TCPbuf.Init(&IOS,isServer,TCPLogJSON["IP"].asString(),TCPLogJSON["Port"].asString());
				pTCPostream = std::make_unique<std::ostream>(&TCPbuf);
			}
		}

		if(pTCPostream)
		{
			auto tcp = std::make_shared<spdlog::sinks::ostream_sink_mt>(*pTCPostream.get());
			tcp->set_level(log_level);
			LogSinksMap["tcp"] = tcp;
			LogSinksVec.push_back(tcp);
		}
		auto pMainLogger = std::make_shared<spdlog::async_logger>("opendatacon", begin(LogSinksVec), end(LogSinksVec),
			4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
		pMainLogger->set_level(spdlog::level::trace);
		spdlog::register_logger(pMainLogger);
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		throw std::runtime_error("Main logger initialization failed: " + std::string(ex.what()));
	}

	auto log = spdlog::get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

	log->critical("This is opendatacon version '{}'", ODC_VERSION_STRING);
	log->critical("Log level set to {}", spdlog::level::level_names[log_level]);
	log->critical("Console level set to {}", spdlog::level::level_names[console_level]);
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
			if(!spdlog::get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::async_logger>(libname, begin(LogSinksVec), end(LogSinksVec),
					4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
				pLibLogger->set_level(spdlog::level::trace);
				spdlog::register_logger(pLibLogger);
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

			//Our API says the library should export a creation function: DataPort* new_<Type>Port(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of DataPort
			std::string new_funcname = "new_"+Ports[n]["Type"].asString()+"Port";
			auto new_port_func = (DataPort*(*)(std::string, std::string, const Json::Value))LoadSymbol(portlib, new_funcname);

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

			//Create a logger if we haven't already
			if(!spdlog::get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::async_logger>(libname, begin(LogSinksVec), end(LogSinksVec),
					4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
				pLibLogger->set_level(spdlog::level::trace);
				spdlog::register_logger(pLibLogger);
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
			4096, spdlog::async_overflow_policy::discard_log_msg, nullptr, std::chrono::seconds(2));
		pConnLogger->set_level(spdlog::level::trace);
		spdlog::register_logger(pConnLogger);

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
void DataConcentrator::BuildOrRebuild()
{
	if(auto log = spdlog::get("opendatacon"))
		log->info("Initialising Interfaces...");
	for(auto& Name_n_UI : Interfaces)
	{
		Name_n_UI.second->BuildOrRebuild();
	}
	if(auto log = spdlog::get("opendatacon"))
		log->info("Initialising DataPorts...");
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->BuildOrRebuild();
	}
	if(auto log = spdlog::get("opendatacon"))
		log->info("Initialising DataConnectors...");
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->BuildOrRebuild();
	}
}
void DataConcentrator::Run()
{
	if(auto log = spdlog::get("opendatacon"))
		log->info("Enabling DataConnectors...");
	for(auto& Name_n_Conn : DataConnectors)
	{
		if(Name_n_Conn.second->InitState == InitState_t::ENABLED)
		{
			IOS.post([&]()
				{
					Name_n_Conn.second->Enable();
				});
		}
		else if(Name_n_Conn.second->InitState == InitState_t::DELAYED)
		{
			auto pTimer = std::make_shared<asio::basic_waitable_timer<std::chrono::steady_clock>>(IOS);
			pTimer->expires_from_now(std::chrono::milliseconds(Name_n_Conn.second->EnableDelayms));
			pTimer->async_wait([pTimer,this,&Name_n_Conn](asio::error_code err_code)
				{
					//FIXME: check err_code?
					Name_n_Conn.second->Enable();
				});
		}
	}
	if(auto log = spdlog::get("opendatacon"))
		log->info("Enabling DataPorts...");
	for(auto& Name_n_Port : DataPorts)
	{
		if(Name_n_Port.second->InitState == InitState_t::ENABLED)
		{
			IOS.post([&]()
				{
					Name_n_Port.second->Enable();
				});
		}
		else if(Name_n_Port.second->InitState == InitState_t::DELAYED)
		{
			auto pTimer = std::make_shared<asio::basic_waitable_timer<std::chrono::steady_clock>>(IOS);
			pTimer->expires_from_now(std::chrono::milliseconds(Name_n_Port.second->EnableDelayms));
			pTimer->async_wait([pTimer,this,&Name_n_Port](asio::error_code err_code)
				{
					//FIXME: check err_code?
					Name_n_Port.second->Enable();
				});
		}
	}
	if(auto log = spdlog::get("opendatacon"))
		log->info("Enabling Interfaces...");
	for(auto& Name_n_UI : Interfaces)
	{
		IOS.post([&]()
			{
				Name_n_UI.second->Enable();
			});
	}

	if(auto log = spdlog::get("opendatacon"))
		log->info("Up and running.");

	IOS.run();
	for(auto& thread : threads)
		thread.join();
	threads.clear();

	if(auto log = spdlog::get("opendatacon"))
		log->info("Destoying Interfaces...");
	Interfaces.clear();

	if(auto log = spdlog::get("opendatacon"))
		log->info("Destoying DataConnectors...");
	DataConnectors.clear();

	if(auto log = spdlog::get("opendatacon"))
		log->info("Destoying DataPorts...");
	DataPorts.clear();
}

void DataConcentrator::Shutdown()
{
	//Shutdown gets called from various places (signal handling, user interface(s))
	//ensure we only act once
	std::call_once(shutdown_flag, [this]()
		{
			if(auto log = spdlog::get("opendatacon"))
				log->info("Disabling Interfaces...");
			for(auto& Name_n_UI : Interfaces)
			{
			      Name_n_UI.second->Disable();
			}
			if(auto log = spdlog::get("opendatacon"))
				log->info("Disabling DataConnectors...");
			for(auto& Name_n_Conn : DataConnectors)
			{
			      Name_n_Conn.second->Disable();
			}
			if(auto log = spdlog::get("opendatacon"))
				log->info("Disabling DataPorts...");
			for(auto& Name_n_Port : DataPorts)
			{
			      Name_n_Port.second->Disable();
			}
			if(auto log = spdlog::get("opendatacon"))
				log->flush();
			TCPbuf.DeInit();
			if(auto log = spdlog::get("opendatacon"))
				log->info("Finishing asynchronous tasks...");
			ios_working.reset();
		});
}
