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
 *  Tests of the translation config of the ports. They don't need a Kafka cluster,
 *  because they only construct and Build() the ports
 */
#include "Helpers.h"
#include "../PortLoader.h"
#include <catch.hpp>
#include <json/json.h>
#include <opendatacon/Translation.h>
#include <sstream>

#define SUITE(name) "KafkaPortConfigTestSuite - " name

namespace
{
Json::Value ParseConf(const std::string& str)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream ss(str);
	REQUIRE(Json::parseFromStream(builder, ss, &root, &errs));
	return root;
}

//Make a port, and Build() it. Returns whether Build() succeeded
bool TryBuild(const std::string& PortType, const std::string& conf_str)
{
	TestSetup();
	auto portlib = LoadModule(GetLibFileName("KafkaPort"));
	REQUIRE(portlib);
	bool built = false;
	{
		newptr newPort = GetPortCreator(portlib, PortType);
		REQUIRE(newPort);
		delptr deletePort = GetPortDestroyer(portlib, PortType);
		REQUIRE(deletePort);

		std::shared_ptr<DataPort> PUT(newPort("PortUnderTest", "", ParseConf(conf_str)), deletePort);
		try
		{
			PUT->Build();
			built = true;
		}
		catch(const std::exception&)
		{}
	}
	TestTearDown();
	UnLoadModule(portlib);
	return built;
}
}

TEST_CASE(SUITE("Existing style config builds"))
{
	const std::string conf = R"({
		"DefaultTopic": "mytopic",
		"DefaultKey": "mykey",
		"DefaultTemplate": "<INDEX>,<EVENTTYPE>,<PAYLOAD>",
		"DefaultCBORStructure": "[\"EVENTTYPE_RAW\",\"INDEX\",\"PAYLOAD\"]",
		"OctetStringFormat": "Hex",
		"DateTimeFormat": "%Y-%m-%d",
		"DateTimeIsUTC": true,
		"PointTranslationSource": "SenderName",
		"BlockUnknownPoints": true,
		"PointTranslationMap": {
			"Analog": [
				{"Index": 1, "Topic": "other", "Key": "k1", "Template": "x<PAYLOAD>", "Description": "a point"},
				{"Range": {"Start": 5, "Stop": 9}, "Source": "Snd", "CBORStructure": ["INDEX","PAYLOAD"]}
			]
		}
	})";
	CHECK(TryBuild("KafkaProducer", conf));
	CHECK(TryBuild("KafkaConsumer", conf));
}

TEST_CASE(SUITE("No config builds"))
{
	CHECK(TryBuild("KafkaProducer", "{}"));
	CHECK(TryBuild("KafkaConsumer", "{}"));
}

TEST_CASE(SUITE("Key templates"))
{
	const std::string conf = R"({
		"TranslationMethod": "Template",
		"DefaultTemplate": "<PAYLOAD>",
		"KeyTranslationMethod": "Template",
		"DefaultKeyTemplate": "site/<EVENTTYPE>/<INDEX>",
		"PointTranslationMap": { "Analog": [ {"Range": {"Start": 0, "Stop": 20}} ] }
	})";
	CHECK(TryBuild("KafkaProducer", conf));
	CHECK(TryBuild("KafkaConsumer", conf));

	const std::string cbor_conf = R"({
		"KeyTranslationMethod": "CBOR",
		"DefaultKeyCBORStructure": "[\"EVENTTYPE_RAW\",\"INDEX\"]",
		"PointTranslationMap": { "Analog": [ {"Range": {"Start": 0, "Stop": 20}} ] }
	})";
	CHECK(TryBuild("KafkaProducer", cbor_conf));
	CHECK(TryBuild("KafkaConsumer", cbor_conf));
}

TEST_CASE(SUITE("Non-static keys need DYNAMIC"))
{
	std::string conf = R"({
		"KeyTranslationMethod": "Template",
		"DefaultKeyTemplate": "site/<INDEX>/<TIMESTAMP>",
		"KeyClashDetection": "WARN",
		"PointTranslationMap": { "Analog": [ {"Index": 1} ] }
	})";
	CHECK_FALSE(TryBuild("KafkaProducer", conf));
	CHECK_FALSE(TryBuild("KafkaConsumer", conf));
	conf.replace(conf.find("WARN"), 4, "STRICT");
	CHECK_FALSE(TryBuild("KafkaProducer", conf));
	conf.replace(conf.find("STRICT"), 6, "DYNAMIC");
	CHECK(TryBuild("KafkaProducer", conf));
	CHECK(TryBuild("KafkaConsumer", conf));
}

TEST_CASE(SUITE("Key clashes"))
{
	std::string conf = R"({
		"DefaultKey": "shared",
		"KeyClashDetection": "WARN",
		"PointTranslationMap": { "Analog": [ {"Index": 1}, {"Index": 2, "Template": "different <PAYLOAD>"} ] }
	})";
	CHECK(TryBuild("KafkaConsumer", conf));
	//the producer doesn't care
	CHECK(TryBuild("KafkaProducer", conf));
	conf.replace(conf.find("WARN"), 4, "STRICT");
	CHECK_FALSE(TryBuild("KafkaConsumer", conf));
	CHECK(TryBuild("KafkaProducer", conf));
}

TEST_CASE(SUITE("Lua translation"))
{
	//The Lua translation is loaded when needed from the LuaWrappers library, which is optional
	const bool lua_available = odc::TranslationFactory::IsAvailable(odc::EventTranslationMethod::Lua);
	if(!lua_available)
		WARN("The LuaWrappers library isn't available, so Lua translation is expected to fail");

	const std::string conf = R"({
		"TranslationMethod": "Lua",
		"DefaultLua": "function serialise(event) return 'x' end function deserialise(payload) return nil end",
		"KeyTranslationMethod": "Lua",
		"DefaultKeyLua": "function key(event) return 'k'..event.Index end",
		"PointTranslationMap": { "Analog": [ {"Range": {"Start": 0, "Stop": 5}} ] }
	})";
	CHECK(TryBuild("KafkaProducer", conf) == lua_available);
	CHECK(TryBuild("KafkaConsumer", conf) == lua_available);

	//no code
	CHECK_FALSE(TryBuild("KafkaProducer", R"({"TranslationMethod": "Lua"})"));
	//a missing function
	CHECK_FALSE(TryBuild("KafkaProducer", R"({"TranslationMethod": "Lua", "DefaultLua": "function deserialise() end"})"));
	//(a consumer only builds deserialisers for the points it has)
	CHECK(TryBuild("KafkaConsumer", R"({"TranslationMethod": "Lua", "DefaultLua": "function deserialise() end", "PointTranslationMap": {"Analog": [{"Index": 1}]}})") == lua_available);
	//bad syntax
	CHECK_FALSE(TryBuild("KafkaProducer", R"({"TranslationMethod": "Lua", "DefaultLua": "function ("})"));
}
