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
* CBUtility.cpp
*
*  Created on: 10/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "CB.h"
#include "CBUtility.h"


int GetBitsSet(uint32_t data, int numberofbits)
{
	int bitsset = 0;
	for (uint8_t i = 0; i < numberofbits; i++)
	{
		if (((data >> i) & 0x001) == 1)
			bitsset++;
	}
	return bitsset;
}
int GetSetBit(uint32_t data, int numberofbits)
{
	int bitsset = 0;
	for (uint8_t i = 0; i < numberofbits; i++)
	{
		if (((data >> i) & 0x001) == 1)
		{
			return numberofbits - i -1;
		}
	}
	return -1;
}

CBPoint::~CBPoint() {} // To keep the compiler/linker happy

CBBinaryPoint::~CBBinaryPoint() {}

// This not in header file due to odd gcc error
CBBinaryPoint& CBBinaryPoint::operator=(const CBBinaryPoint& src)
{
	if (this != &src) // Prevent self assignment
	{
		Index = src.Index;
		Group = src.Group;
		Channel = src.Channel;
		PayloadLocation = src.PayloadLocation;
		ChangedTime = src.ChangedTime;
		HasBeenSet = src.HasBeenSet;
		Binary = src.Binary;
		ModuleBinarySnapShot = src.ModuleBinarySnapShot;
		Changed = src.Changed;
		PointType = src.PointType;
	}
	return *this;
}

CBAnalogCounterPoint::~CBAnalogCounterPoint() {}

std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

static const uint8_t BCH[] = { 0x12,0x09,0x16,0xB,0x17,0x19,0x1E,0xF,0x15,0x18,0xC,0x6,0x3,
	                         0x13,0x1B,0x1F,0x1D,0x1C,0xE,0x7,0x11,0x1A,0xD,0x14,0xA,0x5 };

bool CBBCHCompare(const uint8_t bch1, const uint8_t bch2)
{
	// Just make sure we only compare the 5bits!
	return ((bch1 & 0x1f) == (bch2 & 0x1f));
}

uint8_t CBBCH(const uint32_t data)
{
	uint32_t bch = 0;

	// xor each of the sub - remainders corresponding to set bits in the first 26 bits of the payload
	for (uint8_t bit = 0; bit < 25; bit++)
	{
		if ((ShiftLeftResult32Bits(data, bit) & 0x80000000) == 0x80000000) //The bit is set
		{
			bch = (bch ^ BCH[bit]) & 0x1F; // Limit to 5 bits
		}
	}
	return numeric_cast<uint8_t>(bch & 0x0ff);
}

bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b)
		{
			return tolower(a) == tolower(b);
		});
}

std::string CBMessageAsString(const CBMessage_t& CompleteCBMessage)
{
	std::string res = "";
	// Output a human readable string containing the CB Data Blocks.
	for (auto block : CompleteCBMessage)
	{
		res += block.ToString() + " ";
	}
	return res;
}

CBTime CBNow()
{
	// To get the time to pass through ODC events. CB Uses UTC time in commands - as you would expect.
	return static_cast<CBTime>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Create an ASCII string version of the time from the CB time - which is msec since epoch.
std::string to_timestringfromCBtime(CBTime _time)
{
	time_t tp = _time/1000; // time_t is normally seconds since epoch. We deal in msec!

	std::tm* t = std::localtime(&tp);
	if (t != nullptr)
	{
		std::ostringstream oss;
		oss << std::put_time(t, "%c %z"); // Local time format, with offset from UTC
		return oss.str();
	}
	return "Time Conversion Problem";
}
int tz_offset()
{
	time_t when = std::time(nullptr);
	auto const tm = *std::localtime(&when);
	std::ostringstream os;
	os << std::put_time(&tm, "%z");
	std::string s = os.str();
	int h = std::stoi(s.substr(0, 3), nullptr, 10);
	int m = std::stoi(s[0] + s.substr(3), nullptr, 10);

	return h * 60 + m;
}
