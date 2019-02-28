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
		MomentaryChangeStatus = src.MomentaryChangeStatus;
		Changed = src.Changed;
		PointType = src.PointType;
		SOEPoint = src.SOEPoint;
		SOEIndex = src.SOEIndex;
		SOEGroup = src.SOEGroup;
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

CBTime CBNow()
{
	// To get the time to pass through ODC events. CB Uses UTC time in commands - as you would expect.
	return static_cast<CBTime>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Create an ASCII string version of the time from the CB time - which is msec since epoch.
std::string to_timestringfromCBtime(CBTime _time)
{
	time_t tp = _time / 1000; // time_t is normally seconds since epoch. We deal in msec!

	std::tm* t = std::localtime(&tp);
	if (t != nullptr)
	{
		char timestr[100];
		std::strftime(timestr, 100, "%c %z", t); // Local time format, with offset from UTC
		return std::string(timestr);
	}
	return "Time Conversion Problem";
}
void to_hhmmssmmfromCBtime(CBTime _time, uint8_t &hh, uint8_t &mm, uint8_t &ss, uint16_t &msec)
{
	time_t tp = _time / 1000; // time_t is normally seconds since epoch. We deal in msec!
	msec = _time % 1000;

	std::tm* t = std::gmtime(&tp);
	if (t != nullptr)
	{
		hh = numeric_cast<uint8_t>(t->tm_hour);
		mm = numeric_cast<uint8_t>(t->tm_min);
		ss = numeric_cast<uint8_t>(t->tm_sec);
	}
}

int tz_offset()
{
	time_t when = std::time(nullptr);
	auto const tm = *std::localtime(&when);
	char tzoffstr[6];
	std::strftime(tzoffstr, 6, "%z", &tm);
	std::string s(tzoffstr);
	int h = std::stoi(s.substr(0, 3), nullptr, 10);
	int m = std::stoi(s[0] + s.substr(3), nullptr, 10);

	return h * 60 + m;
}
// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
std::string BuildBinaryStringFromASCIIHexString(const std::string &as)
{
	assert(as.size() % 2 == 0); // Must be even length

	// Create, we know how big it will be
	auto res = std::string(as.size() / 2, 0);

	// Take chars in chunks of 2 and convert to a hex equivalent
	for (size_t i = 0; i < (as.size() / 2); i++)
	{
		auto hexpair = as.substr(i * 2, 2);
		res[i] = static_cast<char>(std::stol(hexpair, nullptr, 16));
	}
	return res;
}
// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a binary string, and produces an ascii hex string in the format of "FF120D567200"g
std::string BuildASCIIHexStringfromBinaryString(const std::string &bs)
{
	// Create, we know how big it will be
	auto res = std::string(bs.size() * 2, 0);

	constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7','8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	for (size_t i = 0; i < bs.size(); i++)
	{
		res[2 * i] = hexmap[(bs[i] & 0xF0) >> 4];
		res[2 * i + 1] = hexmap[bs[i] & 0x0F];
	}

	return res;
}
std::string BuildASCIIHexStringfromCBMessage(const CBMessage_t & CBMessage)
{
	// Create, we know how big it will be
	auto res = std::string(CBMessage.size() * 4, 0);

	constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7','8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	for (size_t i = 0; i < CBMessage.size(); i++)
	{
		for (int j = 0; j < 4; j++)
			res[4 * i + j] = hexmap[CBMessage[i].GetByte(j) & 0x0F];
	}

	return res;
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