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
* CBUtility.h
*
*  Created on: 10/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef CBENGINE_H_
#define CBENGINE_H_

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "CB.h"

#define CONITEL_BLOCK_LENGTH  4
#define MAX_BLOCK_COUNT       16

// Function codes, note not all are valid...
#define     FUNC_SCAN_DATA                      0
#define     FUNC_EXECUTE_COMMAND                1
#define     FUNC_TRIP                           2
#define     FUNC_SETPOINT_A                     3
#define     FUNC_CLOSE                          4
#define     FUNC_SETPOINT_B                     5
#define     FUNC_RESET                          8
#define     FUNC_MASTER_STATION_REQUEST         9
#define     FUNC_SEND_NEW_SOE                   10
#define     FUNC_REPEAT_SOE                     11
#define     FUNC_UNIT_RAISE_LOWER               13
#define     FUNC_FREEZE_AND_SCAN_ACC            14
#define     FUNC_FREEZE_SCAN_AND_RESET_ACC      15

// This will create multiple copies, one in each file that uses this include file...
const std::map<uint8_t, std::string> FunctionCodeStrings = {
	{ FUNC_SCAN_DATA,                       "Scan Data"  },
	{ FUNC_EXECUTE_COMMAND,           "Execute Command" },
	{ FUNC_TRIP,                  "Trip" },
	{ FUNC_SETPOINT_A,            "Setpoint A" },
	{ FUNC_CLOSE,                 "Close" },
	{ FUNC_SETPOINT_B,            "Setpoint B" },
	{ FUNC_RESET,                 "Reset RTU" },
	{ FUNC_MASTER_STATION_REQUEST,"Master Station Request" },
	{ FUNC_SEND_NEW_SOE,          "Send New SOE" },
	{ FUNC_REPEAT_SOE,            "Repeat SOE" },
	{ FUNC_UNIT_RAISE_LOWER,      "Unit Raise / Lower" },
	{ FUNC_FREEZE_AND_SCAN_ACC,   "Freeze and Scan Accumulators" },
	{ FUNC_FREEZE_SCAN_AND_RESET_ACC, "Freeze, Scan and Reset Accumulators" }
};

// Master Station Request Sub-Codes

#define     MASTER_SUB_FUNC_SEND_TIME_UPDATES                     5
#define     MASTER_SUB_FUNC_RETRIEVE_REMOTE_STATUS_WORD           8
#define     MASTER_SUB_FUNC_TIME_CORRECTION_FACTOR_ESTABLISHMENT  10
#define     MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION          11
#define     MASTER_SUB_FUNC_SET_LOOPBACKS                         13
#define     MASTER_SUB_FUNC_RESET_RTU                             15

const std::map<uint8_t, std::string> SubFunctionCodeStrings = {
	{ MASTER_SUB_FUNC_SEND_TIME_UPDATES,                            "Send Time Updates" },
	{ MASTER_SUB_FUNC_RETRIEVE_REMOTE_STATUS_WORD,            "Retrieve Remote Status Word" },
	{ MASTER_SUB_FUNC_TIME_CORRECTION_FACTOR_ESTABLISHMENT,"Time Correction Factor Establishment" },
	{ MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION,       "Repeat Previous Transmission" },
	{ MASTER_SUB_FUNC_SET_LOOPBACKS,                      "Set Loopbacks" },
	{ MASTER_SUB_FUNC_RESET_RTU,                          "Reset RTU" }
};


const int CBBlockArraySize = CONITEL_BLOCK_LENGTH;
typedef  std::array<uint8_t, CBBlockArraySize> CBBlockArray;

uint8_t CBBCH(const uint32_t data);
bool CBBCHCompare(const uint8_t crc1, const uint8_t crc2);

bool iequals(const std::string& a, const std::string& b);

inline uint16_t ShiftLeftResult16Bits( uint16_t val, uint8_t shift)
{
	return numeric_cast<uint16_t>(val << shift);
}
inline uint16_t ShiftLeft8Result16Bits( uint16_t val)
{
	return numeric_cast<uint16_t>(val << 8);
}
inline uint32_t ShiftLeftResult32Bits(uint32_t val, uint8_t shift)
{
	return numeric_cast<uint32_t>(val << shift);
}
inline char ToChar(uint8_t v)
{
	return numeric_cast<char>(v);
}
int BitsSet(uint32_t data, int numberofbits);

std::vector<std::string> split(const std::string& s, char delimiter);

template <class T>
std::string to_hexstring(T val)
{
	std::stringstream sstream;
	sstream << std::hex << val;
	return sstream.str();
}
template <class T>
std::string to_binstring(T val)
{
	std::stringstream sstream;
	sstream << std::setbase(2) << val;
	return sstream.str();
}

// Create an ASCII string version of the time from the CB time - which is msec since epoch.
std::string to_timestringfromCBtime(CBTime _time);
// Return the current UTC offset in minutes.
int tz_offset();

// Every block in CB is 4 bytes. If we don't have 4 bytes, we don't have a block. The last block is marked as such.
// There are two ways this class is used - one for decoding, one for encoding.
// There are two basic packet types - Address and Data. The Address packet is always echoed to the Master so the Master can be sure the correct Station is responding.
// The top 12 bits A section is address/group/function code when bit A is 0. When bit A is 1, the top 12 bit is just data.
// The 13th bit is the A bit. The next 12 buts are the B data, then the next bit is the B bit which is always 0.
// Section C is the last 6 bits. 5 BCH bits and the EOM bit last.
// For a control function (from master) the B section contains 1 of 12 devices to be controlled in a group, or the desired value of a remotely controlled set point.
//
// So the data we get back from a scan (for example) is totally dependent on how the group is defined. The group can be a mix of several different point types.
// The master and outstation both need to know the data layout, otherwise things will not work!!!
// This is different to some other protocols, so will require a few different choices in implementation.
//
// The point types for a Scan are:
//
// Digital - DIG
// Momentary Contact Digital - MCA, MCB, MCC
// 12 bit accumulators - ACC
// 24 bit accumulators - ACC (Note all accumulators must be returned on one scan group only.
// Analog - +-2047 (12 bit)
// Remote Status Byte - RST - Bit codes defined in an extract method below.
//
// The configuration file for the Python simulator actually defines the group, and the "location" that the data defined will take in the returned packet.
// This is specified in terms of 1B, 2A, 2B where the first number is the packet number, the second is the A or B payload location.
// We will follow the same method.

class CBBlockData
{
public:
	enum blocktype {AddressBlock, DataBlock};

	CBBlockData() {}

	// We have received 4 bytes (a block on a stream now we need to decode it) it may not be valid!
	explicit CBBlockData(const CBBlockArray _data)
	{
		data = CombineDataBytes(_data[0],_data[1],_data[2],_data[3]);
	}
	explicit CBBlockData(const std::string &hexdata)
	{
		if (hexdata.size() == 8)
		{
			auto res = std::string(hexdata.size() / 2, 0);

			// Take chars in chunks of 2 and convert to a hex equivalent
			for (uint32_t i = 0; i < (hexdata.size() / 2); i++)
			{
				auto hexpair = hexdata.substr(i * 2, 2);
				res[i] = numeric_cast<char>(std::stol(hexpair, nullptr, 16));
			}

			data = CombineDataBytes(res[0],res[1],res[2],res[3]);
		}
		else if (hexdata.size() == 4)
		{
			data = CombineDataBytes(hexdata[0],hexdata[1],hexdata[2],hexdata[3]);
		}
		else
		{
			// Not a valid length of string passed in..
			LOGERROR("Illegal length passed into CBBlockData constructor - must be 8 or 4");
		}
	}


	explicit CBBlockData(const uint8_t b1, const uint8_t b2, const uint8_t b3, const uint8_t b4)
	{
		data = CombineDataBytes(b1, b2, b3, b4);
	}
	explicit CBBlockData(const char c1, const char c2, const char c3, const char c4)
	{
		data = CombineDataBytes(c1, c2, c3, c4);
	}
	explicit CBBlockData(const uint32_t _data)
	{
		data = _data;
	}
	// Most used constructor
	explicit CBBlockData(const uint16_t AData, const uint16_t BData, bool lastblock = false)
	{
		bool addressblock = false;

		data = 0;
		SetA(AData);
		SetB(BData);
		SetAddressBlock(addressblock);
		// BBit is always zero...
		if (lastblock) MarkAsEndOfMessageBlock();
		SetBCH();
	}
	// Address block constructor
	explicit CBBlockData(const uint8_t StationAddress, const uint8_t Group, const uint8_t FunctionCode, const uint16_t BData, bool lastblock = false)
	{
		data = 0;
		uint16_t AData = ShiftLeftResult16Bits(FunctionCode & 0x0F, 8) | ShiftLeftResult16Bits(StationAddress & 0x0F, 4) | (Group & 0x0F);

		SetA(AData);
		SetB(BData);
		SetAddressBlock(true);
		// BBit is always zero...
		if (lastblock) MarkAsEndOfMessageBlock();
		SetBCH();
	}

	void DoBakerConitelSwap()
	{
		// Swap Station and Group values before sending - the swap is the next two lines...
		uint8_t StationAddress = GetGroup();
		uint8_t Group = GetStationAddress();

		uint8_t FunctionCode = GetFunctionCode();
		uint16_t AData = ShiftLeftResult16Bits(FunctionCode & 0x0F, 8) | ShiftLeftResult16Bits(StationAddress & 0x0F, 4) | (Group & 0x0F);
		SetA(AData);
		SetBCH();
	}
	void MarkAsEndOfMessageBlock()
	{
		data |= 0x01; // Does not change BCH, last bit is EOM bit
	}
	bool IsEndOfMessageBlock() const
	{
		return ((data & 0x001) == 0x001);
	}
	bool IsDataBlock() const
	{
		// Bit A == 0 for address, 1 for data
		return (GetABit());
	}
	bool IsAddressBlock() const
	{
		return !IsDataBlock();
	}
	inline bool CheckBBitIsZero() const
	{
		return ((data >> 6) & 0x1) == 0;
	}
	uint16_t GetA() const
	{
		return numeric_cast<uint16_t>((data >> 20) & 0xFFF);
	}
	void SetA(uint16_t A)
	{
		data = (data & ~ShiftLeftResult32Bits(0xFFF, 20)) | ShiftLeftResult32Bits(A & 0x0FFF,20);
	}
	uint16_t GetB() const
	{
		return numeric_cast<uint16_t>((data >> 7) & 0xFFF);
	}
	void SetB(uint16_t B)
	{
		data = (data & ~ShiftLeftResult32Bits(0xFFF, 7)) | ShiftLeftResult32Bits(B & 0x0FFF, 7);
	}
	uint8_t GetBCH() const
	{
		return numeric_cast<uint8_t>((data >> 1) & 0x1F);
	}
	uint8_t GetFunctionCode() const
	{
		return numeric_cast<uint8_t>((GetA() >> 8) & 0x0f);
	}
	uint8_t GetStationAddress() const
	{
		return numeric_cast<uint8_t>((GetA() >> 4) & 0x0f);
	}
	uint8_t GetGroup() const
	{
		return numeric_cast<uint8_t>(GetA() & 0x0f);
	}
	bool BCHPasses() const
	{
		uint8_t calc = CBBCH(data);
		return CBBCHCompare(calc, GetBCH()); // The compare fn masks...
	}
	// Index 0 to 4 as you would expect
	uint8_t GetByte(int b) const
	{
		assert((b >= 0) && (b < 4));
		uint8_t res = ((data >> (8 * (3 - b))) & 0x0FF);
		return res;
	}
	uint32_t GetData() const
	{
		return data;
	}

	std::string ToBinaryString() const
	{
		std::ostringstream oss;

		oss.put(numeric_cast<char>(data >> 24));
		oss.put(numeric_cast<char>((data >> 16) & 0x0FF));
		oss.put(numeric_cast<char>((data >> 8) & 0x0FF));
		oss.put(numeric_cast<char>(data & 0x0FF));

		return oss.str();
	}
	std::string ToString() const
	{
		std::ostringstream oss;
		oss.fill('0');
		oss << std::hex;
		oss << std::setw(2) << (data >> 24);
		oss << std::setw(2) << ((data >> 16) & 0x0FF);
		oss << std::setw(2) << ((data >> 8) & 0x0FF);
		oss << std::setw(2) << (data & 0x0FF);
		return oss.str();
	}

protected:
	uint32_t CombineDataBytes(const uint8_t b1, const uint8_t b2, const uint8_t b3, const uint8_t b4)
	{
		return ShiftLeftResult32Bits(b1, 24) | ShiftLeftResult32Bits(b2, 16) | ShiftLeftResult32Bits(b3, 8) | numeric_cast<uint32_t>(b4);
	}
	uint32_t CombineDataBytes(const char c1, const char c2, const char c3, const char c4)
	{
		return CombineDataBytes(numeric_cast<uint8_t>(c1), numeric_cast<uint8_t>(c2), numeric_cast<uint8_t>(c3), numeric_cast<uint8_t>(c4));
	}
	inline bool GetABit() const
	{
		return ((data >> 19) & 0x1) == 1;
	}
	inline void SetAddressBlock(bool on)
	{
		if (!on) SetABit(); // Bit A == 0 for Address
	}
	inline void SetABit()
	{
		data |= ShiftLeftResult32Bits(1, 19);
	}
	void SetBCH()
	{
		uint8_t calc = CBBCH(data);
		data = (data &  ~0x3E) | (calc << 1); // Do we need to cast to 32 bit?
	}

	uint32_t data = 0;
};

typedef std::vector<CBBlockData> CBMessage_t;

std::string CBMessageAsString(const CBMessage_t& CompleteCBMessage);

#endif
