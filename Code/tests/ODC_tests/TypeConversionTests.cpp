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
 * TypeConversionTests.cpp
 *
 *	Tests for numeric event type conversion and the TypeConversionTransform
 */

#include "../../../opendatacon/TypeConversionTransform.h"
#include <opendatacon/Platform.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <catch.hpp>
#include <limits>
#include <opendatacon/EventConversion.h>

using namespace odc;

extern spdlog::level::level_enum log_level;

//TestODCHelpers.h defines its own Log, which clashes with the one in Log.h that the transform uses
static void TestSetup()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("Connectors", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);
}
static void TestTearDown()
{
	odc::spdlog_flush_all();
	odc::spdlog_drop_all();
}

#define SUITE(name) "TypeConversionTestSuite - " name

template<EventType ET, typename T>
std::shared_ptr<const EventInfo> MakeEvent(T val, uint16_t index = 1)
{
	auto event = std::make_shared<EventInfo>(ET, index, "Test", QualityFlags::ONLINE, 1234);
	event->SetPayload<ET>(std::move(val));
	return event;
}

TEST_CASE(SUITE("ConvertEvent"))
{
	TestSetup();

	SECTION("Counter to Analog")
	{
		auto e = ConvertEvent(MakeEvent<EventType::Counter>(uint32_t(4000000000u)), EventType::Analog);
		CHECK(e->GetEventType() == EventType::Analog);
		CHECK(e->GetPayload<EventType::Analog>() == 4000000000.0);
		CHECK(e->GetIndex() == 1);
		CHECK(e->GetTimestamp() == 1234);
		CHECK(e->GetQuality() == QualityFlags::ONLINE);
	}
	SECTION("Analog to Counter")
	{
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(42.0), EventType::Counter)->GetPayload<EventType::Counter>() == 42);
		//rounding
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(42.4), EventType::Counter)->GetPayload<EventType::Counter>() == 42);
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(42.6), EventType::Counter)->GetPayload<EventType::Counter>() == 43);
		//clamping
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(-5.0), EventType::Counter)->GetPayload<EventType::Counter>() == 0);
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(1e20), EventType::Counter)->GetPayload<EventType::Counter>() == std::numeric_limits<uint32_t>::max());
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(std::numeric_limits<double>::infinity()), EventType::Counter)->GetPayload<EventType::Counter>() == std::numeric_limits<uint32_t>::max());
	}
	SECTION("FrozenCounter to/from Analog")
	{
		CHECK(ConvertEvent(MakeEvent<EventType::FrozenCounter>(uint32_t(77)), EventType::Analog)->GetPayload<EventType::Analog>() == 77.0);
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(77.5), EventType::FrozenCounter)->GetPayload<EventType::FrozenCounter>() == 78);
		CHECK(ConvertEvent(MakeEvent<EventType::Analog>(-1e10), EventType::FrozenCounter)->GetPayload<EventType::FrozenCounter>() == 0);
	}
	SECTION("Counter and FrozenCounter")
	{
		auto fc = ConvertEvent(MakeEvent<EventType::Counter>(uint32_t(99)), EventType::FrozenCounter);
		CHECK(fc->GetEventType() == EventType::FrozenCounter);
		CHECK(fc->GetPayload<EventType::FrozenCounter>() == 99);
		auto c = ConvertEvent(MakeEvent<EventType::FrozenCounter>(uint32_t(std::numeric_limits<uint32_t>::max())), EventType::Counter);
		CHECK(c->GetEventType() == EventType::Counter);
		CHECK(c->GetPayload<EventType::Counter>() == std::numeric_limits<uint32_t>::max());
	}
	SECTION("Identity and unsupported")
	{
		CHECK(ConvertEvent(MakeEvent<EventType::Counter>(uint32_t(5)), EventType::Counter)->GetPayload<EventType::Counter>() == 5);
		CHECK_THROWS(ConvertEvent(MakeEvent<EventType::Counter>(uint32_t(5)), EventType::Binary));
	}

	TestTearDown();
}

//run a transform on an event, returning whether the handler was called and what it was given
static std::pair<bool,std::shared_ptr<EventInfo>> RunTx(TypeConversionTransform& tx, std::shared_ptr<EventInfo> event)
{
	bool called = false;
	std::shared_ptr<EventInfo> result;
	auto handler = std::make_shared<std::function<void (std::shared_ptr<EventInfo>)>>([&](std::shared_ptr<EventInfo> e){ called = true; result = e; });
	tx.Event(event, handler);
	return {called,result};
}

TEST_CASE(SUITE("Transform"))
{
	TestSetup();

	Json::Value params;
	params["FromType"] = "Counter";
	params["ToType"] = "Analog";

	auto MakeCounter = [](uint16_t idx, uint32_t val)
				 {
					 auto e = std::make_shared<EventInfo>(EventType::Counter, idx, "Test");
					 e->SetPayload<EventType::Counter>(uint32_t(val));
					 return e;
				 };

	SECTION("Converts matching type")
	{
		TypeConversionTransform tx("Tx", params);
		auto [called,result] = RunTx(tx, MakeCounter(3, 10));
		REQUIRE(called);
		REQUIRE(result);
		CHECK(result->GetEventType() == EventType::Analog);
		CHECK(result->GetPayload<EventType::Analog>() == 10.0);
		CHECK(result->GetIndex() == 3);
	}
	SECTION("Passes through non-matching type")
	{
		TypeConversionTransform tx("Tx", params);
		auto e = std::make_shared<EventInfo>(EventType::Analog, 3, "Test");
		e->SetPayload<EventType::Analog>(1.5);
		auto [called,result] = RunTx(tx, e);
		REQUIRE(called);
		CHECK(result == e);
	}
	SECTION("Index allow-list")
	{
		params["Indexes"].append(1);
		params["Indexes"].append(2);
		TypeConversionTransform tx("Tx", params);

		auto [called1,listed] = RunTx(tx, MakeCounter(2, 10));
		REQUIRE(listed);
		CHECK(listed->GetEventType() == EventType::Analog);

		auto unlisted_in = MakeCounter(5, 10);
		auto [called2,unlisted] = RunTx(tx, unlisted_in);
		REQUIRE(called2);
		CHECK(unlisted == unlisted_in);
		CHECK(unlisted->GetEventType() == EventType::Counter);
	}
	SECTION("Unsupported conversion drops event without throwing")
	{
		params["ToType"] = "Binary";
		TypeConversionTransform tx("Tx", params);
		bool called = false;
		std::shared_ptr<EventInfo> result;
		REQUIRE_NOTHROW(std::tie(called,result) = RunTx(tx, MakeCounter(1, 1)));
		CHECK(called);
		CHECK_FALSE(result);
	}
	SECTION("Bad config is inert")
	{
		Json::Value bad;
		bad["FromType"] = "NotAType";
		bad["ToType"] = "Analog";
		TypeConversionTransform tx("Tx", bad);
		auto in = MakeCounter(1, 1);
		auto [called,result] = RunTx(tx, in);
		REQUIRE(called);
		CHECK(result == in);
	}

	TestTearDown();
}
