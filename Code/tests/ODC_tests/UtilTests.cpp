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

