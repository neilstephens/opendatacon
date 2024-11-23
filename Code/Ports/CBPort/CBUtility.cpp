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

CBTime CBNowUTC()
{
	// To get the time to pass through ODC events. CB Uses UTC time in commands - as you would expect.
	return static_cast<CBTime>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Create an ASCII string version of the time from the CB time - which is msec since epoch.
std::string to_LOCALtimestringfromCBtime(CBTime _time)
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
std::string to_stringfromCBtime(CBTime _time)
{
	uint8_t hhin;
	uint8_t mmin;
	uint8_t ssin;
	uint16_t msecin;
	to_hhmmssmmfromCBtime( _time, hhin, mmin, ssin, msecin);

	return to_stringfromhhmmssmsec(hhin, mmin, ssin, msecin);
}
std::string to_stringfromhhmmssmsec(uint8_t hh, uint8_t mm, uint8_t ss, uint16_t msec )
{
	std::string res = fmt::format("{}:{}:{}.{}", hh, mm, ss, msec);

	return res;
}
// CBTime is msec since epoch
void to_hhmmssmmfromCBtime(CBTime _time, uint8_t &hh, uint8_t &mm, uint8_t &ss, uint16_t &msec)
{
	msec = _time % 1000;
	_time = _time / 1000;
	ss = numeric_cast<uint8_t>(_time % 60);
	_time = _time / 60;
	mm = numeric_cast<uint8_t>(_time % 60);
	_time = _time / 60;
	hh = numeric_cast<uint8_t>(_time % 24);
}

void DecodeTimePayload(uint16_t P1B, uint16_t P2A, uint16_t P2B, uint8_t& hhin, uint8_t& mmin, uint8_t& ssin, uint16_t& msecin)
{
	hhin = (P1B & 0x07) << 2 | ((P2A >> 10) & 0x03);
	mmin = ((P2A >> 4) & 0x03F);
	ssin = (P2A & 0x0F) << 2 | ((P2B >> 10) & 0x03);
	msecin = (P2B & 0x03FF);
}
void PackageTimePayload(uint8_t hh, uint8_t mm, uint8_t ss, uint16_t msec, uint16_t& P1B, uint16_t& P2A, uint16_t& P2B)
{
	P1B = ((hh >> 2) & 0x07) | 0x20;                                                                       // The 2 is the number of blocks.	// Top 3 bits of hh
	P2A = ShiftLeftResult16Bits(hh & 0x03, 10) | ShiftLeftResult16Bits(mm & 0x3F, 4) | ((ss >> 2) & 0x0F); // Bottom 2 bits of hh, 6 bits of mm, top 4 bits of ss
	P2B = ShiftLeftResult16Bits(ss & 0x03, 10) | (msec & 0x3ff);                                           // bottom 2 bits of ss, 10 bits of msec
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
CBMessage_t BuildCBMessageFromASCIIHexString(const std::string Message)
{
	// Parse into blocks, add to the message. Used only for testing atm.
	assert(Message.size() % 8 == 0); // Hex string passed to build a CBMessage object must be a multiple of 8 in length
	CBMessage_t Msg;
	for (size_t i = 0; i < Message.size(); i += 8)
	{
		CBBlockData block(Message.substr(i,8));
		Msg.push_back(block);
	}
	return Msg;
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

std::string GetFunctionCodeName(uint8_t functioncode)
{
	switch (functioncode)
	{
		case FUNC_SCAN_DATA: return "SCAN_DATA";
		case     FUNC_EXECUTE_COMMAND: return "EXECUTE_COMMAND";
		case     FUNC_TRIP: return "TRIP";
		case     FUNC_SETPOINT_A: return "SETPOINT_A";
		case     FUNC_CLOSE: return "CLOSE";
		case     FUNC_SETPOINT_B: return "SETPOINT_B";
		case     FUNC_RESET: return "RESET";
		case     FUNC_MASTER_STATION_REQUEST: return "MASTER_STATION_REQUEST";
		case     FUNC_SEND_NEW_SOE: return "SEND_NEW_SOE";
		case     FUNC_REPEAT_SOE: return "REPEAT_SOE";
		case     FUNC_UNIT_RAISE_LOWER: return "";
		case     FUNC_FREEZE_AND_SCAN_ACC: return "FREEZE_AND_SCAN_ACC";
		case     FUNC_FREEZE_SCAN_AND_RESET_ACC: return "FREEZE_SCAN_AND_RESET_ACC";
		default: return "Unknown Function Code";
	}
}
std::string GetSubFunctionCodeName(uint8_t functioncode)
{
	switch (functioncode)
	{
		case MASTER_SUB_FUNC_0_NOTUSED: return "0_NOTUSED";
		case MASTER_SUB_FUNC_TESTRAM: return "TESTRAM";
		case MASTER_SUB_FUNC_TESTPROM: return "TESTPROM";
		case MASTER_SUB_FUNC_TESTEPROM: return "TESTEPROM";
		case MASTER_SUB_FUNC_TESTIO: return "TESTIO";
		case MASTER_SUB_FUNC_SEND_TIME_UPDATES: return "SEND_TIME_UPDATES";
		case MASTER_SUB_FUNC_SPARE1: return "SPARE1";
		case MASTER_SUB_FUNC_SPARE2: return "SPARE2";
		case MASTER_SUB_FUNC_RETRIEVE_REMOTE_STATUS_WORD: return "RETRIEVE_REMOTE_STATUS_WORD";
		case MASTER_SUB_FUNC_RETREIVE_INPUT_CIRCUIT_DATA: return "RETREIVE_INPUT_CIRCUIT_DAT";
		case MASTER_SUB_FUNC_TIME_CORRECTION_FACTOR_ESTABLISHMENT: return "TIME_CORRECTION_FACTOR_ESTABLISHMENT";
		case MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION: return "REPEAT_PREVIOUS_TRANSMISSION";
		case MASTER_SUB_FUNC_SPARE3: return "SPARE3";
		case MASTER_SUB_FUNC_SET_LOOPBACKS: return "SET_LOOPBACKS";
		case MASTER_SUB_FUNC_RESET_RTU_WARM: return "RESET_RTU_WARM";
		case MASTER_SUB_FUNC_RESET_RTU_COLD: return "RESET_RTU_COLD";
		default: return "Unknown Sub-Function Code";
	}
}