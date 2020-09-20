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
 * ConfigParserTests.cpp
 *
 *  Created on: 19/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "TestODCHelpers.h"
#include <opendatacon/ConfigParser.h>
#include <json/json.h>
#include <catch.hpp>
#include <fstream>
#include <sstream>

using namespace odc;

#define SUITE(name) "ConfigParserTestSuite - " name

class TestConfigParser: public ConfigParser
{
public:
	TestConfigParser(const std::string& aConfFilename, const Json::Value& aConfOverrides = Json::Value()):
		ConfigParser(aConfFilename, aConfOverrides)
	{}
	void Go()
	{
		ProcessFile();
	}
	void ProcessElements(const Json::Value &JSONRoot) final
	{
		auto members = JSONRoot.getMemberNames();
		for(auto member : members)
		{
			FinalConfig[member] = JSONRoot[member];
		}
	}
	Json::Value FinalConfig;
};

const char* parent_file1_s =
	R"001(
{
	"One" : "Parent1",
	"Two" : "Parent1"
}
)001";

const char* parent_file2_s =
	R"001(
{
	"Two" : "Parent2",
	"Three" : "Parent2"
}
)001";

const char* config_file_s =
	R"001(
{
	"Inherits" : ["Inher1.test","Inher2.test"],
	"Three" : "Own",
	"Four" : "Own"
}
)001";

const char* overrides_s =
	R"001(
{
	"Four" : "Override",
	"Five" : "Override"
}
)001";

Json::Value PrepInputs()
{
	std::ofstream fconf("Conf.test");
	REQUIRE_FALSE(fconf.fail());
	fconf << config_file_s;
	fconf.close();

	std::ofstream finher1("Inher1.test");
	REQUIRE_FALSE(finher1.fail());
	finher1 << parent_file1_s;
	finher1.close();

	std::ofstream finher2("Inher2.test");
	REQUIRE_FALSE(finher2.fail());
	finher2 << parent_file2_s;
	finher2.close();

	Json::Value overrides;
	std::stringstream(overrides_s) >> overrides;
	return overrides;
}

void RemoveInputs()
{
	std::remove("Conf.test");
	std::remove("Inher1.test");
	std::remove("Inher2.test");
}

TEST_CASE(SUITE("Elements"))
{
	TestSetup();

	PrepInputs();
	TestConfigParser test_parser("Inher1.test");
	test_parser.Go();

	Json::Value expected;
	std::stringstream(R"001(
				{
					  "One" : "Parent1",
					  "Two" : "Parent1"
				}
				)001") >> expected;

	REQUIRE(test_parser.FinalConfig == expected);

	RemoveInputs();
	TestTearDown();
}

TEST_CASE(SUITE("Inherits"))
{
	TestSetup();

	PrepInputs();
	TestConfigParser test_parser("Conf.test");
	test_parser.Go();

	Json::Value expected;
	std::stringstream(R"001(
				{
					  "Inherits" :
					  [
						    "Inher1.test",
						    "Inher2.test"
					  ],
					  "One" : "Parent1",
					  "Two" : "Parent2",
					  "Three" : "Own",
					  "Four" : "Own"
				}
				)001") >> expected;

	REQUIRE(test_parser.FinalConfig == expected);

	RemoveInputs();
	TestTearDown();
}

TEST_CASE(SUITE("Overrides"))
{
	TestSetup();

	auto overrides = PrepInputs();
	TestConfigParser test_parser("Conf.test", overrides);
	test_parser.Go();

	Json::Value expected;
	std::stringstream(R"001(
				{
					  "Inherits" :
					  [
						    "Inher1.test",
						    "Inher2.test"
					  ],
					  "One" : "Parent1",
					  "Two" : "Parent2",
					  "Three" : "Own",
					  "Four" : "Override",
					  "Five" : "Override"
				}
				)001") >> expected;

	REQUIRE(test_parser.FinalConfig == expected);

	RemoveInputs();
	TestTearDown();
}

TEST_CASE(SUITE("Cache"))
{
	TestSetup();

	auto overrides = PrepInputs();
	TestConfigParser test_parser("Conf.test", overrides);
	test_parser.Go();

	RemoveInputs();

	//this will only work if the cache works, because the files a gone now
	TestConfigParser test_parser2("Conf.test", overrides);
	test_parser2.Go();

	REQUIRE(test_parser.FinalConfig == test_parser2.FinalConfig);

	TestTearDown();
}

TEST_CASE(SUITE("Get"))
{
	TestSetup();

	auto overrides = PrepInputs();
	TestConfigParser test_parser("Conf.test", overrides);
	test_parser.Go();

	Json::Value expected;
	std::stringstream(R"001(
	{
		  "Conf.test" :
		  {
			    "Four" : "Own",
			    "Inherits" :
			    [
					"Inher1.test",
					"Inher2.test"
			    ],
			    "Three" : "Own"
		  },
		  "ConfigOverrides" :
		  {
			    "Five" : "Override",
			    "Four" : "Override"
		  },
		  "Inher1.test" :
		  {
			    "One" : "Parent1",
			    "Two" : "Parent1"
		  },
		  "Inher2.test" :
		  {
			    "Three" : "Parent2",
			    "Two" : "Parent2"
		  }
	})001") >> expected;

	auto got = test_parser.GetConfiguration();
	REQUIRE(got == expected);

	RemoveInputs();
	TestTearDown();
}
