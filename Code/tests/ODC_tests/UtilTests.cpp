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
 *  Created on: 19/02/2023
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/util.h>
#include <catch.hpp>
#include <string>
#include <fstream>
#include <chrono>
#include <filesystem>

using namespace odc;

#define SUITE(name) "UtilTestSuite - " name

TEST_CASE(SUITE("DateTime"))
{
	std::string raw_date = "1984-04-19 07:53:30.951";
	auto since_epoch = datetime_to_since_epoch(raw_date);
	INFO("Milliseconds since epoch: " << since_epoch);
	auto parsed_date = since_epoch_to_datetime(since_epoch);
	CHECK(parsed_date == raw_date);
}

TEST_CASE(SUITE("FileSystemTime"))
{
	const std::string fname("fstimetestfile");
	std::ofstream fout(fname);
	fout<<'\0';

	auto sys_now = std::chrono::system_clock::now();
	auto fs_now = std::filesystem::last_write_time(std::filesystem::path(fname));
	auto fs_to_sys = fs_to_sys_time_point(fs_now);

	auto diff = std::chrono::round<std::chrono::milliseconds>(sys_now - fs_to_sys).count();

	INFO("System time: " << since_epoch_to_datetime(std::chrono::round<std::chrono::milliseconds>(sys_now).time_since_epoch().count()));
	INFO("Filesystem time: " << since_epoch_to_datetime(std::chrono::round<std::chrono::milliseconds>(fs_to_sys).time_since_epoch().count()));

	CHECK(diff < 200);
}

TEST_CASE(SUITE("CRC16"))
{
	std::map<uint16_t,std::vector<uint8_t>> test_data =
	{
		{
			//result
			0x313E,
			//data
			{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
		},
		{
			//result
			0x798A,
			//data
			{0x64,0x66,0x67,0x68,0x64,0x68,0x6b,0x6a,
			 0x64,0x67,0x68,0x6b,0x34,0x75,0x35,0x36,
			 0x75,0x23,0x24,0x5e,0x24,0x25,0x26,0x2a}
		},
		{
			//result
			0x453C,
			//data
			{0x33,0x34,0x77,0x65,0x72,0x67,0x65,0x23,
			 0x24,0x5e,0x2a,0x25,0x24,0x26,0x2a,0x67,
			 0x64,0x68,0x64,0x66,0x34,0x33,0x77,0x36,
			 0x24,0x25,0x5e,0x26,0x5e}
		},
		{
			//result
			0x91F3,
			//data
			{0x24,0x25,0x5e,0x64,0x68,0x74,0x37,0x34,
			 0x64,0x72,0x67,0x68}
		},
		{
			//result
			0x29B1,
			//data
			{0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
			 0x39 }
		},
	};
	for(auto& [expected_result,data] : test_data)
	{
		auto result = crc_ccitt(data.data(),data.size());
		CAPTURE(data);
		CHECK(result == expected_result);
	}
	std::vector<uint8_t> data = {0x64,0x66,0x67,0x69,0x6a,0x23,0x24,0x5e,0x26,0x2a,0x29,0x40,0x2b,0x7c};
	auto result = crc_ccitt(data.data(),data.size(),0,0x8005);
	CAPTURE(data);
	CHECK(result == 0xAFC1);
}

