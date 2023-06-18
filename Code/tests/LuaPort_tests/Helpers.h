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
 * Helpers.h
 *
 *  Created on: 17/06/2023
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef TESTHELPERS_H
#define TESTHELPERS_H

#include <json/json.h>
#include <opendatacon/IOHandler.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <opendatacon/Platform.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <whereami++.h>
#include <fstream>
#include <filesystem>
#include <random>

extern spdlog::level::level_enum log_level;

inline void TestSetup()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("LuaPort", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pODCLogger->set_level(log_level);
	odc::spdlog_register_logger(pODCLogger);

	static std::once_flag once_flag;
	std::call_once(once_flag,[]()
		{
			InitLibaryLoading();
		});
}
inline void TestTearDown()
{
	odc::spdlog_drop_all(); // Close off everything
}

inline Json::Value GetConfigJSON()
{
	std::string ExePath = whereami::getExecutablePath().dirname();
	std::string conf = "{\"LuaFile\" : \""+ExePath+"/test.lua\"}";

	std::istringstream iss(conf);
	Json::CharReaderBuilder JSONReader;
	std::string err_str;
	Json::Value json_conf;
	bool parse_success = Json::parseFromStream(JSONReader,iss, &json_conf, &err_str);
	if (!parse_success)
	{
		if(auto log = odc::spdlog_get("LuaPort"))
			log->error("Failed to parse configuration: '{}'", err_str);
	}
	return json_conf;
}

#endif // TESTHELPERS_H

