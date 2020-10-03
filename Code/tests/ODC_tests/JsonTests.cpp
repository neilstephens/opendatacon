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
 * JsonTests.cpp
 *
 *  Created on: 19/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "TestODCHelpers.h"
#include <json/json.h>
#include <sstream>
#include <catch.hpp>

using namespace odc;

#define SUITE(name) "JsonTestSuite - " name

TEST_CASE(SUITE("JsonCompare"))
{
	TestSetup();

	const char* sample_json_s = R"001(
	{
		//comment
		"HttpIP" : "0.0.0.0",
		"HttpPort" : 9000,
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",

		"Analogs" :
		[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],

		"BinaryControls" :
		[
			{
				"Index" : 0,
				"FeedbackBinaries":
				[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* immaterial_changes_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",
		"Analogs" :[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* tiny_change_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123356789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",
		"Analogs" :[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* arrayorder_change_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",
		"Analogs" :[
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* string_change_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020 ",
		"Analogs" :[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* deep_change_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",
		"Analogs" :[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":false,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* addition_change_s = R"001(
	{
		//comments changed
		"Number" : 123.456789123456789,
		"Version" : "Dummy#$^&*% Version 2-3-2020",
		"Analogs" :[
			{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 10000, "StdDev" : 2},
			{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
			{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
			{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
		],
		//and order/indents changed
	   "HttpIP" : "0.0.0.0",
	   "HttpPort" : 9000,
		"HttpPort2" : 9000,
		"BinaryControls" :[{
				"Index" : 0,
				"FeedbackBinaries":[
					{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
					{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
				]
			}
		]
	})001";

	const char* deletion_change_s = R"001(
		{
			//comments changed
			"Number" : 123.456789123456789,
			"Version" : "Dummy#$^&*% Version 2-3-2020",
			"Analogs" :[
				{"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "StdDev" : 2},
				{"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 10000, "StdDev" : 5},
				{"Index" : 7, "StartVal" : 5, "StdDev" : 0}, //this is the tap position feedback
				{"Index" : 19281919, "StartVal" : 240, "UpdateIntervalms" : 10000, "StdDev" : 5}
			],
			//and order/indents changed
		   "HttpIP" : "0.0.0.0",
		   "HttpPort" : 9000,
			"BinaryControls" :[{
					"Index" : 0,
					"FeedbackBinaries":[
						{"Index":0,"FeedbackMode":"LATCH","OnValue":true,"OffValue":false},
						{"Index":1,"FeedbackMode":"LATCH","OnValue":false,"OffValue":true}
					]
				}
			]
		})001";

	Json::Value sample_json;
	Json::Value immaterial_changes;
	Json::Value string_change;
	Json::Value arrayorder_change;
	Json::Value deep_change;
	Json::Value addition_change;
	Json::Value deletion_change;
	Json::Value tiny_change;

	std::stringstream(sample_json_s)        >> sample_json;
	std::stringstream(immaterial_changes_s) >> immaterial_changes;
	std::stringstream(string_change_s)      >> string_change;
	std::stringstream(arrayorder_change_s)  >> arrayorder_change;
	std::stringstream(deep_change_s)        >> deep_change;
	std::stringstream(addition_change_s)    >> addition_change;
	std::stringstream(deletion_change_s)    >> deletion_change;
	std::stringstream(tiny_change_s)        >> tiny_change;

	REQUIRE(immaterial_changes == sample_json);

	REQUIRE(string_change != sample_json);
	REQUIRE(string_change != immaterial_changes);

	REQUIRE(arrayorder_change != sample_json);
	REQUIRE(arrayorder_change != immaterial_changes);

	REQUIRE(deep_change != sample_json);
	REQUIRE(deep_change != immaterial_changes);

	REQUIRE(addition_change != sample_json);
	REQUIRE(addition_change != immaterial_changes);

	REQUIRE(deletion_change != sample_json);
	REQUIRE(deletion_change != immaterial_changes);

	REQUIRE(tiny_change != sample_json);
	REQUIRE(tiny_change != immaterial_changes);

	TestTearDown();
}
