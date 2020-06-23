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
#include "CB.h"
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>
#include <iomanip>

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
#define     FUNC_SEND_NEW_SOE                   10 //A
#define     FUNC_REPEAT_SOE                     11 //B
//C
#define     FUNC_UNIT_RAISE_LOWER               13 //D
#define     FUNC_FREEZE_AND_SCAN_ACC            14 //E
#define     FUNC_FREEZE_SCAN_AND_RESET_ACC      15 //F

// Master Station Request Sub-Codes

#define MASTER_SUB_FUNC_0_NOTUSED                                               0
#define MASTER_SUB_FUNC_TESTRAM                                                 1
#define MASTER_SUB_FUNC_TESTPROM                                              2
#define MASTER_SUB_FUNC_TESTEPROM                                             3
#define MASTER_SUB_FUNC_TESTIO                                                      4
#define MASTER_SUB_FUNC_SEND_TIME_UPDATES                               5 // Could be used for other memory areas (OTHER THAN TIME), but none are defined.
#define MASTER_SUB_FUNC_SPARE1                                                      6
#define MASTER_SUB_FUNC_SPARE2                                                      7
#define MASTER_SUB_FUNC_RETRIEVE_REMOTE_STATUS_WORD                     8
#define MASTER_SUB_FUNC_RETREIVE_INPUT_CIRCUIT_DATA                     9
#define MASTER_SUB_FUNC_TIME_CORRECTION_FACTOR_ESTABLISHMENT      10
#define MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION              11
#define MASTER_SUB_FUNC_SPARE3                                                      12
#define MASTER_SUB_FUNC_SET_LOOPBACKS                                         13
#define MASTER_SUB_FUNC_RESET_RTU_WARM                                        14
#define MASTER_SUB_FUNC_RESET_RTU_COLD                                        15

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
int GetBitsSet(uint32_t data, int numberofbits);
int GetSetBit(uint32_t data, int numberofbits);

std::vector<std::string> split(const std::string& s, char delimiter);

std::string GetFunctionCodeName(uint8_t functioncode);
std::string GetSubFunctionCodeName(uint8_t functioncode);


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
std::string to_LOCALtimestringfromCBtime(CBTime _time);
std::string to_stringfromCBtime(CBTime _time);
std::string to_stringfromhhmmssmsec(uint8_t hh, uint8_t mm, uint8_t ss, uint16_t msec);

void to_hhmmssmmfromCBtime(CBTime _time, uint8_t &hh, uint8_t &mm, uint8_t &ss, uint16_t &msec);
void DecodeTimePayload(uint16_t P1B, uint16_t P2A, uint16_t P2B, uint8_t& hhin, uint8_t& mmin, uint8_t& ssin, uint16_t& msecin);
void PackageTimePayload(uint8_t hh, uint8_t mm, uint8_t ss, uint16_t msec, uint16_t& P1B, uint16_t& P2A, uint16_t& P2B);

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

// the following maintains a string version during block assembly so that I can watch it in the debugger. Turn off normally
//#define  DEBUGSTR

class CBBlockData
{
public:
	enum blocktype {AddressBlock, DataBlock};

	// Empty/Zero constructor
	CBBlockData() {}

	// We have received 4 bytes (a block on a stream now we need to decode it) it may not be valid!
	explicit CBBlockData(const CBBlockArray _data)
	{
		data = CombineDataBytes(_data[0],_data[1],_data[2],_data[3]);
		builddataindex = 4;
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
			builddataindex = 4;
		}
		else if (hexdata.size() == 4)
		{
			data = CombineDataBytes(hexdata[0],hexdata[1],hexdata[2],hexdata[3]);
			builddataindex = 4;
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
		builddataindex = 4;
	}
	explicit CBBlockData(const char c1, const char c2, const char c3, const char c4)
	{
		data = CombineDataBytes(c1, c2, c3, c4);
		builddataindex = 4;
	}
	explicit CBBlockData(const uint32_t _data)
	{
		data = _data;
		builddataindex = 4;
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
		builddataindex = 4;
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
		builddataindex = 4;
	}

	void XORBit(uint8_t bit) // Used to corrupt the message...
	{
		if (bit < 32)
			data = data ^ (1 << (31 - bit));
	}

	void DoBakerConitelSwap()
	{
		// Swap Station and Group values before sending - the swap is the next two lines...
		uint8_t StationAddress = GetGroup();
		uint8_t Group = GetStationAddress();

		uint8_t FunctionCode = GetFunctionCode();
		uint16_t AData = ShiftLeftResult16Bits(FunctionCode & 0x0F, 8) | ShiftLeftResult16Bits(StationAddress & 0x0F, 4) | (Group & 0x0F);
		SetA(AData);
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
		SetBCH();
	}
	uint16_t GetB() const
	{
		return numeric_cast<uint16_t>((data >> 7) & 0xFFF);
	}
	void SetB(uint16_t B)
	{
		data = (data & ~ShiftLeftResult32Bits(0xFFF, 7)) | ShiftLeftResult32Bits(B & 0x0FFF, 7);
		SetBCH();
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


	// Used to build a 4 byte block from the TCP stream so we can test if it is a valid CB Block. If the block is already full, push out one byte to make room for a new one.
	void AddByteToBlock(uint8_t b)
	{
		if (builddataindex < 4)
		{
			// We do not have a full block yet, so add
			int shiftcount = 8 * (3 - builddataindex++);
			data |= ShiftLeftResult32Bits(b,shiftcount);
		}
		else
		{
			// We have a full block, so push out one byte then add the new one.
			data = ShiftLeftResult32Bits(data, 8);
			data |= ShiftLeftResult32Bits(b, 0); // No shift but convert to 32 bit.
		}
		#ifdef DEBUGSTR
		dataasstr = ToString();
		#endif
	}
	bool IsValidBlock()
	{
		// Test everything we can to verify this is a CB block of data.
		return CheckBBitIsZero() && BCHPasses() && (builddataindex == 4);
	}
	void Clear()
	{
		data = 0;
		builddataindex = 0;
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
		data = (data &  numeric_cast<uint32_t>(~0x3E)) | numeric_cast<uint32_t>(calc << 1);
	}

	uint32_t data = 0;
	uint32_t builddataindex = 0; // Used in TCP framingblock building only.
	#ifdef DEBUGSTR
	std::string dataasstr; // Used in TCP framingblock building only.
	#endif
};

typedef std::vector<CBBlockData> CBMessage_t;

// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
std::string BuildBinaryStringFromASCIIHexString(const std::string &as);

// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a binary string, and produces an ascii hex string in the format of "FF120D567200"
std::string BuildASCIIHexStringfromBinaryString(const std::string &bs);

std::string BuildASCIIHexStringfromCBMessage(const CBMessage_t & CBMessage);
std::string CBMessageAsString(const CBMessage_t& CompleteCBMessage);

// SOE Data Packet Definitions.
const uint8_t SOELongBitLength = 44;
const uint8_t SOEShortBitLength = 33;

inline bool TestBit(const uint64_t &data, const uint8_t bitindex)
{
	uint64_t testbit = uint64_t(1) << bitindex;
	return ((data & testbit) == testbit);
}

// The maximum number of bits we can send is 12 * 31 = 372.
const uint32_t MaxSOEBits = 12 * 31;

class SOEEventFormat // Use the Bits uint64_t to get at the resulting packed bits. - bit order???
{
public:
	// 1 - 3 bits group #, 7 bits point number, Status(value) bit, Quality Bit(unused), Time Bit - changes what comes next
	// T==1 Time - 27 bits, Hour (0-23, 5 bits), Minute (0-59, 6 bits), Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// T==0 Hours and Minutes same as previous event, 16 bits - Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// Last bit L, when set indicates last record.
	// So the data can be 13+27+1 bits = 41 bits, or 13+16+1 = 30 bits.
	SOEEventFormat() {};

	// Use the bitarray to construct an event, return the start of the next event in the bitarray.
	// The bitarray data may give us a short timed event (only sec and msec received) in this case add the passed in LastEventTime to the seconds/msec value
	// And change the TimeFormatBit to indicate that the Hours/Minutes are valid.
	SOEEventFormat(std::array<bool, MaxSOEBits> BitArray, uint32_t startbit, uint32_t usedbits, uint32_t &newstartbit, CBTime LastEventTime, bool &success)
	{
		success = false;

		// Check there is enough data for at least the short version of the packet.
		if ((startbit + 30) > usedbits)
		{
			LOGDEBUG("SOEEventFormat constructor failed, only {} bits of data available", usedbits - startbit);
			return;
		}

		try
		{
			Group = GetBits8(BitArray, startbit, 3); // Bottom 3 bits of SCAN group - significant? Different to SOE Group?
			startbit += 3;
			Number = GetBits8(BitArray, startbit, 7);
			startbit += 7;

			ValueBit = BitArray[startbit++];
			QualityBit = BitArray[startbit++];
			TimeFormatBit = BitArray[startbit++];

			if (TimeFormatBit) // Long format
			{
				// Check there is enough data for the long version of the packet.
				if ((startbit + 41) > usedbits)
				{
					LOGDEBUG("SOEEventFormat constructor failed in long version, only {} bits of data available", usedbits - startbit);
					return;
				}
				Hour = GetBits8(BitArray, startbit, 5);
				startbit += 5;
				Minute = GetBits8(BitArray, startbit, 6);
				startbit += 6;
			}
			Second = GetBits8(BitArray, startbit, 6);
			startbit += 6;
			Millisecond = GetBits16(BitArray, startbit, 10);
			startbit += 10;

			if (!TimeFormatBit) // Short Format, so add the LastEventTime to the delta we have (seconds+mseconds)
			{
				bool FirstEvent = (LastEventTime == 0); // Can only be zero for our first event...
				// Add the LastEventTime to the Second/Millisecond values
				LastEventTime += (Millisecond + Second * 1000);
				SetTimeFields(LastEventTime, FirstEvent);
			}
			LastEventFlag = BitArray[startbit++];

			newstartbit = startbit;
			success = true;
		}
		catch (const std::exception& e)
		{
			LOGERROR("SOEEventFormat constructor failed, probably array index exceeded. {}", e.what());
		}
	}

	uint8_t Group = 0;  // 3 bits - 0 - 7
	uint8_t Number = 0; // 7 bits - 0 - 120
	bool ValueBit = false;
	bool QualityBit = false;
	bool TimeFormatBit = true; // True ,1 Long Format
	// (13 bits)

	uint8_t Hour = 0;         // 5 bits, 0-23
	uint8_t Minute = 0;       // 6 bits, 0-59
	uint8_t Second = 0;       // 6 bits, 0-59
	uint16_t Millisecond = 0; // 10 bits,  0-999
	// (27 or 16 bits for the time section)

	bool LastEventFlag = false; // Set when no more events are available
	// 41 or 30 bits

	void SetTimeFields(CBTime TimeDelta, bool FirstEvent)
	{
		if ((TimeDelta > 1000 * 60) || FirstEvent)
		{
			// Set full time, with TimeFormatBit == true
			TimeFormatBit = true;

			CBTime Days = TimeDelta / 1000 / 60 / 60 / 24; // Days is not used...handle just in case
			CBTime LongHour = TimeDelta / 1000 / 60 / 60 % 24;
			CBTime Remainder = TimeDelta - (1000 * 60 * 60 * LongHour) - (1000 * 60 * 60 * 24 * Days);

			Hour = numeric_cast<uint8_t>(LongHour);
			Minute = Remainder / 1000 / 60 % 60;
			Second = Remainder / 1000 % 60;
			Millisecond = Remainder % 1000;
		}
		else
		{
			// Set delta time, with TimeFormatBit == false
			TimeFormatBit = false;

			Second = TimeDelta / 1000 % 60;
			Millisecond = TimeDelta % 1000;
		}
	}
	CBTime GetTotalMsecTime()
	{
		return (((numeric_cast<CBTime>(Hour) * 60ul + numeric_cast<CBTime>(Minute)) * 60ul + numeric_cast<CBTime>(Second)) * 1000ul + numeric_cast<CBTime>(Millisecond));
	}

	uint8_t GetBits8(std::array<bool, MaxSOEBits> BitArray, uint32_t startbit, uint32_t numberofbits)
	{
		assert(numberofbits <= 8);
		assert(numberofbits != 0);

		uint8_t res = 0;
		for (uint32_t i = startbit; i < (startbit+numberofbits); i++ )
		{
			res <<= 1; // Shift left by one bit

			if (BitArray[i]) // Do we set this bit?
				res |= 0x01;
		}
		return res;
	}
	uint16_t GetBits16(std::array<bool, MaxSOEBits> BitArray, uint32_t startbit, uint32_t numberofbits)
	{
		assert(numberofbits <= 16);
		assert(numberofbits != 0);

		uint16_t res = 0;
		for (uint32_t i = startbit;  i < (startbit + numberofbits); i++)
		{
			res <<= 1; // Shift left by one bit

			if (BitArray[i]) // Do we set this bit?
				res |= 0x01;
		}
		return res;
	}
	uint64_t GetFormattedData()
	{
		// We will format this from the Most Significant Bit down, so the lower bits are not used.
		uint32_t res1 = ShiftLeftResult32Bits(Group, 32 - 3) | ShiftLeftResult32Bits(Number, 32 - 7 - 3) | ShiftLeftResult32Bits(ValueBit, 32 - 7 - 3 - 1) |
		                ShiftLeftResult32Bits(QualityBit, 32 - 7 - 3 - 1 - 1) | ShiftLeftResult32Bits(TimeFormatBit, 32 - 7 - 3 - 1 - 1 - 1);

		uint64_t res = numeric_cast<uint64_t>(res1) << 32;

		uint32_t res2 = 0;
		if (TimeFormatBit)
		{
			res2 = ShiftLeftResult32Bits(Hour, 32 - 5) | ShiftLeftResult32Bits(Minute, 32 - 5 - 6) | ShiftLeftResult32Bits(Second, 32 - 5 - 6 - 6)
			       | ShiftLeftResult32Bits(Millisecond, 32 - 5 - 6 - 6 - 10) | ShiftLeftResult32Bits(LastEventFlag, 32 - 5 - 6 - 6 - 10 - 1);
		}
		else
		{
			res2 = ShiftLeftResult32Bits(Second, 32 - 6) | ShiftLeftResult32Bits(Millisecond, 32 - 6 - 10) | ShiftLeftResult32Bits(LastEventFlag, 32 - 6 - 10 - 1);
		}
		res |= numeric_cast<uint64_t>(res2) << (32 - 7 - 3 - 1 - 1 - 1);

		return res;
	}
	uint8_t GetResultBitLength()
	{
		if (TimeFormatBit)
		{
			return 41;
		}
		else
		{
			return 30;
		}
	}
	bool AddDataToBitArray(std::array<bool, MaxSOEBits> &BitArray, uint32_t &UsedBits)
	{
		uint8_t numberofbits = GetResultBitLength();

		// Do we have room in the bit vector to add the data?
		if (UsedBits + numberofbits >= MaxSOEBits)
			return false;

		// We can fit this data - proceed
		uint64_t bitdata = GetFormattedData();
		LOGDEBUG("---PackedEventData {}", to_hexstring(bitdata));

		for (uint8_t i = 0; i < numberofbits; i++) // 41 or 30 depending on TimeFormatBit
		{
			BitArray[UsedBits++] = TestBit(bitdata, 63 - i);
		}
		return true;
	}
};

#endif
