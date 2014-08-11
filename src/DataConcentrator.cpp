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
#include <opendnp3/LogLevels.h>

#include "DataConcentrator.h"
#include "Console.h"

#include <asiodnp3/ConsoleLogger.h>
#include "logging_cmds.h"
#include "DNP3OutstationPort.h"
#include "DNP3MasterPort.h"
#include "../JSONPort/JSONClientPort.h"
#include "NullPort.h"

DataConcentrator::DataConcentrator(std::string FileName):
	DNP3Mgr(std::thread::hardware_concurrency()),
	LOG_LEVEL(openpal::logflags::WARN),
	AdvConsoleLog(asiodnp3::ConsoleLogger::Instance(),LOG_LEVEL),
	FileLog("datacon_log"),
	AdvFileLog(FileLog,LOG_LEVEL),
	IOS(std::thread::hardware_concurrency()),
	ios_working(new asio::io_service::work(IOS))
{
	//fire up some worker threads
	for(size_t i=0; i < std::thread::hardware_concurrency(); ++i)
		std::thread([&](){IOS.run();}).detach();

	AdvConsoleLog.AddIngoreAlways(".*"); //silence all console messages by default
	DNP3Mgr.AddLogSubscriber(&AdvConsoleLog);
	DNP3Mgr.AddLogSubscriber(&AdvFileLog);

	//Parse the configs and create all the ports and connections
	ProcessFile(FileName);

	for(auto& port : DataPorts)
	{
		port.second->AddLogSubscriber(&AdvConsoleLog);
		port.second->AddLogSubscriber(&AdvFileLog);
		port.second->SetIOS(&IOS);
		port.second->SetLogLevel(LOG_LEVEL);
	}
	for(auto& conn : DataConnectors)
	{
		conn.second->AddLogSubscriber(&AdvConsoleLog);
		conn.second->AddLogSubscriber(&AdvFileLog);
		conn.second->SetIOS(&IOS);
		conn.second->SetLogLevel(LOG_LEVEL);
	}
}
DataConcentrator::~DataConcentrator()
{
	//turn everything off
	this->Shutdown();
	DNP3Mgr.Shutdown();
	//tell the io service to let it's run functions return once there's no handlers left (letting our threads end)
	ios_working.reset();
	//help finish any work
	IOS.run();
}

void DataConcentrator::ProcessElements(const Json::Value& JSONRoot)
{
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
	}

	if(!JSONRoot["Ports"].isNull())
	{
		const Json::Value Ports = JSONRoot["Ports"];

		for(Json::Value::ArrayIndex n = 0; n < Ports.size(); ++n)
		{
			if(Ports[n]["Type"].isNull() || Ports[n]["Name"].isNull() || Ports[n]["ConfFilename"].isNull())
			{
				std::cout<<"Warning: invalid port config: need at least Type, Name, ConfFilename: \n'"<<Ports[n].toStyledString()<<"\n' : ignoring"<<std::endl;
			}
			if(Ports[n]["Type"].asString() == "DNP3Outstation")
			{
				DataPorts[Ports[n]["Name"].asString()] = std::unique_ptr<DataPort>(new DNP3OutstationPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"].asString()));
			}
			else if(Ports[n]["Type"].asString() == "DNP3Master")
			{
				DataPorts[Ports[n]["Name"].asString()] = std::unique_ptr<DataPort>(new DNP3MasterPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"].asString()));
			}
			else if(Ports[n]["Type"].asString() == "JSONClient")
			{
				DataPorts[Ports[n]["Name"].asString()] = std::unique_ptr<DataPort>(new JSONClientPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"].asString()));
			}
			else if(Ports[n]["Type"].asString() == "Null")
			{
				DataPorts[Ports[n]["Name"].asString()] = std::unique_ptr<DataPort>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"].asString()));
			}
			else
			{
				//TODO: unrecognised port type should default to seaching for a plugin.
				continue;
			}
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
			DataConnectors[Connectors[n]["Name"].asString()] = std::unique_ptr<DataConnector>(new DataConnector(Connectors[n]["Name"].asString(), Connectors[n]["ConfFilename"].asString(), Connectors[n]["ConfOverrides"].asString()));
		}
	}
}
void DataConcentrator::BuildOrRebuild()
{
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->BuildOrRebuild(DNP3Mgr,LOG_LEVEL);
	}
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->BuildOrRebuild(DNP3Mgr,LOG_LEVEL);
	}
}
void DataConcentrator::Run()
{
	for(auto& Name_n_Conn : DataConnectors)
	{
		IOS.post([=]()
		{
			Name_n_Conn.second->Enable();
		});
	}
	for(auto& Name_n_Port : DataPorts)
	{
		IOS.post([=]()
		{
			Name_n_Port.second->Enable();
		});
	}

	Console console("odc> ");

	std::function<void (std::stringstream&)> bound_func;

	//console logging control
	bound_func = std::bind(cmd_ignore_message,std::placeholders::_1,std::ref(AdvConsoleLog));
	console.AddCmd("ignore_message",bound_func,"Enter regex to silence matching messages from the console logger.");
	bound_func = std::bind(cmd_unignore_message,std::placeholders::_1,std::ref(AdvConsoleLog));
	console.AddCmd("unignore_message",bound_func,"Enter regex to remove from the console ignore list.");
	bound_func = std::bind(cmd_show_ignored,std::placeholders::_1,std::ref(AdvConsoleLog));
	console.AddCmd("show_ignored",bound_func,"Shows all console message ignore regexes and how many messages they've matched.");

	//file logging control
	bound_func = std::bind(cmd_ignore_message,std::placeholders::_1,std::ref(AdvFileLog));
	console.AddCmd("ignore_file_message",bound_func,"Enter regex to silence matching messages from the file logger.");
	bound_func = std::bind(cmd_unignore_message,std::placeholders::_1,std::ref(AdvFileLog));
	console.AddCmd("unignore_file_message",bound_func,"Enter regex to remove from the file ignore list.");
	bound_func = std::bind(cmd_show_ignored,std::placeholders::_1,std::ref(AdvFileLog));
	console.AddCmd("show_file_ignored",bound_func,"Shows all file message ignore regexes and how many messages they've matched.");

	console.run();
	Shutdown();
}
void DataConcentrator::Shutdown()
{
	for(auto& Name_n_Port : DataPorts)
	{
		Name_n_Port.second->Disable();
	}
	for(auto& Name_n_Conn : DataConnectors)
	{
		Name_n_Conn.second->Disable();
	}
}
