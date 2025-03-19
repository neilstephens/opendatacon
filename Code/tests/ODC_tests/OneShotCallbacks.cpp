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
#include "../../opendatacon/DataConnector.h"
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

inline bool LogContains(const std::string& search_string)
{
	odc::spdlog_flush_all();
	std::ifstream log_file("oneshottest.log");
	std::string line;
	while (std::getline(log_file, line))
	{
		if (line.find(search_string) != std::string::npos)
			return true;
	}
	return false;
}

auto test_callback = std::make_shared<std::function<void(CommandStatus)>>([](CommandStatus)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->debug("test callback");
	});

#define SUITE(name) "OneShotCallbacks - " name

TEST_CASE(SUITE("Call Once"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotWrap(test_callback);

	(*wrapped_callback)(CommandStatus::SUCCESS);

	bool log_line_found = LogContains("error");
	REQUIRE_FALSE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Call Twice"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotWrap(test_callback);

	(*wrapped_callback)(CommandStatus::SUCCESS);
	(*wrapped_callback)(CommandStatus::SUCCESS);

	bool log_line_found = LogContains("One-shot function called more than once.");
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Call Never"))
{
	SetupLogging();

	{
		auto wrapped_callback = odc::OneShotWrap(test_callback);
	}

	bool log_line_found = LogContains("One-shot function not called before destruction.");
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("Wrap Twice"))
{
	SetupLogging();

	auto wrapped_callback = odc::OneShotWrap(test_callback);

	(*wrapped_callback)(CommandStatus::SUCCESS);
	auto rewrapped = odc::OneShotWrap(wrapped_callback);

	REQUIRE(wrapped_callback == rewrapped);

	(*rewrapped)(CommandStatus::SUCCESS);

	bool log_line_found = LogContains("One-shot function called more than once.");
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

#include "TestPorts.h"
TEST_CASE(SUITE("Test IOHandler::PublishEvent"))
{
	SetupLogging();

	auto ios = odc::asio_service::Get();

	auto publisher = std::make_shared<PublicPublishPort>("publisher", "", Json::Value());
	auto subscriber = std::make_shared<CustomEventHandlerPort>("subscriber", "", Json::Value(), [](std::shared_ptr<const EventInfo>, const std::string&, SharedStatusCallback_t cb)
		{
			//call cb twice - which is an error
			(*cb)(CommandStatus::UNDEFINED);
			(*cb)(CommandStatus::UNDEFINED);
		});
	publisher->Subscribe(subscriber.get(),"subscriber");
	auto event = std::make_shared<EventInfo>(EventType::ConnectState,0,"publisher");
	event->SetPayload<EventType::ConnectState>(ConnectState::CONNECTED);
	publisher->PublicPublishEvent(event,test_callback);

	ios->run();

	bool log_line_found = LogContains("One-shot function called more than once.");
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

#include "TestTransforms.h"
class DataConcentrator //because DataConcentrator is a freind class of DataConnector
{
public:
	DataConcentrator(DataConnector& Con, bool useBadTx)
	{
		auto tx_cleanup = [=](Transform* tx)
					{
						delete tx;
					};
		if(useBadTx)
			Con.SenderTransforms["publisher"].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new BadTransform("BadTx",Json::Value::nullSingleton()),tx_cleanup));
		else
			Con.SenderTransforms["publisher"].push_back(std::unique_ptr<Transform, decltype(tx_cleanup)>(new NopTransform("GoodTx",Json::Value::nullSingleton()),tx_cleanup));
	}
};

TEST_CASE(SUITE("DataConnector n Transform - Good Behaviour"))
{
	SetupLogging();

	auto ios = odc::asio_service::Get();

	auto good_handler = [](std::shared_ptr<const EventInfo>, const std::string&, SharedStatusCallback_t cb)
				  {
					  (*cb)(CommandStatus::SUCCESS);
				  };

	auto publisher = std::make_shared<PublicPublishPort>("publisher", "", Json::Value());
	auto subscriber1 = std::make_shared<CustomEventHandlerPort>("subscriber1", "", Json::Value(), good_handler);
	auto subscriber2 = std::make_shared<CustomEventHandlerPort>("subscriber2", "", Json::Value(), good_handler);


	Json::Value Conn1Conf;
	Conn1Conf["Connections"][0]["Name"] = "connector1";
	Conn1Conf["Connections"][0]["Port1"] = "subscriber1";
	Conn1Conf["Connections"][0]["Port2"] = "publisher";
	Conn1Conf["Connections"][1]["Name"] = "connector2";
	Conn1Conf["Connections"][1]["Port1"] = "subscriber2";
	Conn1Conf["Connections"][1]["Port2"] = "publisher";
	DataConnector Conn1("Conn1","",Conn1Conf);

	const bool bad_tx = false;
	DataConcentrator(Conn1, bad_tx);

	Conn1.Enable();

	auto event = std::make_shared<EventInfo>(EventType::ConnectState,0,"publisher");
	event->SetPayload<EventType::ConnectState>(ConnectState::CONNECTED);
	publisher->PublicPublishEvent(event,test_callback);

	ios->run();

	bool log_line_found = LogContains("error");
	REQUIRE_FALSE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("DataConnector n Transform - Bad Subscribers"))
{
	SetupLogging();

	auto ios = odc::asio_service::Get();

	auto bad_handler = [](std::shared_ptr<const EventInfo>, const std::string&, SharedStatusCallback_t cb)
				 {
					 //bad behaviour to call the callback twice.
					 (*cb)(CommandStatus::SUCCESS);
					 (*cb)(CommandStatus::SUCCESS);
				 };

	auto publisher = std::make_shared<PublicPublishPort>("publisher", "", Json::Value());
	auto subscriber1 = std::make_shared<CustomEventHandlerPort>("subscriber1", "", Json::Value(), bad_handler);
	auto subscriber2 = std::make_shared<CustomEventHandlerPort>("subscriber2", "", Json::Value(), bad_handler);


	Json::Value Conn1Conf;
	Conn1Conf["Connections"][0]["Name"] = "connector1";
	Conn1Conf["Connections"][0]["Port1"] = "subscriber1";
	Conn1Conf["Connections"][0]["Port2"] = "publisher";
	Conn1Conf["Connections"][1]["Name"] = "connector2";
	Conn1Conf["Connections"][1]["Port1"] = "subscriber2";
	Conn1Conf["Connections"][1]["Port2"] = "publisher";
	DataConnector Conn1("Conn1","",Conn1Conf);

	const bool bad_tx = false;
	DataConcentrator(Conn1, bad_tx);

	Conn1.Enable();

	auto event = std::make_shared<EventInfo>(EventType::ConnectState,0,"publisher");
	event->SetPayload<EventType::ConnectState>(ConnectState::CONNECTED);
	publisher->PublicPublishEvent(event,test_callback);

	ios->run();

	bool log_line_found = LogContains("One-shot function called more than once.");
	REQUIRE(log_line_found);

	odc::spdlog_drop_all();
}

TEST_CASE(SUITE("DataConnector n Transform - Bad Transform"))
{
	SetupLogging();

	auto ios = odc::asio_service::Get();

	auto good_handler = [](std::shared_ptr<const EventInfo>, const std::string&, SharedStatusCallback_t cb)
				  {
					  (*cb)(CommandStatus::SUCCESS);
				  };

	auto publisher = std::make_shared<PublicPublishPort>("publisher", "", Json::Value());
	auto subscriber1 = std::make_shared<CustomEventHandlerPort>("subscriber1", "", Json::Value(), good_handler);
	auto subscriber2 = std::make_shared<CustomEventHandlerPort>("subscriber2", "", Json::Value(), good_handler);


	Json::Value Conn1Conf;
	Conn1Conf["Connections"][0]["Name"] = "connector1";
	Conn1Conf["Connections"][0]["Port1"] = "subscriber1";
	Conn1Conf["Connections"][0]["Port2"] = "publisher";
	Conn1Conf["Connections"][1]["Name"] = "connector2";
	Conn1Conf["Connections"][1]["Port1"] = "subscriber2";
	Conn1Conf["Connections"][1]["Port2"] = "publisher";
	DataConnector Conn1("Conn1","",Conn1Conf);

	const bool bad_tx = true;
	DataConcentrator(Conn1, bad_tx);

	Conn1.Enable();

	auto event = std::make_shared<EventInfo>(EventType::ConnectState,0,"publisher");
	event->SetPayload<EventType::ConnectState>(ConnectState::CONNECTED);
	publisher->PublicPublishEvent(event,test_callback);

	ios->run();

	bool log_line_found = LogContains("One-shot function called more than once.");
	REQUIRE(log_line_found);


	odc::spdlog_drop_all();
}
