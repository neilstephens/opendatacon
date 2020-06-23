/*	opendatacon
*
*	Copyright (c) 2018:
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
* MD3Utility.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "MD3.h"
#include "MD3Utility.h"

// Maybe need an MD3.cpp file to be clear?

MD3Point::~MD3Point() {} // To keep the compiler/linker happy

MD3BinaryPoint::~MD3BinaryPoint() {}

// This not in header file due to odd gcc error
MD3BinaryPoint& MD3BinaryPoint::operator=(const MD3BinaryPoint& src)
{
	if (this != &src) // Prevent self assignment
	{
		Index = src.Index;
		ModuleAddress = src.ModuleAddress;
		Channel = src.Channel;
		PollGroup = src.PollGroup;
		ChangedTime = src.ChangedTime;
		HasBeenSet = src.HasBeenSet;
		Binary = src.Binary;
		ModuleBinarySnapShot = src.ModuleBinarySnapShot;
		Changed = src.Changed;
		PointType = src.PointType;
	}
	return *this;
}

MD3AnalogCounterPoint::~MD3AnalogCounterPoint() {}

static const uint8_t fcstab[256] =
{
	0x00, 0x6c, 0xd8, 0xb4, 0xdc, 0xb0, 0x04, 0x68, 0xd4, 0xb8, 0x0c, 0x60, 0x08, 0x64, 0xd0, 0xbc,
	0xc4, 0xa8, 0x1c, 0x70, 0x18, 0x74, 0xc0, 0xac, 0x10, 0x7c, 0xc8, 0xa4, 0xcc, 0xa0, 0x14, 0x78,
	0xe4, 0x88, 0x3c, 0x50, 0x38, 0x54, 0xe0, 0x8c, 0x30, 0x5c, 0xe8, 0x84, 0xec, 0x80, 0x34, 0x58,
	0x20, 0x4c, 0xf8, 0x94, 0xfc, 0x90, 0x24, 0x48, 0xf4, 0x98, 0x2c, 0x40, 0x28, 0x44, 0xf0, 0x9c,
	0xa4, 0xc8, 0x7c, 0x10, 0x78, 0x14, 0xa0, 0xcc, 0x70, 0x1c, 0xa8, 0xc4, 0xac, 0xc0, 0x74, 0x18,
	0x60, 0x0c, 0xb8, 0xd4, 0xbc, 0xd0, 0x64, 0x08, 0xb4, 0xd8, 0x6c, 0x00, 0x68, 0x04, 0xb0, 0xdc,
	0x40, 0x2c, 0x98, 0xf4, 0x9c, 0xf0, 0x44, 0x28, 0x94, 0xf8, 0x4c, 0x20, 0x48, 0x24, 0x90, 0xfc,
	0x84, 0xe8, 0x5c, 0x30, 0x58, 0x34, 0x80, 0xec, 0x50, 0x3c, 0x88, 0xe4, 0x8c, 0xe0, 0x54, 0x38,
	0x24, 0x48, 0xfc, 0x90, 0xf8, 0x94, 0x20, 0x4c, 0xf0, 0x9c, 0x28, 0x44, 0x2c, 0x40, 0xf4, 0x98,
	0xe0, 0x8c, 0x38, 0x54, 0x3c, 0x50, 0xe4, 0x88, 0x34, 0x58, 0xec, 0x80, 0xe8, 0x84, 0x30, 0x5c,
	0xc0, 0xac, 0x18, 0x74, 0x1c, 0x70, 0xc4, 0xa8, 0x14, 0x78, 0xcc, 0xa0, 0xc8, 0xa4, 0x10, 0x7c,
	0x04, 0x68, 0xdc, 0xb0, 0xd8, 0xb4, 0x00, 0x6c, 0xd0, 0xbc, 0x08, 0x64, 0x0c, 0x60, 0xd4, 0xb8,
	0x80, 0xec, 0x58, 0x34, 0x5c, 0x30, 0x84, 0xe8, 0x54, 0x38, 0x8c, 0xe0, 0x88, 0xe4, 0x50, 0x3c,
	0x44, 0x28, 0x9c, 0xf0, 0x98, 0xf4, 0x40, 0x2c, 0x90, 0xfc, 0x48, 0x24, 0x4c, 0x20, 0x94, 0xf8,
	0x64, 0x08, 0xbc, 0xd0, 0xb8, 0xd4, 0x60, 0x0c, 0xb0, 0xdc, 0x68, 0x04, 0x6c, 0x00, 0xb4, 0xd8,
	0xa0, 0xcc, 0x78, 0x14, 0x7c, 0x10, 0xa4, 0xc8, 0x74, 0x18, 0xac, 0xc0, 0xa8, 0xc4, 0x70, 0x1c
};

bool MD3CRCCompare(const uint8_t crc1, const uint8_t crc2)
{
	// Just make sure we only compare the 6 bits!
	return ((crc1 & 0x3f) == (crc2 & 0x3f));
}

uint8_t MD3CRC(const uint32_t data)
{
	uint8_t CRCChar = fcstab[(data >> 24) & 0x0ff];
	CRCChar = fcstab[((data >> 16) ^ CRCChar) & 0xff];
	CRCChar = fcstab[((data >> 8) ^ CRCChar) & 0xff];
	CRCChar = fcstab[(data  ^ CRCChar) & 0xff];

	CRCChar = ~(CRCChar >> 2) & 0x3F;

	return CRCChar;
}

// Case insensitive equals
bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b)
		{
			return tolower(a) == tolower(b);
		});
}

std::string MD3MessageAsString(const MD3Message_t& CompleteMD3Message)
{
	std::string res = "";
	// Output a human readable string containing the MD3 Data Blocks.
	for (auto block : CompleteMD3Message)
	{
		res += block.ToString() + " ";
	}
	return res;
}

MD3Time MD3NowUTC()
{
	// To get the time to pass through ODC events. MD3 Uses UTC time in commands - as you would expect.
	return static_cast<MD3Time>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Create an ASCII string version of the time from the MD3 time - which is msec since epoch.
std::string to_LOCALtimestringfromMD3time(MD3Time _time)
{
	time_t tp = _time/1000; // time_t is normally seconds since epoch. We deal in msec!

	std::tm* t = std::localtime(&tp);
	if (t != nullptr)
	{
		char timestr[100];
		std::strftime(timestr,100,"%c %z",t); // Local time format, with offset from UTC
		return std::string(timestr);
	}
	return "Time Conversion Problem";
}
std::string to_stringfromMD3time(MD3Time _time)
{
	uint16_t msec = _time % 1000;
	_time = _time / 1000;
	auto ss = numeric_cast<uint8_t>(_time % 60);
	_time = _time / 60;
	auto mm = numeric_cast<uint8_t>(_time % 60);
	_time = _time / 60;
	auto hh = numeric_cast<uint8_t>(_time % 24);

	return fmt::format("{}:{}:{}.{}", hh, mm, ss, msec);
}
int tz_offset()
{
	time_t when = std::time(nullptr);
	auto const tm = *std::localtime(&when);
	char tzoffstr[6];
	std::strftime(tzoffstr,6,"%z",&tm);
	std::string s(tzoffstr);
	int h = std::stoi(s.substr(0, 3), nullptr, 10);
	int m = std::stoi(s[0] + s.substr(3), nullptr, 10);

	return h * 60 + m;
}
