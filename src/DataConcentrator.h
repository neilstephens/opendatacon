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

#include <asio.hpp>
#include <unordered_map>
#include <asiodnp3/DNP3Manager.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/ConfigParser.h>

#include "DataConnector.h"
#include "AdvancedLogger.h"
#include "LogToFile.h"

#ifdef WIN32 
const std::string DYNLIBPRE = "";
const std::string DYNLIBEXT = ".dll";
#define DYNLIBLOAD(a) LoadLibraryExA(a, 0, DWORD(0))
#define DYNLIBGETSYM(a,b) GetProcAddress(a, b)
#else
const std::string DYNLIBPRE = "lib";
#ifdef __APPLE__
const std::string DYNLIBEXT = ".dylib";
#else
const std::string DYNLIBEXT = ".so";
#endif
#define DYNLIBLOAD(a) dlopen(a, RTLD_LAZY)
#define DYNLIBGETSYM(a,b) dlsym(a, b)
#endif

inline std::string GetLibFileName(const std::string LibName)
{
	return DYNLIBPRE + LibName + DYNLIBEXT;
}

class DataConcentrator: public ConfigParser
{
public:
	DataConcentrator(std::string FileName);
	~DataConcentrator();

	std::unordered_map<std::string, std::shared_ptr<DataPort>> DataPorts;
	std::unordered_map<std::string, std::shared_ptr<DataConnector>> DataConnectors;

	asiodnp3::DNP3Manager DNP3Mgr;
	openpal::LogFilters LOG_LEVEL;
	AdvancedLogger AdvConsoleLog;//just prints messages to the console plus filtering (Adv)
	LogToFile FileLog;//Prints all messages to a rolling set of log files.
	AdvancedLogger AdvFileLog;
	asiopal::LogFanoutHandler FanoutHandler;
	asio::io_service IOS;
	std::unique_ptr<asio::io_service::work> ios_working;

	void ProcessElements(const Json::Value& JSONRoot) override;
	void BuildOrRebuild();
	void Run();
	void Shutdown();
private:
	void EnablePortOrConn(std::stringstream& args, bool enable);
	void RestartPortOrConn(std::stringstream& args);
	void ListConns(std::stringstream& args);
	void ListPorts(std::stringstream& args);
};

#endif /* DATACONCENTRATOR_H_ */
