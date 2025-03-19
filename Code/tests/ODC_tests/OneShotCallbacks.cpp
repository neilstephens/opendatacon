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
 * UtilTests.cpp
 *
 *  Created on: 18/03/2025
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/OneShotFunc.h>
#include <catch.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <functional>

extern spdlog::level::level_enum log_level;

inline void SetupLogging()
{
	const bool truncate = true;
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("oneshottest.log",truncate);
	file_sink->set_level(spdlog::level::level_enum::err);
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(log_level);
	auto logger = std::make_shared<spdlog::logger>("opendatacon", spdlog::sinks_init_list{console_sink,file_sink});
	logger->set_level(spdlog::level::level_enum::trace);
	odc::spdlog_register_logger(logger);
}

auto test_callback = std::make_shared<std::function<void(void)>>([]()
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->debug("test callback");
	});

#define SUITE(name) "OneShotCallbacks - " name

TEST_CASE(SUITE("Call Once"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotFunc<void(void)>::Wrap(test_callback);

	(*wrapped_callback)();
	odc::spdlog_flush_all();

	std::ifstream log_file("oneshottest.log");
	std::string line;
	bool log_line_found = false;
	while (std::getline(log_file, line))
	{
		if (line.find("error") != std::string::npos)
		{
			log_line_found = true;
			break;
		}
	}
	REQUIRE_FALSE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Call Twice"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotFunc<void(void)>::Wrap(test_callback);

	(*wrapped_callback)();
	(*wrapped_callback)();
	odc::spdlog_flush_all();

	std::ifstream log_file("oneshottest.log");
	std::string line;
	bool log_line_found = false;
	while (std::getline(log_file, line))
	{
		if (line.find("One-shot function called more than once.") != std::string::npos)
		{
			log_line_found = true;
			break;
		}
	}
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Wrap Twice"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotFunc<void(void)>::Wrap(test_callback);

	(*wrapped_callback)();
	auto rewrapped = odc::OneShotFunc<void(void)>::Wrap(wrapped_callback);

	REQUIRE(wrapped_callback == rewrapped);

	(*rewrapped)();
	odc::spdlog_flush_all();

	std::ifstream log_file("oneshottest.log");
	std::string line;
	bool log_line_found = false;
	while (std::getline(log_file, line))
	{
		if (line.find("One-shot function called more than once.") != std::string::npos)
		{
			log_line_found = true;
			break;
		}
	}
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Test IOHandler::PublishEvent"))
{
	SetupLogging();

	//TODO: Test IOHandler::PublishEvent

	odc::spdlog_flush_all();

	//TODO: check the log for errors

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Test DataConnector::Event"))
{
	SetupLogging();

	//TODO: Test DataConnector::Event (including Transform::Event)

	odc::spdlog_flush_all();

	//TODO: check the log for errors

	odc::spdlog_drop_all();
}
