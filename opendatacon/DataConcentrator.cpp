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

#include "DataConcentrator.h"
#include "NullPort.h"
#include <opendatacon/Version.h>
#include <opendatacon/asio.h>
#include <opendatacon/asio_syslog_spdlog_sink.h>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <spdlog/async.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>
#include <thread>
#include <set>

/*
   Whenever needed we will ask for all the Sinks present,
   I don't like this personally, but due to the limitations with spdlogs,
   we need to create sinks only once can't add on the fly.
 */
inline std::vector<spdlog::sink_ptr> GetAllSinks(const std::unordered_map<std::string, spdlog::sink_ptr>& sinks)
{
	std::vector<spdlog::sink_ptr> sink_vec;
	for (auto it = sinks.begin(); it != sinks.end(); ++it)
		sink_vec.push_back(it->second);
	return sink_vec;
}

inline void AddLogger(const std::string& name, const std::unordered_map<std::string, spdlog::sink_ptr>& sinks)
{
	const std::vector<spdlog::sink_ptr> sink_vec = GetAllSinks(sinks);
	auto pLogger = std::make_shared<spdlog::async_logger>(name, sink_vec.begin(), sink_vec.end(),
		odc::spdlog_thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
	pLogger->set_level(spdlog::level::trace);
	odc::spdlog_register_logger(pLogger);
}

//Drop and reload all the loggers
//This may cause some log messages to be lost...
inline void ReloadLogSinks(const std::unordered_map<std::string, spdlog::sink_ptr>& sinks)
{
	std::vector<std::string> lognames;
	odc::spdlog_apply_all([&lognames](std::shared_ptr<spdlog::logger> log)
		{
			lognames.push_back(log->name());
		});
	odc::spdlog_drop_all();
	for(const auto& name : lognames)
		AddLogger(name, sinks);

	odc::spdlog_flush_every(std::chrono::seconds(60));
}

DataConcentrator::DataConcentrator(const std::string& FileName):
	ConfigParser(FileName),
	pIOS(odc::asio_service::Get()),
	ios_working(pIOS->make_work()),
	shutting_down(false),
	shut_down(false)
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

	RefreshIUIResponders();

	for(auto& interface : Interfaces)
		PrepInterface(interface.second);
}

void DataConcentrator::RefreshIUIResponders()
{
	RespondersMasterCopy.clear();
	RespondersMasterCopy["OpenDataCon"] = this;
	RespondersMasterCopy["DataPorts"] = &DataPorts;
	RespondersMasterCopy["DataConnectors"] = &DataConnectors;
	RespondersMasterCopy["Plugins"] = &Interfaces;
	for(auto& port : DataPorts)
	{
		auto ResponderPair = port.second->GetUIResponder();
		//if it's a different, valid responder pair, store it
		if(ResponderPair.second && RespondersMasterCopy.count(ResponderPair.first) == 0)
			RespondersMasterCopy.insert(ResponderPair);
	}
}

void DataConcentrator::PrepInterface(std::shared_ptr<IUI> interface)
{
	interface->AddCommand("shutdown",[this](std::stringstream& ss)
		{
			std::thread([this](){this->Shutdown();}).detach();
		}
		,"Shutdown opendatacon");
	interface->AddCommand("reload_config",[this](std::stringstream& ss)
		{
			std::string filename;
			if(!(ss >> filename))
				filename = "";
			std::thread([this,f{std::move(filename)}](){this->ReloadConfig(f);}).detach();
		}
		,"Reload config file(s). Detects changed or new Ports, Connectors and log levels. Usage: reload_config [<optional_filename>]");
	interface->AddCommand("version",[] (std::stringstream& ss)
		{
			std::cout<<"Release " << ODC_VERSION_STRING <<std::endl
			         <<"Submodules:"<<std::endl
			         <<"\t"<<ODC_VERSION_SUBMODULES<<std::endl;

		},"Print version information");
	interface->AddCommand("set_loglevel",[this] (std::stringstream& ss)
		{
			this->SetLogLevel(ss);
		},"Set the threshold for logging");
	interface->AddCommand("flush_logs",[] (std::stringstream& ss)
		{
			odc::spdlog_flush_all();
		},"Flush all registered loggers and sinks");
	interface->AddCommand("add_logsink",[this] (std::stringstream& ss)
		{
			this->AddLogSink(ss);
		},"Add a log sink. WARNING: May cause missed log messages momentarily");
	interface->AddCommand("del_logsink",[this] (std::stringstream& ss)
		{
			this->DeleteLogSink(ss);
		},"Delete a log sink. WARNING: May cause missed log messages momentarily");
	interface->AddCommand("ls_logsink",[this] (std::stringstream& ss)
		{
			this->ListLogSinks();
		},"List the names of all the log sinks");

	interface->SetResponders(RespondersMasterCopy);
}

DataConcentrator::~DataConcentrator()
{
	/*
	  If there's a TCP sink, we need to destroy it
	  because ostream will be destroyed
	  Same for the syslog, because asio::io_service will be destroyed
	  so let's destroy all except main console sink and file sink

	  flush doesn't wait for the async Qs: '(
	  it will only flushes the sinks
	  only thing to do is give some time for the Qs to empty
	 */

	if (auto log = odc::spdlog_get("opendatacon"))
	{
		log->info("Destroying user log sinks");
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		log->flush();
	}

	//For detecting when erased sinks are destroyed
	std::vector<std::weak_ptr<spdlog::sinks::sink>> weak_sinks;

	std::vector<std::string> sink_names;
	for (auto it = LogSinks.cbegin(); it != LogSinks.cend(); ++it)
		sink_names.push_back(it->first);
	for (const std::string& sink_name : sink_names)
	{
		if (!(sink_name == "file" || sink_name == "console"))
		{
			weak_sinks.push_back(LogSinks[sink_name]);
			LogSinks.erase(sink_name);
		}
	}

	ReloadLogSinks(LogSinks);

	// Wait for all the sinks to be destroyed
	for (auto weak_sink : weak_sinks)
		while (!weak_sink.expired())
			;
}

//FIXME: the following command functions shouldn't really print to the console,
//	they should return values to be compatible with non-cosole UIs
inline void DataConcentrator::ListLogSinks()
{
	std::cout << "Sinks:" << std::endl;
	for (auto it = LogSinks.begin(); it != LogSinks.end(); ++it)
		std::cout << "\t" << it->first << std::endl;
}
inline void DataConcentrator::ListLogLevels()
{
	std::cout << "Levels:" << std::endl;
	for(uint8_t i = 0; i < 7; i++)
		std::cout << "\t" << spdlog::level::level_string_views[i].data() << std::endl;
}


void DataConcentrator::SetLogLevel(std::stringstream& ss)
{
	std::string sinkname;
	std::string level_str;
	if(ss>>sinkname && ss>>level_str)
	{
		bool valid_name = false;
		for(const auto& sink : LogSinks)
			if(sink.first == sinkname)
				valid_name = true;

		bool fail = false;

		if (!valid_name)
		{
			fail = true;
			std::cout << "Log sink not found." << std::endl;
			ListLogSinks();
		}
		auto new_level = spdlog::level::from_str(level_str);
		if(new_level == spdlog::level::off && level_str != "off")
		{
			fail = true;
			std::cout << "Invalid log level." << std::endl;
			ListLogLevels();
		}

		if(!fail)
			LogSinks[sinkname]->set_level(new_level);

		return;
	}
	std::cout << "Usage: set_loglevel <sinkname> <level>" << std::endl;
	ListLogSinks();
	ListLogLevels();
}

void DataConcentrator::AddLogSink(std::stringstream& ss)
{
	std::string sinkname;
	std::string sinklevel;
	std::string sinktype;
	auto log = odc::spdlog_get("opendatacon");

	if(ss>>sinkname && ss>>sinklevel && ss>>sinktype)
	{
		if(LogSinks.find(sinkname) == LogSinks.end())
		{
			std::stringstream level_params;
			level_params << sinkname << " " << sinklevel;

			if(sinktype == "SYSLOG")
			{
				std::string host;
				if(ss>>host)
				{
					std::string port = "514";
					std::string local_host = "-";
					std::string app = "opendatacon";
					std::string category = "-";

					if(ss>>port)
						if(ss>>local_host)
							if(ss>>app)
								ss>>category;

					auto syslog_sink = std::make_shared<odc::asio_syslog_spdlog_sink>(
						*pIOS,host,port,1,local_host,app,category);
					syslog_sink->set_level(spdlog::level::off);
					LogSinks[sinkname] = syslog_sink;
				}
				else
				{
					std::cout << "Usage: add_logsink <sinkname> <level> SYSLOG <host> [ <port> [ <localhost> [ <appname> [ <category> ]]]]" << std::endl;
				}
			}
			else if(sinktype == "TCP")
			{
				std::string host;
				std::string tcp_port;
				std::string client_server;
				if (ss >> host && ss >> tcp_port && ss >> client_server)
				{
					TCPbufs[sinkname].Init(pIOS, client_server == "CLIENT" ? false : true, host, tcp_port);
					pTCPostreams[sinkname] = std::make_unique<std::ostream>(&TCPbufs[sinkname]);

					if(pTCPostreams[sinkname])
					{
						auto tcp = std::make_shared<spdlog::sinks::ostream_sink_mt>(*pTCPostreams[sinkname], true);
						tcp->set_level(spdlog::level::off);
						LogSinks[sinkname] = tcp;
					}
				}
				else
				{
					std::cout << "Usage: add_logsink <sinkname> <level> TCP <host> <port> <client / server>" << std::endl;
				}
			}
			else if(sinktype == "FILE")
			{
				//TODO: implement
				std::cout << "Not implemented." << std::endl;
			}
			else
			{
				std::cout << "Usage: add_logsink <sinkname> <level> <TCP|FILE|SYSLOG> ..." << std::endl;
				return;
			}
			SetLogLevel(level_params);
		}
		else
		{
			std::cout << "Error: [" << sinkname << "] Log sink name already taken." << std::endl;
			ListLogSinks();
			return;
		}
	}
	else
	{
		std::cout << "Usage: add_logsink <sinkname> <level> <TCP|FILE|SYSLOG> ..." << std::endl;
	}

	ReloadLogSinks(LogSinks);
}

void DataConcentrator::DeleteLogSink(std::stringstream& ss)
{
	std::string sinkname;
	if(ss>>sinkname)
	{
		if(LogSinks.find(sinkname) != LogSinks.end())
		{
			LogSinks.erase(sinkname);
		}
		else
		{
			std::cout << "Log sink name doesn't exist." << std::endl;
			ListLogSinks();
			return;
		}
	}
	else
	{
		std::cout << "Usage: del_logsink <sinkname>" << std::endl;
	}

	ReloadLogSinks(LogSinks);
}

std::pair<spdlog::level::level_enum,spdlog::level::level_enum> DataConcentrator::ConfigureLogSinks(const Json::Value& JSONRoot)
{
	//delete old ones in case this is a re-load
	LogSinks.erase("tcp");
	LogSinks.erase("syslog");
	LogSinks.erase("file");
	LogSinks.erase("console");
	ReloadLogSinks(LogSinks);
	auto tcp_buf_it = TCPbufs.find("tcp");
	if(tcp_buf_it != TCPbufs.end())
		tcp_buf_it->second.DeInit();
	pTCPostreams.erase("tcp");
	TCPbufs.erase("tcp");

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

	auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_name, log_size_kb*1024, log_num);
	auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

	file->set_level(log_level);
	console->set_level(console_level);

	LogSinks["file"] = file;
	LogSinks["console"] = console;

	//TODO: document these config options
	if(JSONRoot.isMember("SyslogLog"))
	{
		const std::vector<spdlog::sink_ptr> sink_vec = GetAllSinks(LogSinks);
		auto temp_logger = std::make_shared<spdlog::logger>("init", begin(sink_vec), end(sink_vec));

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
			LogSinks["syslog"] = syslog_sink;
		}
	}

	//TODO: document these config options
	if(JSONRoot.isMember("TCPLog"))
	{
		const std::vector<spdlog::sink_ptr> sink_vec = GetAllSinks(LogSinks);
		auto temp_logger = std::make_shared<spdlog::logger>("init", begin(sink_vec), end(sink_vec));

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

			TCPbufs["tcp"].Init(pIOS, isServer, TCPLogJSON["IP"].asString(), TCPLogJSON["Port"].asString());
			pTCPostreams["tcp"] = std::make_unique<std::ostream>(&TCPbufs["tcp"]);
		}
	}

	if(pTCPostreams.find("tcp") != pTCPostreams.end())
	{
		auto tcp = std::make_shared<spdlog::sinks::ostream_sink_mt>(*pTCPostreams["tcp"], true);
		tcp->set_level(log_level);
		LogSinks["tcp"] = tcp;
	}

	ReloadLogSinks(LogSinks);
	return {log_level,console_level};
}

void DataConcentrator::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject())
		throw std::runtime_error("No valid JSON config object");

	odc::SetConfigVersion(JSONRoot.isMember("Version") ? JSONRoot["Version"].asString() : "No Version Available");

	std::pair<spdlog::level::level_enum,spdlog::level::level_enum> levels;
	try
	{
		levels = ConfigureLogSinks(JSONRoot);

		odc::spdlog_init_thread_pool(4096,3);
		AddLogger("opendatacon", LogSinks);
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		throw std::runtime_error("Main logger initialization failed: " + std::string(ex.what()));
	}

	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

	log->critical("This is opendatacon version '{}'", ODC_VERSION_STRING);
	log->critical("Log level set to {}", spdlog::level::level_string_views[levels.first]);
	log->critical("Console level set to {}", spdlog::level::level_string_views[levels.second]);
	log->info("Loading configuration... ");

	//Configure the user interface
	if(JSONRoot.isMember("Plugins"))
		ProcessPlugins(JSONRoot["Plugins"]);

	if(JSONRoot.isMember("Ports"))
		ProcessPorts(JSONRoot["Ports"]);

	if(JSONRoot.isMember("Connectors"))
	{
		//make a logger for use by Connectors
		AddLogger("Connectors", LogSinks);
		ProcessConnectors(JSONRoot["Connectors"]);
	}

	odc::spdlog_flush_every(std::chrono::seconds(60));
}

void DataConcentrator::ProcessPorts(const Json::Value& Ports)
{
	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

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
			DataPorts.emplace(Ports[n]["Name"].asString(), std::shared_ptr<DataPort>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
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
		auto portlib = LoadModule(libfilename);

		if(portlib == nullptr)
		{
			log->error("{}",LastSystemError());
			log->error("Failed to load library '{}' mapping {} to NullPort...", libfilename, Ports[n]["Name"].asString());
			DataPorts.emplace(Ports[n]["Name"].asString(), std::shared_ptr<DataPort>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
			set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
			continue;
		}

		//Create a logger if we haven't already
		if(!odc::spdlog_get(libname))
			AddLogger(libname, LogSinks);

		//Our API says the library should export a creation function: DataPort* new_<Type>Port(Name, Filename, Overrides)
		//it should return a pointer to a heap allocated instance of a descendant of DataPort
		std::string new_funcname = "new_"+Ports[n]["Type"].asString()+"Port";
		auto new_port_func = reinterpret_cast<DataPort*(*)(const std::string&, const std::string&, const Json::Value&)>(LoadSymbol(portlib, new_funcname));

		std::string delete_funcname = "delete_"+Ports[n]["Type"].asString()+"Port";
		auto delete_port_func = reinterpret_cast<void (*)(DataPort*)>(LoadSymbol(portlib, delete_funcname));

		if(new_port_func == nullptr)
			log->info("{} : Failed to load symbol '{}' from library '{}' - {}", Ports[n]["Name"].asString(), new_funcname, libfilename, LastSystemError());
		if(delete_port_func == nullptr)
			log->info("{} : Failed to load symbol '{}' from library '{}' - {}", Ports[n]["Name"].asString(), delete_funcname, libfilename, LastSystemError());

		if(new_port_func == nullptr || delete_port_func == nullptr)
		{
			log->error("{} : Failed to load port, mapping to NullPort...", Ports[n]["Name"].asString());
			DataPorts.emplace(Ports[n]["Name"].asString(), std::shared_ptr<DataPort>(new NullPort(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]),[](DataPort* pDP){delete pDP;}));
			set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
			continue;
		}

		auto port_cleanup = [=](DataPort* port)
					  {
						  delete_port_func(port);
						  UnLoadModule(portlib);
					  };

		//call the creation function and wrap the returned pointer to a new port
		DataPorts.emplace(Ports[n]["Name"].asString(), std::shared_ptr<DataPort>(new_port_func(Ports[n]["Name"].asString(), Ports[n]["ConfFilename"].asString(), Ports[n]["ConfOverrides"]), port_cleanup));
		set_init_mode(DataPorts.at(Ports[n]["Name"].asString()).get());
	}
}

void DataConcentrator::ProcessConnectors(const Json::Value& Connectors)
{
	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

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
		DataConnectors.emplace(Connectors[n]["Name"].asString(), std::shared_ptr<DataConnector>(new DataConnector(Connectors[n]["Name"].asString(), Connectors[n]["ConfFilename"].asString(), Connectors[n]["ConfOverrides"]),[](DataConnector* pDC){delete pDC;}));
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

void DataConcentrator::ProcessPlugins(const Json::Value& Plugins)
{
	auto log = odc::spdlog_get("opendatacon");
	if(!log)
		throw std::runtime_error("Failed to fetch main logger registration");

	log->info("Loading Plugins... ");

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
		auto pluginlib = LoadModule(libfilename);

		if(pluginlib == nullptr)
		{
			log->error("{}",LastSystemError());
			log->error("{} : Dynamic library '{}' load failed skipping plugin...", PluginName, libfilename);
			continue;
		}

		//Our API says the library should export a creation function: IUI* new_<Type>Plugin(Name, Filename, Overrides)
		//it should return a pointer to a heap allocated instance of a descendant of IUI
		std::string new_funcname = "new_"+Plugins[n]["Type"].asString()+"Plugin";
		auto new_plugin_func = reinterpret_cast<IUI*(*)(std::string, std::string, const Json::Value)>(LoadSymbol(pluginlib, new_funcname));

		std::string delete_funcname = "delete_"+Plugins[n]["Type"].asString()+"Plugin";
		auto delete_plugin_func = reinterpret_cast<void (*)(IUI*)>(LoadSymbol(pluginlib, delete_funcname));

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
			AddLogger(libname, LogSinks);

		auto plugin_cleanup = [=](IUI* plugin)
					    {
						    delete_plugin_func(plugin);
						    UnLoadModule(pluginlib);
					    };

		//call the creation function and wrap the returned pointer to a new plugin
		Interfaces.emplace(PluginName, std::shared_ptr<IUI>(new_plugin_func(PluginName, Plugins[n]["ConfFilename"].asString(), Plugins[n]["ConfOverrides"]), plugin_cleanup));
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

void DataConcentrator::EnableIOHandler(std::shared_ptr<IOHandler> ioh)
{
	starting_element_count++;
	if(ioh->InitState == InitState_t::ENABLED)
	{
		pIOS->post([this,ioh]()
			{
				ioh->Enable();
				starting_element_count--;
			});
	}
	else if(ioh->InitState == InitState_t::DELAYED)
	{
		std::shared_ptr<asio::steady_timer> pTimer = pIOS->make_steady_timer();
		pTimer->expires_from_now(std::chrono::milliseconds(ioh->EnableDelayms));
		pTimer->async_wait([this,pTimer,ioh](asio::error_code err_code)
			{
				//FIXME: check err_code?
				ioh->Enable();
				starting_element_count--;
			});
	}
	else
		starting_element_count--;
}

void DataConcentrator::EnableIUI(std::shared_ptr<IUI> iui)
{
	starting_element_count++;
	pIOS->post([this,iui]()
		{
			iui->Enable();
			starting_element_count--;
		});
}

void DataConcentrator::Run()
{
	if (auto log = odc::spdlog_get("opendatacon"))
		log->info("Starting worker threads...");

	for (int i = 0; i < pIOS->GetConcurrency(); ++i)
		threads.emplace_back([this]()
			{
				try
				{
				      pIOS->run();
				}
				catch (std::exception& e)
				{
				      if(auto log = odc::spdlog_get("opendatacon"))
						log->critical("Shutting down due to exception from thread pool: {}", e.what());
				      Shutdown();
				}
			});

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling DataConnectors...");
	for(auto& Name_n_Conn : DataConnectors)
		EnableIOHandler(Name_n_Conn.second);

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling DataPorts...");
	for(auto& Name_n_Port : DataPorts)
		EnableIOHandler(Name_n_Port.second);

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabling Interfaces...");
	for(auto& Name_n_UI : Interfaces)
		EnableIUI(Name_n_UI.second);

	try
	{
		while(starting_element_count > 0)
			pIOS->run_one();

		if(auto log = odc::spdlog_get("opendatacon"))
			log->info("Up and running.");

		pIOS->run();
	}
	catch (std::exception& e)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->critical("Shutting down due to exception from thread pool: {}", e.what());
		Shutdown();
		pIOS->run();
	}

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

void DataConcentrator::ParkThread()
{
	if(shutting_down || !parking)
		return;

	//wait for startup to finish
	while(starting_element_count > 0)
		pIOS->run_one();

	//park another threads if there's any left
	pIOS->post([this](){ParkThread();});

	if(auto log = odc::spdlog_get("opendatacon"))
		log->debug("ParkThread() parking thread {}",std::this_thread::get_id());

	num_parked_threads++;
	while(!shutting_down && parking)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	num_parked_threads--;

	if(auto log = odc::spdlog_get("opendatacon"))
		log->debug("ParkThread() releasing thread {}",std::this_thread::get_id());
}

bool DataConcentrator::ParkThreads()
{
	parking = true;
	//post a paking task.
	//As each thread is parked, it posts another paking task
	pIOS->post([this](){ParkThread();});

	//we have to park the pool threads plus the main run thread
	auto threads_to_park = threads.size()+1;
	while(num_parked_threads < threads_to_park && !shutting_down)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

	if(shutting_down)
		parking = false;

	if(auto log = odc::spdlog_get("opendatacon"))
		log->debug("ParkThreads() returning {}",parking);

	return parking;
}

Json::Value DataConcentrator::FindChangedConfs(const std::string& collection_name,
	std::unordered_map<std::string,std::shared_ptr<Json::Value>>& old_file_confs,
	const Json::Value& old_main_conf, const Json::Value& new_main_conf,
	std::set<std::string>& created, std::set<std::string>& changed, std::set<std::string>& deleted)
{
	Json::Value changed_confs;
	//check for new or changed objects in the new config
	for(Json::ArrayIndex n = 0; n < new_main_conf[collection_name].size(); ++n)
	{
		const auto& new_object = new_main_conf[collection_name][n];
		if(!new_object.isMember("Name"))
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->error("{} object without 'Name' : '{}'",collection_name,new_object.toStyledString());
			continue;
		}
		const auto& name = new_object["Name"].asString();

		//try to find that name in the old config
		const Json::Value* p_old_object = nullptr;
		for(Json::ArrayIndex m = 0; m < old_main_conf[collection_name].size(); ++m)
			if(old_main_conf[collection_name][m]["Name"] == name)
			{
				p_old_object = &old_main_conf[collection_name][m];
				break;
			}

		if(p_old_object)
		{
			auto isDifferent = [&]() -> bool
						 {
							 if((*p_old_object)["Type"] != new_object["Type"])
								 return true;
							 if((*p_old_object)["Library"] != new_object["Library"])
								 return true;
							 if((*p_old_object)["ConfOverrides"] != new_object["ConfOverrides"])
								 return true;

							 auto pOld = old_file_confs[(*p_old_object)["ConfFilename"].asString()];
							 if(!pOld)
								 pOld = std::make_shared<Json::Value>();

							 auto pNew = RecallOrCreate(new_object["ConfFilename"].asString());

							 if(*pOld != *pNew)
								 return true;

							 if(pNew->isMember("Inherits") && (*pNew)["Inherits"].isArray())
								 for(Json::ArrayIndex n = 0; n < (*pNew)["Inherits"].size(); ++n)
								 {
									 auto filename = (*pNew)["Inherits"][n].asString();
									 auto pOldInherit = old_file_confs[filename];
									 auto pNewInherit = RecallOrCreate(filename);
									 if(pOldInherit && pNewInherit && *pOldInherit != *pNewInherit)
										 return true;
								 }

							 return false;
						 };

			if(isDifferent())
			{
				changed.insert(name);
				changed_confs.append(new_object);
			}
		}
		else //totally new
		{
			created.insert(name);
			changed_confs.append(new_object);
		}
	}
	//check for old objects that aren't in the new config
	for(Json::ArrayIndex m = 0; m < old_main_conf[collection_name].size(); ++m)
	{
		bool found = false;
		for(Json::ArrayIndex n = 0; n < new_main_conf[collection_name].size(); ++n)
			if(old_main_conf[collection_name][m]["Name"] == new_main_conf[collection_name][n]["Name"])
			{
				found = true;
				break;
			}
		if(!found)
			deleted.insert(old_main_conf[collection_name][m]["Name"].asString());
	}
	return changed_confs;
}

bool DataConcentrator::ReloadConfig(const std::string &filename, const size_t disable_delay)
{
	static std::mutex mtx;
	std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
	if(!lock.owns_lock())
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("Reload failed: Another reload already in progress.");
		return false;
	}

	auto old_main_conf = RecallOrCreate(ConfFilename);

	//copy all the old config file contents
	auto old_file_confs = ConfigParser::JSONFileCache;

	//clear the config parser cache so new files are loaded when we GetConfiguration()
	ConfigParser::JSONFileCache.clear();

	auto old_ConfFilename = ConfFilename;
	if(filename != "")
		ConfFilename = filename;

	auto new_main_conf = RecallOrCreate(ConfFilename);

	Json::Value changed_confs;
	std::set<std::string> createdIOHs;
	std::set<std::string> changedIOHs;
	std::set<std::string> deletedIOHs;
	std::set<std::string> createdIUIs;
	std::set<std::string> changedIUIs;
	std::set<std::string> deletedIUIs;
	try
	{
		if(new_main_conf->isNull() ||
		   (!(*new_main_conf)["Ports"].size()
		    && !(*new_main_conf)["Connectors"].size()
		    && !(*new_main_conf)["Plugins"].size()))
			throw std::runtime_error("No objects found");

		changed_confs["Ports"] = FindChangedConfs("Ports",old_file_confs,*old_main_conf,*new_main_conf,createdIOHs,changedIOHs,deletedIOHs);
		changed_confs["Connectors"] = FindChangedConfs("Connectors",old_file_confs,*old_main_conf,*new_main_conf,createdIOHs,changedIOHs,deletedIOHs);
		changed_confs["Plugins"] = FindChangedConfs("Plugins",old_file_confs,*old_main_conf,*new_main_conf,createdIUIs,changedIUIs,deletedIUIs);
	}
	catch(std::exception& e)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("DataConcentrator::ReloadConfig() Failed : '{}'",e.what());
		ConfigParser::JSONFileCache = old_file_confs;
		ConfFilename = old_ConfFilename;
		return false;
	}

	if(auto log = odc::spdlog_get("opendatacon"))
	{
		for(auto create : createdIOHs)
			log->info("New IOHandler: '{}'",create);
		for(auto change : changedIOHs)
			log->info("Changed IOHandler: '{}'",change);
		for(auto del : deletedIOHs)
			log->info("Deleted IOHandler: '{}'",del);
		for(auto create : createdIUIs)
			log->info("New UI: '{}'",create);
		for(auto change : changedIUIs)
			log->info("Changed UI: '{}'",change);
		for(auto del : deletedIUIs)
			log->info("Deleted UI: '{}'",del);
	}

	//do log sinks first
	std::pair<spdlog::level::level_enum,spdlog::level::level_enum> levels;
	try
	{
		Json::Value copy_old = *old_main_conf;
		Json::Value copy_new = *new_main_conf;
		for(Json::Value* conf_copy : {&copy_new,&copy_old})
		{
			(*conf_copy)["Ports"] = Json::Value::nullSingleton();
			(*conf_copy)["Connectors"] = Json::Value::nullSingleton();
			(*conf_copy)["Plugins"] = Json::Value::nullSingleton();
		}
		//all that's left in the copied confs should be logging config
		if(copy_old != copy_new)
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->info("Logging config changed - reloading log sinks");
			levels = ConfigureLogSinks(*new_main_conf);
			if(auto log = odc::spdlog_get("opendatacon"))
			{
				log->critical("Log level set to {}", spdlog::level::level_string_views[levels.first]);
				log->critical("Console level set to {}", spdlog::level::level_string_views[levels.second]);
			}
		}
		else if(auto log = odc::spdlog_get("opendatacon"))
			log->info("Logging config didn't change - not reloading log sinks");
	}
	catch(std::exception& e)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("DataConcentrator::ReloadConfig() Failed : '{}'",e.what());
		ConfigureLogSinks(*old_main_conf);
		ConfigParser::JSONFileCache = old_file_confs;
		ConfFilename = old_ConfFilename;
		return false;
	}

	//if nothig else has changed, we're done
	if(createdIOHs.size()+changedIOHs.size()+deletedIOHs.size()
	   +createdIUIs.size()+changedIUIs.size()+deletedIUIs.size() == 0)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->info("No changed objects - reload finished.");
		return true;
	}

	//superset for all IOHs that will be deleted
	std::set<std::string> delete_or_changeIOHs(deletedIOHs);
	for(auto name : changedIOHs)
		delete_or_changeIOHs.insert(name);

	//check to make sure IOHs really got constructed, not just in the config
	std::set<std::string> not_found;
	for(auto& name : delete_or_changeIOHs)
	{
		if(IOHandler::GetIOHandlers().find(name) == IOHandler::GetIOHandlers().end())
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->warn("IOHandler '{}' from old config not found.",name);
			not_found.insert(name);
		}
	}
	for(auto& name : not_found)
	{
		delete_or_changeIOHs.erase(name);
		deletedIOHs.erase(name);
		if(changedIOHs.erase(name))
			createdIOHs.insert(name);
	}

	//superset for all IUIs that will be deleted
	std::set<std::string> delete_or_changeIUIs(deletedIUIs);
	for(auto name : changedIUIs)
		delete_or_changeIUIs.insert(name);

	//check to make sure IUIs really got constructed, not just in the config
	for(auto name : delete_or_changeIUIs)
		if(Interfaces.find(name) == Interfaces.end())
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->warn("UI '{}' from old config not found.",name);
			delete_or_changeIUIs.erase(name);
			deletedIUIs.erase(name);
			if(changedIUIs.erase(name))
				createdIUIs.insert(name);
		}

	//check for connections that would be left dangling by deleted ports before we do anything
	for(auto name : deletedIOHs)
		for(auto& sub_pair : IOHandler::GetIOHandlers().at(name)->GetSubscribers())
			if(dynamic_cast<DataConnector*>(sub_pair.second) && delete_or_changeIOHs.find(sub_pair.first) == delete_or_changeIOHs.end())
			{
				if(auto log = odc::spdlog_get("opendatacon"))
					log->error("Connector '{}' would be left with dangling reference to deleted object '{}'.",sub_pair.first,name);
				ConfigParser::JSONFileCache = old_file_confs;
				ConfFilename = old_ConfFilename;
				return false;
			}

	std::set<std::string> reenable;
	for(auto name : delete_or_changeIOHs)
	{
		//disable IOHandlers marked for delete/change
		IOHandler::GetIOHandlers().at(name)->Disable();

		//if it's a connector, also disable connected ports
		if(auto pConn = dynamic_cast<DataConnector*>(IOHandler::GetIOHandlers().at(name)))
			for(auto conn_pair : pConn->GetConnections())
				if(conn_pair.second.second->Enabled() && delete_or_changeIOHs.find(conn_pair.second.second->GetName()) == delete_or_changeIOHs.end())
				{
					if(auto log = odc::spdlog_get("opendatacon"))
						log->debug("Temporary disablement of IOHandler '{}', connected to changing connection '{}'.",conn_pair.second.second->GetName(),pConn->GetName());
					conn_pair.second.second->Disable();
					reenable.insert(conn_pair.second.second->GetName());
				}
	}

	//Disable all interfaces because deleteing ports can delete IUIResponders - refresh those below
	for(auto interface : Interfaces)
		interface.second->Disable();

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Disabled {} objects affected by reload.",Interfaces.size()+delete_or_changeIOHs.size()+reenable.size());

	//wait a while to make sure disable events flow through
	for(auto t = disable_delay; t>0; t--)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->info("{} seconds until reload...",t);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	//post a task and wait for it, just in case it's all backed up
	std::promise<char> result_prom;
	auto result = result_prom.get_future();
	pIOS->post([&result_prom]()
		{
			result_prom.set_value(0);
		});
	result.get(); //wait

	///////////// PARK THREADS ///////////////
	if(!ParkThreads()) //This should only return false if we're shutting down, so no cleanup required
		return false;

	//Now we can modify the unprotected collections
	//We've induced a state similar to start-up (when there are no other threads running)
	if(auto log = odc::spdlog_get("opendatacon"))
		log->critical("Applying reloaded config.");

	//Unsubscribe conns to be deleted (from thier ports)
	for(auto name : delete_or_changeIOHs)
	{
		if(auto pConn = dynamic_cast<DataConnector*>(IOHandler::GetIOHandlers().at(name)))
			for(auto conn_pair : pConn->GetConnections())
				conn_pair.second.second->UnSubscribe(pConn->GetName());
	}

	//delete old objects
	std::map<std::string,std::unordered_map<std::string,IOHandler*>> old_subs;
	std::map<std::string,std::map<std::string,bool>> old_demands;
	std::multimap<std::string,DataConnector*> needs_new_addr;
	for(auto name : delete_or_changeIOHs)
	{
		if(changedIOHs.find(name) != changedIOHs.end())
		{
			//store the old addr
			auto pIOH = IOHandler::GetIOHandlers().at(name);
			old_demands[name] = pIOH->GetDemands();
			old_subs[name] = pIOH->GetSubscribers();

			for(auto& sub_pair : old_subs[name])
				if(auto pConn = dynamic_cast<DataConnector*>(sub_pair.second))
					if(delete_or_changeIOHs.find(sub_pair.first) == delete_or_changeIOHs.end())
						needs_new_addr.insert({name,pConn});
		}

		//FIXME: backup OnDemand state??? anything else stateful???
		IOHandler::GetIOHandlers().erase(name);
		DataPorts.erase(name);
		DataConnectors.erase(name);
	}
	for(auto name : delete_or_changeIUIs)
		Interfaces.erase(name);

	//create replacements and new
	ProcessPorts(changed_confs["Ports"]);
	for(auto name : changedIOHs)
	{
		//go through and replace subscriptions and inDemand states
		auto ioh_it = IOHandler::GetIOHandlers().find(name);
		if(ioh_it != IOHandler::GetIOHandlers().end())
			//Can't downcast to DataPort because RTTI info isn't exported from our dynamically loaded modules
			//This is intentional - to have a simple C API
			//Not to worry - if it's not a connector, it's a port
			if(!dynamic_cast<DataConnector*>(ioh_it->second))
			{
				for(auto sub_pair : old_subs[name])
					ioh_it->second->Subscribe(sub_pair.second,sub_pair.first);
				for(auto name_demand : old_demands[name])
					if(name_demand.second)
						ioh_it->second->Event(ConnectState::CONNECTED, name_demand.first);
			}
	}

	ProcessConnectors(changed_confs["Connectors"]);
	for(auto name : changedIOHs)
	{
		//go through and replace addresses
		auto ioh_it = IOHandler::GetIOHandlers().find(name);
		if(ioh_it != IOHandler::GetIOHandlers().end())
		{
			auto bounds = needs_new_addr.equal_range(name);
			for(auto aMatch_it = bounds.first; aMatch_it != bounds.second; aMatch_it++)
				aMatch_it->second->ReplaceAddress(name,ioh_it->second);
		}
		else if(auto count = needs_new_addr.count(name))
		{
			if(auto log = odc::spdlog_get("opendatacon"))
				log->critical("{} connectors with dangling pointer because port '{}' construction failed.",count,name);
			throw std::runtime_error("Reload config failed terminally - connectors with dangling pointer");
		}
	}

	ProcessPlugins(changed_confs["Plugins"]);

	//superset for all changed and new
	std::set<std::string> created_or_changeIOHs(createdIOHs);
	for(auto name : changedIOHs)
		created_or_changeIOHs.insert(name);
	std::set<std::string> created_or_changeIUIs(createdIUIs);
	for(auto name : changedIUIs)
		created_or_changeIUIs.insert(name);

	for(auto& port_pair : DataPorts)
		if(created_or_changeIOHs.find(port_pair.first) != created_or_changeIOHs.end())
			port_pair.second->Build();

	for(auto& conn_pair : DataConnectors)
		if(created_or_changeIOHs.find(conn_pair.first) != created_or_changeIOHs.end())
			conn_pair.second->Build();

	RefreshIUIResponders();
	for(auto& ui_pair : Interfaces)
	{
		if(created_or_changeIUIs.find(ui_pair.first) != created_or_changeIUIs.end())
		{
			PrepInterface(ui_pair.second);
			ui_pair.second->Build();
		}
		else
		{
			ui_pair.second->SetResponders(RespondersMasterCopy);
		}
	}

	if(auto log = odc::spdlog_get("opendatacon"))
		log->critical("Reloaded config applied.");

	////////////// UNPARK THREADS ////////////
	parking = false;

	for(auto& conn_pair : DataConnectors)
		if(created_or_changeIOHs.find(conn_pair.first) != created_or_changeIOHs.end())
			EnableIOHandler(conn_pair.second);

	for(auto& port_pair : DataPorts)
		if(created_or_changeIOHs.find(port_pair.first) != created_or_changeIOHs.end())
			EnableIOHandler(port_pair.second);

	for(auto enable : reenable)
		IOHandler::GetIOHandlers().at(enable)->Enable();

	//enable all interfaces because we disabled them all above
	for(auto& ui_pair : Interfaces)
		EnableIUI(ui_pair.second);

	if(auto log = odc::spdlog_get("opendatacon"))
		log->info("Enabled {} objects affected by reload.",created_or_changeIUIs.size()+created_or_changeIOHs.size()+reenable.size());

	return true;
}

void DataConcentrator::Shutdown()
{
	//Shutdown gets called from various places (signal handling, user interface(s))
	//ensure we only act once, and catch all exceptions
	//call_once propagates exceptions, but allows calling again in that case!
	// so we catch within handler
	std::call_once(shutdown_flag, [this]()
		{
			shutting_down = true;
			try
			{
			//wait for startup to finish before shutdown
			      while(starting_element_count > 0)
					pIOS->run_one();

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
			      for (auto it = TCPbufs.begin(); it != TCPbufs.end(); ++it)
			      {
			            it->second.DeInit();
			            if(LogSinks.find(it->first) != LogSinks.end())
						LogSinks[it->first]->set_level(spdlog::level::off);
				}

			      ios_working.reset();
			}
			catch(const std::exception& e)
			{
			      if(auto log = odc::spdlog_get("opendatacon"))
					log->critical("Caught exception in DataConcentrator::Shutdown(): {}", e.what());
			//Fall through - we set the shutting_down flag for watchdog
			}
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
