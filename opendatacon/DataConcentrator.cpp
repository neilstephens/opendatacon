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
#include <asio.hpp>
#include <asiodnp3/ConsoleLogger.h>
#include <opendnp3/LogLevels.h>

#include <opendatacon/Version.h>

#include "DataConcentrator.h"
#include "logging_cmds.h"
#include "NullPort.h"

DataConcentrator::DataConcentrator(std::string FileName):
	ConfigParser(FileName),
	DNP3Mgr(std::thread::hardware_concurrency()),
	IOS(std::thread::hardware_concurrency()),
	ios_working(new asio::io_service::work(IOS)),
	LOG_LEVEL(opendnp3::levels::NORMAL),
	FileLog("datacon_log")
{
	// Enable loading of libraries
	InitLibaryLoading();

	//Version
	this->AddCommand("version", [this](const ParamCollection &params) { //"Print version information"
	                       Json::Value result;
	                       result["version"] = ODC_VERSION_STRING;
	                       return result;
			     },"Return the version information of opendatacon.");

	//fire up some worker threads
	for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
		std::thread([&](){IOS.run();}).detach();

	AdvancedLoggers.emplace("Console Log", std::unique_ptr<AdvancedLogger,void(*)(AdvancedLogger*)>(new AdvancedLogger(asiodnp3::ConsoleLogger::Instance(),LOG_LEVEL),[](AdvancedLogger* pAL){delete pAL;}));
	AdvancedLoggers.at("Console Log")->AddIngoreAlways(".*"); //silence all console messages by default
	DNP3Mgr.AddLogSubscriber(*AdvancedLoggers.at("Console Log").get());

	AdvancedLoggers.emplace("File Log", std::unique_ptr<AdvancedLogger,void(*)(AdvancedLogger*)>(new AdvancedLogger(FileLog,LOG_LEVEL),[](AdvancedLogger* pAL){delete pAL;}));
	DNP3Mgr.AddLogSubscriber(*AdvancedLoggers.at("File Log").get());


	//Parse the configs and create all user interfaces, ports and connections
	ProcessFile();

	for(auto& interface : Interfaces)
	{
		interface.second->AddCommand("shutdown",[this](std::stringstream& ss){this->Shutdown();},"Shutdown opendatacon");
		interface.second->AddCommand("version",[] (std::stringstream& ss){
		                                   std::cout<<"Release " << ODC_VERSION_STRING <<std::endl;
						     },"Print version information");

		interface.second->AddResponder("OpenDataCon", *this);
		interface.second->AddResponder("DataPorts", DataPorts);
		interface.second->AddResponder("DataConnectors", DataConnectors);
		interface.second->AddResponder("Loggers", AdvancedLoggers);
		interface.second->AddResponder("Plugins", Interfaces);
	}
	for(auto& port : DataPorts)
	{
		port.second->AddLogSubscriber(AdvancedLoggers.at("Console Log").get());
		port.second->AddLogSubscriber(AdvancedLoggers.at("File Log").get());
		port.second->SetIOS(&IOS);
		port.second->SetLogLevel(LOG_LEVEL);
	}
	for(auto& conn : DataConnectors)
	{
		conn.second->AddLogSubscriber(AdvancedLoggers.at("Console Log").get());
		conn.second->AddLogSubscriber(AdvancedLoggers.at("File Log").get());
		conn.second->SetIOS(&IOS);
		conn.second->SetLogLevel(LOG_LEVEL);
	}
}

DataConcentrator::~DataConcentrator()
{}

void DataConcentrator::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	//Configure the user interface
	if(!JSONRoot["Plugins"].isNull())
	{
		const Json::Value Plugins = JSONRoot["Plugins"];

		for(Json::Value::ArrayIndex n = 0; n < Plugins.size(); ++n)
		{
			if(Plugins[n]["Type"].isNull() || Plugins[n]["Name"].isNull() || Plugins[n]["ConfFilename"].isNull())
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
			if(!Plugins[n]["Library"].isNull())
			{
				libname = GetLibFileName(Plugins[n]["Library"].asString());
			}
			//Otherwise use the naming convention lib<Type>Plugin.so to find the default lib that implements a type of plugin
			else
			{
				libname = GetLibFileName(Plugins[n]["Type"].asString());
			}

			//try to load the lib
			auto* pluginlib = LoadModule(libname);

			if(pluginlib == nullptr)
			{
				std::cout << PluginName << " Info: dynamic library load failed '" << libname << "' skipping plugin..." << std::endl;
				std::cout << PluginName << " Error: failed to load plugin, skipping..." << std::endl;
				continue;
			}

			//Our API says the library should export a creation function: IUI* new_<Type>Plugin(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of IUI
			std::string new_funcname = "new_"+Plugins[n]["Type"].asString()+"Plugin";
			auto new_plugin_func = (IUI*(*)(std::string, std::string, const Json::Value))LoadSymbol(pluginlib, new_funcname);

			std::string delete_funcname = "delete_"+Plugins[n]["Type"].asString()+"Plugin";
			auto delete_plugin_func = (void(*)(IUI*))LoadSymbol(pluginlib, delete_funcname);

			if(new_plugin_func == nullptr)
			{
				std::cout << PluginName << " Info: failed to load symbol '" << new_funcname << "' from library '" << libname << "' - " << LastSystemError() << std::endl;
			}
			if(delete_plugin_func == nullptr)
			{
				std::cout << PluginName << " Info: failed to load symbol '" << delete_funcname << "' from library '" << libname << "' - " << LastSystemError() << std::endl;
			}
			if(new_plugin_func == nullptr || delete_plugin_func == nullptr)
			{
				std::cout << PluginName << " Error: failed to load plugin, skipping..." << std::endl;
				continue;
			}

			//call the creation function and wrap the returned pointer to a new plugin
			Interfaces.emplace(PluginName, std::unique_ptr<IUI,void(*)(IUI*)>(new_plugin_func(PluginName, Plugins[n]["ConfFilename"].asString(), Plugins[n]["ConfOverrides"]), delete_plugin_func));
		}
	}

	if(!JSONRoot["LogFileSizekB"].isNull())
		FileLog.SetLogFileSizekB(JSONRoot["LogFileSizekB"].asUInt());

	if(!JSONRoot["NumLogFiles"].isNull())
		FileLog.SetNumLogFiles(JSONRoot["NumLogFiles"].asUInt());

	if(!JSONRoot["LogName"].isNull())
		FileLog.SetLogName(JSONRoot["LogName"].asString());

	if(!JSONRoot["LOG_LEVEL"].isNull())
	{
		std::string value = JSONRoot["LOG_LEVEL"].asString();
		if(value == "ALL")
			LOG_LEVEL = opendnp3::levels::ALL;
		else if(value == "ALL_COMMS")
			LOG_LEVEL = opendnp3::levels::ALL_COMMS;
		else if(value == "NORMAL")
			LOG_LEVEL = opendnp3::levels::NORMAL;
		else if(value == "NOTHING")
			LOG_LEVEL = opendnp3::levels::NOTHING;
		else
			std::cout << "Warning: invalid LOG_LEVEL setting: '" << value << "' : ignoring and using 'NORMAL' log level." << std::endl;
		AdvancedLoggers.at("File Log")->SetLogLevel(LOG_LEVEL);
		AdvancedLoggers.at("Console Log")->SetLogLevel(LOG_LEVEL);
	}

	if(!JSONRoot["Ports"].isNull())
	{
		const Json::Value Ports = JSONRoot["Ports"];

		for(Json::Value::ArrayIndex n = 0; n < Ports.size(); ++n)
		{
			if(Ports[n]["Type"].isNull() || Ports[n]["Name"].isNull() || Ports[n]["ConfFilename"].isNull())
			{
				std::cout<<"Warning: invalid port config: need at least Type, Name, ConfFilename: \n'"<<Ports[n].toStyledString()<<"\n' : ignoring"<<std::endl;
				continue;
			}
			if(Ports[n]["Type"].asString() == "Null")
			{
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void(*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				continue;
			}

			//Looks for a specific library (for libs that implement more than one class)
			std::string libname;
			if(!Ports[n]["Library"].isNull())
			{
				libname = GetLibFileName(Ports[n]["Library"].asString());
			}
			//Otherwise use the naming convention lib<Type>Port.so to find the default lib that implements a type of port
			else
			{
				libname = GetLibFileName(Ports[n]["Type"].asString()+"Port");
			}

			//try to load the lib
			auto* portlib = LoadModule(libname.c_str());

			if(portlib == nullptr)
			{
				std::cout << "Warning: failed to load library '"<<libname<<"' mapping to null port..."<<std::endl;
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void(*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				continue;
			}

			//Our API says the library should export a creation function: DataPort* new_<Type>Port(Name, Filename, Overrides)
			//it should return a pointer to a heap allocated instance of a descendant of DataPort
			std::string new_funcname = "new_"+Ports[n]["Type"].asString()+"Port";
			auto new_port_func = (DataPort*(*)(std::string, std::string, const Json::Value))LoadSymbol(portlib, new_funcname);

			std::string delete_funcname = "delete_"+Ports[n]["Type"].asString()+"Port";
			auto delete_port_func = (void(*)(DataPort*))LoadSymbol(portlib, delete_funcname);

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
				DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void(*)(DataPort*)>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
				continue;
			}

			//call the creation function and wrap the returned pointer to a new port
			DataPorts.emplace(Ports[n]["Name"].asString(), std::unique_ptr<DataPort,void(*)(DataPort*)>(new_port_func(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]), delete_port_func));
		}
	}

	if(!JSONRoot["Connectors"].isNull())
	{
		const Json::Value Connectors = JSONRoot["Connectors"];

		for(Json::Value::ArrayIndex n = 0; n < Connectors.size(); ++n)
		{
			if(Connectors[n]["Name"].isNull() || Connectors[n]["ConfFilename"].isNull())
			{
				std::cout<<"Warning: invalid Connector config: need at least Name, ConfFilename: \n'"<<Connectors[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}
			DataConnectors.emplace(Connectors[n]["Name"].asString(), std::unique_ptr<DataConnector,void(*)(DataConnector*)>(new DataConnector(Connectors[n]["Name"].asString(), Connectors[n]["ConfFilename"].asString(), Connectors[n]["ConfOverrides"]),[](DataConnector* pDC){delete pDC;}));
		}
	}
}
void DataConcentrator::BuildOrRebuild()
{
	std::cout << "User Interfaces" << std::endl;
	for(auto& Name_n_UI : Interfaces)
	{
		// TODO: BuildOrRebuild for UserInterfaces
		// Name_n_UI.second->BuildOrRebuild(DNP3Mgr,LOG_LEVEL);
	}
	std::cout << "Ports" << std::endl;
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->BuildOrRebuild(DNP3Mgr,LOG_LEVEL);
	}
	std::cout << "Connectors" << std::endl;
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->BuildOrRebuild(DNP3Mgr,LOG_LEVEL);
	}
}
void DataConcentrator::Run()
{
	for(auto& Name_n_UI : Interfaces)
	{
		IOS.post([&]()
		         {
		               Name_n_UI.second->Enable();
			   });
	}
	for(auto& Name_n_Conn : DataConnectors)
	{
		IOS.post([&]()
		         {
		               Name_n_Conn.second->Enable();
			   });
	}
	for(auto& Name_n_Port : DataPorts)
	{
		IOS.post([&]()
		         {
		               Name_n_Port.second->Enable();
			   });
	}

	IOS.run();
	std::cout << "done" << std::endl;

	std::cout << "Destoying Interfaces... ";
	Interfaces.clear();
	std::cout << "done" << std::endl;

	std::cout << "Destoying DataConnectors... ";
	DataConnectors.clear();
	std::cout << "done" << std::endl;

	std::cout << "Destoying DataPorts... ";
	DataPorts.clear();
	std::cout << "done" << std::endl;

	std::cout << "Shutting down DNP3 manager... ";
	DNP3Mgr.Shutdown();
	std::cout << "done" << std::endl;
}

void DataConcentrator::RestartPortOrConn(std::stringstream& args)
{
	EnablePortOrConn(args,false);
	EnablePortOrConn(args,true);
}
void DataConcentrator::EnablePortOrConn(std::stringstream& args, bool enable)
{
	std::string arg = "";
	std::string mregex;
	std::regex reg;
	if(!extract_delimited_string(args,mregex))
	{
		std::cout<<"Syntax error: Delimited regex expected, found \"..."<<mregex<<"\""<<std::endl;
		return;
	}
	try
	{
		reg = std::regex(mregex);
	}
	catch(std::exception& e)
	{
		std::cout<<e.what()<<std::endl;
		return;
	}
	for(auto& Name_n_UI : Interfaces)
	{
		if(std::regex_match(Name_n_UI.first, reg))
			enable ? Name_n_UI.second->Enable() : Name_n_UI.second->Disable();
	}
	for(auto& Name_n_Conn : DataConnectors)
	{
		if(std::regex_match(Name_n_Conn.first, reg))
			enable ? Name_n_Conn.second->Enable() : Name_n_Conn.second->Disable();
	}
	for(auto& Name_n_Port : DataPorts)
	{
		if(std::regex_match(Name_n_Port.first, reg))
			enable ? Name_n_Port.second->Enable() : Name_n_Port.second->Disable();
	}
}

void DataConcentrator::Shutdown()
{
	std::cout << "done" << std::endl << "Disabling user interfaces... ";
	for(auto& Name_n_UI : Interfaces)
	{
		Name_n_UI.second->Disable();
	}
	std::cout << "done" << std::endl << "Disabling data connectors... ";
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->Disable();
	}
	std::cout << "done" << std::endl << "Disabling data ports... ";
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->Disable();
	}
	std::cout << "done" << std::endl << "Finishing asynchronous tasks... ";
	ios_working.reset();
}
