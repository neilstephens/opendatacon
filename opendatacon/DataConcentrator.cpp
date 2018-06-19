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
#include <asiodnp3/ConsoleLogger.h>

#include <spdlog/spdlog.h>

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
		std::thread([&](){IOS.run();}).detach();

	//Parse the configs and create all user interfaces, ports and connections
	ProcessFile();

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

		interface.second->AddResponder("OpenDataCon", *this);
		interface.second->AddResponder("DataPorts", DataPorts);
		interface.second->AddResponder("DataConnectors", DataConnectors);
		interface.second->AddResponder("Loggers", AdvancedLoggers);
		interface.second->AddResponder("Plugins", Interfaces);
	}
	for(auto& port : DataPorts)
		port.second->SetIOS(&IOS);
	for(auto& conn : DataConnectors)
		conn.second->SetIOS(&IOS);
}

DataConcentrator::~DataConcentrator()
{}

void DataConcentrator::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	//setup log sinks
	auto log_size_kb = JSONRoot.isMember("LogFileSizekB") ? JSONRoot["LogFileSizekB"].asUInt() : 5*1024;
	auto log_num = JSONRoot.isMember("NumLogFiles") ? JSONRoot["NumLogFiles"].asUInt() : 5;
	auto log_name = JSONRoot.isMember("LogName") ? JSONRoot["LogName"].asString() : "datacon_log";
	auto log_level_name = JSONRoot.isMember("LogLevel") ? JSONRoot["LogLevel"].asString() : "info";

	auto log_level = spdlog::level::info;
	size_t level_number = spdlog::level::trace; //lowest log level
	do
	{
		if(log_level_name == spdlog::level::level_names[level_number])
		{
			log_level = static_cast<spdlog::level::level_enum>(level_number);
			break;
		}
	} while(level_number++ < spdlog::level::off); //highest log level

	//TODO: document these config options
	if(JSONRoot.isMember("TCPLog"))
	{
		auto TCPLogJSON = JSONRoot["TCPLog"];
		if(!TCPLogJSON.isMember("IP") || !TCPLogJSON.isMember("Port") || !TCPLogJSON.isMember("TCPClientServer"))
		{
			std::cout<<"Warning: invalid TCPLog config: need at least IP, Port, TCPClientServer: \n'"<<TCPLogJSON.toStyledString()<<"\n' : ignoring"<<std::endl;
		}
		else
		{
			bool isServer = true;

			if(TCPLogJSON["TCPClientServer"].asString() == "CLIENT")
				isServer = false;
			else if(TCPLogJSON["TCPClientServer"].asString() != "SERVER")
				std::cout<<"Warning: invalid TCPLog TCPClientServer setting. Choose CLIENT or SERVER. Defaulting to SERVER."<<std::endl;

			TCPbuf.Init(&IOS,isServer,TCPLogJSON["IP"].asString(),TCPLogJSON["Port"].asString());
			pTCPostream = std::make_unique<std::ostream>(&TCPbuf);
		}
	}

	try
	{
		size_t q_size = 4096;
		auto flush_interval = std::chrono::seconds(2);
		spdlog::set_async_mode(q_size, spdlog::async_overflow_policy::discard_log_msg, nullptr, flush_interval);
		spdlog::set_level(log_level);

		LogSinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_name, "txt", log_size_kb*1024, log_num));
		if(pTCPostream)
			LogSinks.push_back(std::make_shared<spdlog::sinks::ostream_sink_mt>(*pTCPostream.get()));
		auto pMainLogger = std::make_shared<spdlog::logger>("opendatacon", begin(LogSinks), end(LogSinks));
		spdlog::register_logger(pMainLogger);
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		throw std::runtime_error("Main logger initialization failed: " + std::string(ex.what()));
	}
	spdlog::get("opendatacon")->info("Log level set to {}", spdlog::level::level_names[log_level]);

	//Configure the user interface
	if(JSONRoot.isMember("Plugins"))
	{
		const Json::Value Plugins = JSONRoot["Plugins"];

		for(Json::Value::ArrayIndex n = 0; n < Plugins.size(); ++n)
		{
			if(!Plugins[n].isMember("Type") || !Plugins[n].isMember("Name") || !Plugins[n].isMember("ConfFilename"))
			{
				std::cout << "Warning: invalid plugin config: need at least Type, Name, ConfFilename: \n'" << Plugins[n].toStyledString() << "\n' : ignoring" << std::endl;
				continue;
			}

			auto PluginName = Plugins[n]["Name"].asString();
			if(Interfaces.count(PluginName) > 0)
			{
				std::cout << PluginName << " Warning: ignoring duplicate plugin name." << std::endl;
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
				std::cout << PluginName << " Info: dynamic library load failed '" << libfilename << "' skipping plugin..." << std::endl;
				std::cout << PluginName << " Error: failed to load plugin, skipping..." << std::endl;
				continue;
			}

			//Our API says the library should export a creation function: IUI* new_<Type>Plugin(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of IUI
			std::string new_funcname = "new_"+Plugins[n]["Type"].asString()+"Plugin";
			auto new_plugin_func = (IUI*(*)(std::string, std::string, const Json::Value))LoadSymbol(pluginlib, new_funcname);

			std::string delete_funcname = "delete_"+Plugins[n]["Type"].asString()+"Plugin";
			auto delete_plugin_func = (void (*)(IUI*))LoadSymbol(pluginlib, delete_funcname);

			if(new_plugin_func == nullptr)
			{
				std::cout << PluginName << " Info: failed to load symbol '" << new_funcname << "' from library '" << libfilename << "' - " << LastSystemError() << std::endl;
			}
			if(delete_plugin_func == nullptr)
			{
				std::cout << PluginName << " Info: failed to load symbol '" << delete_funcname << "' from library '" << libfilename << "' - " << LastSystemError() << std::endl;
			}
			if(new_plugin_func == nullptr || delete_plugin_func == nullptr)
			{
				std::cout << PluginName << " Error: failed to load plugin, skipping..." << std::endl;
				continue;
			}

			//call the creation function and wrap the returned pointer to a new plugin
			Interfaces.emplace(PluginName, std::unique_ptr<IUI,void (*)(IUI*)>(new_plugin_func(PluginName, Plugins[n]["ConfFilename"].asString(), Plugins[n]["ConfOverrides"]), delete_plugin_func));

			//Create a logger if we haven't already
			if(!spdlog::get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::logger>(libname, begin(LogSinks), end(LogSinks));
				spdlog::register_logger(pLibLogger);
			}
		}
	}

	if(JSONRoot.isMember("Ports"))
	{
		const Json::Value Ports = JSONRoot["Ports"];

		for(Json::Value::ArrayIndex n = 0; n < Ports.size(); ++n)
		{
			if(!Ports[n].isMember("Type") || !Ports[n].isMember("Name") || !Ports[n].isMember("ConfFilename"))
			{
				std::cout<<"Warning: invalid port config: need at least Type, Name, ConfFilename: \n'"<<Ports[n].toStyledString()<<"\n' : ignoring"<<std::endl;
				continue;
			}
			if(DataPorts.count(Ports[n]["Name"].asString()))
			{
				std::cout<<"Warning: Duplicate Port Name; ignoring:\n'"<<Ports[n].toStyledString()<<"\n'"<<std::endl;
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
						std::cout<<"Warning: invalid port config: missing 'EnableDelayms': \n'"<<Ports[n].toStyledString()<<"\n' : defaulting to 0"<<std::endl;
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
				std::cout << "Warning: failed to load library '"<<libfilename<<"' mapping to null port..."<<std::endl;
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
			{
				std::cout << "Warning: failed to load symbol '"<<new_funcname<<"' for port type '"<<Ports[n]["Type"].asString()<<"'"<<std::endl;
			}
			if(delete_port_func == nullptr)
			{
				std::cout << "Warning: failed to load symbol '"<<delete_funcname<<"' for port type '"<<Ports[n]["Type"].asString()<<"'"<<std::endl;
			}
			if(new_port_func == nullptr || delete_port_func == nullptr)
			{
				std::cout<<"Mapping '"<<Ports[n]["Type"].asString()<<"' to null port..."<<std::endl;
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void (*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
				continue;
			}

			//call the creation function and wrap the returned pointer to a new port
			DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void (*)(DataPort*)>(new_port_func(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]), delete_port_func));
			set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());

			//Create a logger if we haven't already
			if(!spdlog::get(libname))
			{
				auto pLibLogger = std::make_shared<spdlog::logger>(libname, begin(LogSinks), end(LogSinks));
				spdlog::register_logger(pLibLogger);
			}
		}
	}

	if(JSONRoot.isMember("Connectors"))
	{
		const Json::Value Connectors = JSONRoot["Connectors"];

		auto pConnLogger = std::make_shared<spdlog::logger>("Connectors", begin(LogSinks), end(LogSinks));
		spdlog::register_logger(pConnLogger);

		for(Json::Value::ArrayIndex n = 0; n < Connectors.size(); ++n)
		{
			if(!Connectors[n].isMember("Name") || !Connectors[n].isMember("ConfFilename"))
			{
				std::cout<<"Warning: invalid Connector config: need at least Name, ConfFilename: \n'"<<Connectors[n].toStyledString()<<"\n' : ignoring"<<std::endl;
				continue;
			}
			if(DataConnectors.count(Connectors[n]["Name"].asString()))
			{
				std::cout<<"Warning: Duplicate Connector Name; ignoring:\n'"<<Connectors[n].toStyledString()<<"\n'"<<std::endl;
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
						std::cout<<"Warning: invalid Connector config: missing 'EnableDelayms': \n'"<<Connectors[n].toStyledString()<<"\n' : defaulting to 0"<<std::endl;
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
	std::cout << "Initialising Interfaces" << std::endl;
	for(auto& Name_n_UI : Interfaces)
	{
		Name_n_UI.second->BuildOrRebuild();
	}
	std::cout << "Initialising DataPorts" << std::endl;
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->BuildOrRebuild();
	}
	std::cout << "Initialising DataConnectors" << std::endl;
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->BuildOrRebuild();
	}
}
void DataConcentrator::Run()
{
	std::cout << "Enabling DataConnectors... " << std::endl;
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
	std::cout << "Enabling DataPorts... " << std::endl;
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
	std::cout << "Enabling Interfaces... " << std::endl;
	for(auto& Name_n_UI : Interfaces)
	{
		IOS.post([&]()
			{
				Name_n_UI.second->Enable();
			});
	}

	std::cout << "Up and running." << std::endl;
	IOS.run();

	std::cout << "Destoying Interfaces... " << std::endl;
	Interfaces.clear();

	std::cout << "Destoying DataConnectors... " << std::endl;
	DataConnectors.clear();

	std::cout << "Destoying DataPorts... " << std::endl;
	DataPorts.clear();
}

void DataConcentrator::Shutdown()
{
	//Shutdown gets called from various places (signal handling, user interface(s))
	//ensure we only act once
	std::call_once(shutdown_flag, [this]()
		{
			std::cout << "Disabling user interfaces... " << std::endl;
			for(auto& Name_n_UI : Interfaces)
			{
			      Name_n_UI.second->Disable();
			}
			std::cout << "Disabling data connectors... " << std::endl;
			for(auto& Name_n_Conn : DataConnectors)
			{
			      Name_n_Conn.second->Disable();
			}
			std::cout << "Disabling data ports... " << std::endl;
			for(auto& Name_n_Port : DataPorts)
			{
			      Name_n_Port.second->Disable();
			}
			spdlog::get("opendatacon")->flush();
			TCPbuf.DeInit();
			std::cout << "Finishing asynchronous tasks... " << std::endl;
			ios_working.reset();
		});
}
