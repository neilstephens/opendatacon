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
* MD3Utility.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3ENGINE_H_
#define MD3ENGINE_H_
#include "MD3.h"
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>


const int MD3BlockArraySize = 6;
typedef  std::array<uint8_t, MD3BlockArraySize> MD3BlockArray;

uint8_t MD3CRC(const uint32_t data);
bool MD3CRCCompare(const uint8_t crc1, const uint8_t crc2);

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

template <class T>
std::string to_hexstring(T val)
{
	return fmt::format("{:#06x}",val);
	//std::stringstream sstream;
	//sstream << std::hex << val;
	//return sstream.str();
}
template <class T>
std::string to_binstring(T val)
{
	std::stringstream sstream;
	sstream << std::setbase(2) << val;
	return sstream.str();
}
// Create an ASCII string version of the time from the MD3 time - which is msec since epoch.
std::string to_LOCALtimestringfromMD3time(MD3Time _time);
std::string to_stringfromMD3time(MD3Time _time);
// Return the current UTC offset in minutes.
int tz_offset();

// Every block in MD3 is 5 bytes, plus one zero byte. If we don't have 5 bytes, we don't have a block. The last block is marked as such.
// We will create a class to load the 6 byte array into, then we can just ask it to return the information in the variety of ways we require,
// depending on the block content.
// There are two ways this class is used - one for decoding, one for encoding.
// Create child classes for each of the functions where the data block has specific layout and meaning.
class MD3BlockData
{
public:
	enum blocktype {FormattedBlock, DataBlock};

	MD3BlockData() {}

	// We have received 6 bytes (a block on a stream now we need to decode it) it may not be valid!
	explicit MD3BlockData(const MD3BlockArray _data)
	{
		data = CombineDataBytes(_data[0],_data[1],_data[2],_data[3]);
		endbyte = _data[4];
		builddataindex = 6;
	}
	explicit MD3BlockData(const std::string &hexdata)
	{
		if (hexdata.size() == 12)
		{
			auto res = std::string(hexdata.size() / 2, 0);

			// Take chars in chunks of 2 and convert to a hex equivalent
			for (uint32_t i = 0; i < (hexdata.size() / 2); i++)
			{
				auto hexpair = hexdata.substr(i * 2, 2);
				res[i] = numeric_cast<char>(std::stol(hexpair, nullptr, 16));
			}

			data = CombineDataBytes(res[0],res[1],res[2],res[3]);
			endbyte = numeric_cast<uint8_t>(res[4]);
			assert(res[5] == 0x00); // Sixth byte should always be zero.
			builddataindex = 6;

		}
		else if (hexdata.size() == 6)
		{
			data = CombineDataBytes(hexdata[0],hexdata[1],hexdata[2],hexdata[3]);
			endbyte = numeric_cast<uint8_t>(hexdata[4]);
			assert(hexdata[5] == 0x00); // Sixth byte should always be zero.
			builddataindex = 6;

		}
		else
		{
			// Not a valid length of string passed in..
			LOGERROR("Illegal length passed into MD3BlockData constructor - must be 12 or 6");
		}
	}
	explicit MD3BlockData(const uint16_t firstword, const uint16_t secondword, bool lastblock = false)
	{
		data = static_cast<uint32_t>(firstword) << 16 | static_cast<uint32_t>(secondword);
		SetEndByte(DataBlock, lastblock);
		builddataindex = 6;
	}

	explicit MD3BlockData(const uint8_t b1, const uint8_t b2, const uint8_t b3, const uint8_t b4, bool lastblock = false)
	{
		data = CombineDataBytes(b1, b2, b3, b4);
		SetEndByte(DataBlock, lastblock);
	}
	explicit MD3BlockData(const char c1, const char c2, const char c3, const char c4, bool lastblock = false)
	{
		data = CombineDataBytes(c1, c2, c3, c4);
		SetEndByte(DataBlock, lastblock);
	}
	explicit MD3BlockData(const uint32_t _data, bool lastblock = false)
	{
		data = _data;
		SetEndByte(DataBlock, lastblock);
	}

	static uint32_t CombineDataBytes(const uint8_t b1, const uint8_t b2, const uint8_t b3, const uint8_t b4)
	{
		return ShiftLeftResult32Bits(b1, 24) | ShiftLeftResult32Bits(b2, 16) | ShiftLeftResult32Bits(b3, 8) | numeric_cast<uint32_t>(b4);
	}
	static uint32_t CombineDataBytes(const char c1, const char c2, const char c3, const char c4)
	{
		return CombineDataBytes(numeric_cast<uint8_t>(c1), numeric_cast<uint8_t>(c2), numeric_cast<uint8_t>(c3), numeric_cast<uint8_t>(c4));
	}
	void SetEndByte(blocktype blockt, bool lastblock)
	{
		endbyte = MD3CRC(data); // Max 6 bits returned

		if (blockt == DataBlock)
			endbyte |= FOMBIT; // NOT a formatted block, must be 1

		endbyte |= lastblock ? EOMBIT : 0x00;
		builddataindex = 6; // Just to mark it as full. Only used for rx'd block assembly.
	}
	void XORBit(uint8_t bit) // Used to corrupt the message...
	{
		if (bit < 32)
			data = data ^ (1 << (31 - bit));
		else if (bit < 40)
			endbyte = endbyte ^ (1 << (39 - bit));
		//else ignore!
	}
	bool IsEndOfMessageBlock() const
	{
		return ((endbyte & EOMBIT) == EOMBIT);
	}
	void MarkAsEndOfMessageBlock()
	{
		endbyte |= EOMBIT; // Does not change CRC
	}
	bool IsFormattedBlock() const
	{
		return !((endbyte & FOMBIT) == FOMBIT); // 0 is Formatted, 1 is Unformatted
	}

	bool CheckSumPasses() const
	{
		uint8_t calc = MD3CRC(data);
		return MD3CRCCompare(calc, endbyte);
	}
	// Index 0 to 3, 0 is MSB not LSB
	uint8_t GetByte(int b) const
	{
		assert((b >= 0) && (b < 4));
		uint8_t res = ((data >> (8 * (3 - b))) & 0x0FF);
		return res;
	}
	uint16_t GetFirstWord() const
	{
		return (data >> 16);
	}
	uint16_t GetSecondWord() const
	{
		return (data & 0xFFFF);
	}
	// The extended address would be retrieved from the correct block with this call.
	uint32_t GetData() const
	{
		return data;
	}
	uint8_t GetEndByte() const
	{
		return endbyte;
	}
	std::string ToBinaryString() const
	{
		std::ostringstream oss;

		oss.put(numeric_cast<char>(data >> 24));
		oss.put(numeric_cast<char>((data >> 16) & 0x0FF));
		oss.put(numeric_cast<char>((data >> 8) & 0x0FF));
		oss.put(numeric_cast<char>(data & 0x0FF));
		oss.put(numeric_cast<char>(endbyte));
		oss.put(0x00);

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
		oss << std::setw(2) << static_cast<uint32_t>(endbyte);
		oss << "00";
		return oss.str();
	}

	// Used to build a 6 byte block from the TCP stream so we can test if it is a valid MD3B Block. If the block is already full, push out one byte to make room for a new one.
	void AddByteToBlock(uint8_t b)
	{
		if (builddataindex < 4)
		{
			// We do not have a full block yet, so add
			int shiftcount = 8 * (3 - builddataindex++);
			data |= ShiftLeftResult32Bits(b, shiftcount);
		}
		else if (builddataindex == 4)
		{
			// The CRC/EOM data
			endbyte = b;
			builddataindex++;
		}
		else if (builddataindex == 5)
		{
			// The Paddign Byte
			paddingbyte = b;
			builddataindex++;
		}
		else if (builddataindex > 5)
		{
			// We have a full block, so push out one byte then add the new one. Have to shift things in the 3 storage locations..bit cumbersome
			data = ShiftLeftResult32Bits(data, 8);
			data |= ShiftLeftResult32Bits(endbyte, 0); // No shift but convert to 32 bit.
			endbyte = paddingbyte;
			paddingbyte = b;
		}
	}
	bool IsValidBlock()
	{
		// Test everything we can to verify this is a CB block of data.
		return CheckSumPasses() && (builddataindex == 6) && (paddingbyte == 0);
	}
	void Clear()
	{
		data = 0;
		endbyte = 0;
		paddingbyte = 0;
		builddataindex = 0;
	}

protected:
	uint32_t data = 0;
	uint8_t endbyte = 0;
	uint8_t paddingbyte = 0;     // Should always be 0!
	uint32_t builddataindex = 0; // Used in TCP framingblock building only.
};

typedef std::vector<MD3BlockData> MD3Message_t;
std::string MD3MessageAsString(const MD3Message_t& CompleteMD3Message);

class MD3BlockFormatted: public MD3BlockData
{
public:
	MD3BlockFormatted() {}

	explicit MD3BlockFormatted(const MD3BlockData &parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFormatted(uint32_t _data, bool lastblock = false)
	{
		data = _data;
		SetEndByte(FormattedBlock, lastblock);
	}
	// Create a formatted block including checksum
	// Note if the station address is set to 0x7F , then the next data block contains the address.
	MD3BlockFormatted(uint8_t StationAddress, bool mastertostation, MD3_FUNCTION_CODE functioncode, uint8_t moduleaddress, uint8_t channels, bool lastblock = false,
		bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// Channels -> 0 on the wire == 1 channel, 15 == 16
		channels--;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert((channels & 0x0F) == channels);             // Max 4 bits;

		uint8_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = CombineFormattedBytes( mastertostation, StationAddress,functioncode,moduleaddress, flags | channels);

		SetEndByte(FormattedBlock, lastblock);
	}

	static uint32_t CombineFormattedBytes(bool mastertostation, uint8_t StationAddress, uint8_t functioncode, uint8_t moduleaddress, uint8_t b4)
	{
		uint8_t direction = mastertostation ? 0x00 : 0x80;
		return CombineDataBytes(direction | (StationAddress & 0x7f), functioncode, moduleaddress, b4);
	}

	// Apply to formatted blocks only!
	bool IsMasterToStationMessage() const
	{
		return !((data & DIRECTIONBIT) == DIRECTIONBIT);
	}
	uint8_t GetStationAddress() const
	{
		//NOTE: Extended Addressing - Address 0x7F indicates an extended address - not supported - flagged elsewhere
		return (data >> 24) & 0x7F;
	}
	uint8_t GetModuleAddress() const
	{
		return (data >> 8) & 0xFF;
	}
	uint8_t GetFunctionCode() const
	{
		// How do we check it is a valid function code first?
		// There are a few complex ways to check the values of an enum - mostly by creating a new class.
		// As we are likely to have some kind of function dispatcher class anyway - dont check here.
		return (data >> 16) & 0xFF;
	}
	uint8_t GetChannels() const
	{
		// 0 on the wire is 1 channel
		return (data & 0x0F)+1;
	}

	// Set the RSF flag. This comes from a global flag we maintain, and we set just before we send.
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		uint32_t flags = 0;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;
		data &= numeric_cast<uint32_t>(~(RSFBIT|HRPBIT|DCPBIT)); // Clear the bits we are going to set.
		data |= flags;

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
	bool GetAPL() const
	{
		return ((data & APLBIT) == APLBIT);
	}
	bool GetRSF() const
	{
		return ((data & RSFBIT) == RSFBIT);
	}
	bool GetHRP() const
	{
		return ((data & HRPBIT) == HRPBIT);
	}
	bool GetDCP() const
	{
		return ((data & DCPBIT) == DCPBIT);
	}
};

// The datablocks can take on different formats where the bytes and buts in the block change meaning.
// Use child classes to separate out this on a function by function basis
class MD3BlockFn9: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn9(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn9(uint8_t StationAddress, bool mastertostation, uint8_t sequencenumber, uint8_t maximumevents, bool moreevents, bool lastblock = false)
	{
		uint8_t mevbit = moreevents ? 0x08 : 0;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert((sequencenumber & 0x0F) == sequencenumber); // Max 4 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, HRER_LIST_SCAN, numeric_cast<uint8_t>((sequencenumber & 0x0F) << 4 | mevbit), maximumevents);

		SetEndByte(FormattedBlock, lastblock);
	}
	void SetEventCountandMoreEventsFlag(uint8_t events, bool moreevents )
	{
		data &= numeric_cast<uint32_t>(~(MOREEVENTSBIT | 0x0FF)); // Clear the bits we are going to set.

		uint32_t mevbit = moreevents ? MOREEVENTSBIT : 0;
		data |=  mevbit | events;

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
	bool GetMoreEventsFlag() const
	{
		return (data & MOREEVENTSBIT) == MOREEVENTSBIT;
	}
	uint8_t GetSequenceNumber() const
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 12) & 0x0F);
	}
	uint8_t GetEventCount() const
	{
		return (data & 0x0FF);
	}
	static uint16_t MilliSecondsPacket( uint16_t allmsec)
	{
		assert(allmsec < 31999);
		uint16_t deltasec = allmsec / 1000;
		uint16_t msec = allmsec % 1000;

		return numeric_cast<uint16_t>(0x8000 | (deltasec & 0x1F) << 10 | (msec & 0x00FF));
	}
	static uint16_t HREREventPacket(uint8_t bitstate, uint8_t channel, uint8_t moduleaddress)
	{
		return ShiftLeftResult16Bits(bitstate,14) | ShiftLeftResult16Bits(channel,8) | ShiftLeftResult16Bits(moduleaddress,0);
	}
	static uint16_t FillerPacket()
	{
		return 0;
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn10: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn10(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn10(uint8_t StationAddress, bool mastertostation, uint8_t moduleaddress, uint8_t modulecount, bool lastblock, bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert((modulecount & 0x0F) == modulecount);       // Max 4 bits;

		uint8_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_CHANGE_OF_STATE, moduleaddress, flags | (modulecount & 0x0F));

		SetEndByte(FormattedBlock, lastblock);
	}

	uint8_t GetModuleCount() const
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetModuleCount(uint8_t modulecount)
	{
		data &= numeric_cast<uint32_t>(~(0x0F)); // Clear the bits we are going to set.
		data |= modulecount;

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
};

class MD3BlockFn11MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn11MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn11MtoS(uint8_t StationAddress, uint8_t taggedeventcount, uint8_t digitalsequencenumber, uint8_t modulecount)
	{
		bool mastertostation = true;
		bool lastblock = true;

		assert((StationAddress & 0x7F) == StationAddress);               // Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);           // Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber); // Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);                     // Max 4 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_CHANGE_OF_STATE_TIME_TAGGED, numeric_cast<uint8_t>(taggedeventcount << 4), numeric_cast<uint8_t>(digitalsequencenumber << 4 | modulecount));

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetTaggedEventCount() const
	{
		// Can be a zero event count - in the spec 1's numbered. (0's numbered is when 0 is equivalent to 1 - 0 is not valid)
		return (data >> 12) & 0x0F;
	}
	uint8_t GetDigitalSequenceNumber() const
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 4) & 0x0F);
	}
	uint8_t GetModuleCount() const
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn11StoM: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn11StoM(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn11StoM(uint8_t StationAddress, uint8_t taggedeventcount, uint8_t digitalsequencenumber, uint8_t modulecount,
		bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		bool mastertostation = false;
		bool lastblock = false; // There will always be following data, otherwise we send an Digital No Change response

		assert((StationAddress & 0x7F) == StationAddress);               // Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);           // Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber); // Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);                     // Max 4 bits;

		uint8_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_CHANGE_OF_STATE_TIME_TAGGED, numeric_cast<uint8_t>(taggedeventcount << 4 | digitalsequencenumber), flags | modulecount);

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetTaggedEventCount() const
	{
		// Can be a zero event count - in the spec 1's numbered. (0's numbered is when 0 is equivalent to 1 - 0 is not valid)
		return (data >> 12) & 0x0F;
	}
	void SetTaggedEventCount(uint8_t eventcount)
	{
		data &= numeric_cast<uint32_t>(~(0x0F << 12)); // Clear the bits we are going to set.
		data |= ShiftLeftResult32Bits(eventcount,12);

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
	uint8_t GetDigitalSequenceNumber() const
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 8) & 0x0F);
	}
	uint8_t GetModuleCount() const
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetModuleCount(uint8_t modulecount)
	{
		data &= numeric_cast<uint32_t>(~(0x0F)); // Clear the bits we are going to set.
		data |= modulecount;

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
	static uint16_t FillerPacket()
	{
		// Will be at the end and have no effect on anything. The RTU's have this as zero
		return 0x0000;
	}
	static uint16_t MilliSecondsDiv256OffsetPacket(uint16_t allmsec)
	{
		// There is a msec in the data packet, this is to allow us extra offset - should be divisible by 256 for accuracy
		return allmsec/256;
	}
};
// The reply to Fn12 is actually a Fn11 packet
class MD3BlockFn12MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn12MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn12MtoS(uint8_t StationAddress, uint8_t startmoduleaddress, uint8_t digitalsequencenumber, uint8_t modulecount)
	{
		bool mastertostation = true;
		bool lastblock = true;

		assert((StationAddress & 0x7F) == StationAddress);               // Max of 7 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber); // Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);                     // Max 4 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_UNCONDITIONAL, startmoduleaddress, numeric_cast<uint8_t>(digitalsequencenumber << 4 | modulecount));

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetStartingModuleAddress() const
	{
		return (data >> 8) & 0xFF;
	}
	uint8_t GetDigitalSequenceNumber() const
	{
		return ((data >> 4) & 0x0F);
	}
	uint8_t GetModuleCount() const
	{
		// 1's numbered
		return (data & 0x0F);
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};

class MD3BlockFn14StoM: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn14StoM(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn14StoM(uint8_t StationAddress, uint8_t moduleaddress, uint8_t modulecount, bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		bool lastblock = true;
		bool mastertostation = false;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert((modulecount & 0x0F) == modulecount);       // Max 4 bits;

		uint8_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_NO_CHANGE_REPLY, moduleaddress, flags | modulecount);

		SetEndByte(FormattedBlock, lastblock);
	}
	MD3BlockFn14StoM(uint8_t StationAddress,  uint8_t digitalsequencenumber, bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		bool lastblock = true;
		bool mastertostation = false;
		uint8_t taggedeventcount = 0; // Not returning anything..
		uint8_t modulecount = 0;      // Not returning anything..

		assert((StationAddress & 0x7F) == StationAddress);               // Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);           // Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber); // Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);                     // Max 4 bits;

		uint8_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = CombineFormattedBytes(mastertostation, StationAddress, DIGITAL_NO_CHANGE_REPLY, numeric_cast<uint8_t>(taggedeventcount << 4 | digitalsequencenumber), flags | modulecount);

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetDigitalSequenceNumber() const
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 8) & 0x0F);
	}
	uint8_t GetModuleCount() const
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
};

class MD3BlockFn15StoM: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn15StoM(const MD3BlockData& parent)
	{
		// This Block is a copy of the originating block header data, with the function code changed.
		// We change the function code, change the direction bit, mark as last block and recalc the checksum.
		data = parent.GetData();

		bool lastblock = true; // Only this block will be returned.
		bool mastertostation = false;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		data &= ~(ShiftLeftResult32Bits(0x0FF, 16) | DIRECTIONBIT);        // Clear the bits we are going to set.
		data |= ShiftLeftResult32Bits(CONTROL_REQUEST_OK, 16) | direction; // Set the function code and direction

		// Regenerate the last byte
		SetEndByte(FormattedBlock, lastblock);
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		assert(false);
		// No flags in this formatted packet
	}
};

// The freeze (and possibly reset) counters command
class MD3BlockFn16MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn16MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn16MtoS(uint8_t StationAddress, bool NoCounterReset)
	{
		bool lastblock = true;
		bool mastertostation = true;

		uint8_t nocounterresetbit = NoCounterReset ? 0x01 : 0x0000;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, FREEZE_AND_RESET, nocounterresetbit, ~StationAddress & 0x7F);

		SetEndByte(FormattedBlock, lastblock);
	}

	bool GetNoCounterReset() const
	{
		return ((data & NOCOUNTERRESETBIT) == NOCOUNTERRESETBIT);
	}

	// Check the Station address against its complement
	bool IsValid() const
	{
		// Check that the complements and the originals match
		if (GetStationAddress() != (~GetByte(3) & 0x7F)) // Is the station address correct?
			return false;

		return CheckSumPasses();
	}
};
// POM Control
class MD3BlockFn17MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn17MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn17MtoS(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t OutputSelection)
	{
		bool lastblock = false;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, POM_TYPE_CONTROL, ModuleAddress, OutputSelection);

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetOutputSelection() const
	{
		return data & 0x0FF;
	}
	// The second block in the message only contains a different format of the information in the first
	MD3BlockData GenerateSecondBlock() const
	{
		uint16_t lowword = ShiftLeftResult16Bits(1, (15 - GetOutputSelection()));
		uint32_t direction = GetData() & DIRECTIONBIT;
		uint32_t seconddata = direction | ShiftLeftResult32Bits(~GetStationAddress() & 0x07f, 24) | ShiftLeftResult32Bits(~GetModuleAddress() & 0x0FF, 16) | numeric_cast<uint32_t>(lowword);

		MD3BlockData sb(seconddata, true);
		return sb;
	}
	// Check the second block against the first (this one) to see if we have a valid POM command
	bool VerifyAgainstSecondBlock(MD3BlockData &SecondBlock) const
	{
		// Check that the complements and the originals match
		if (GetStationAddress() != ((~SecondBlock.GetData() >> 24) & 0x7F)) // Is the station address correct?
			return false;

		if ((GetModuleAddress() & 0x0ff) != ((~SecondBlock.GetData() >> 16) & 0x0FF)) // Is the module address correct?
			return false;

		uint16_t lowword = ShiftLeftResult16Bits(1, (15 - GetOutputSelection()));
		if (lowword != SecondBlock.GetSecondWord())
			return false;

		return true;
	}
};
// DOM Control
class MD3BlockFn19MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn19MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn19MtoS(uint8_t StationAddress, uint8_t ModuleAddress)
	{
		bool lastblock = false;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, DOM_TYPE_CONTROL, ModuleAddress, ~ModuleAddress);

		SetEndByte(FormattedBlock, lastblock);
	}

	// The second block in the message only contains a different format of the information in the first
	MD3BlockData GenerateSecondBlock(uint16_t OutputData) const
	{
		uint32_t checkdata = (OutputData & 0x0FF) + ((OutputData >> 8) & 0x0FF);
		uint32_t seconddata = ShiftLeftResult32Bits(OutputData, 16) | ShiftLeftResult32Bits(checkdata , 8) | numeric_cast<uint32_t>(~GetStationAddress() & 0x07f);
		MD3BlockData sb(seconddata, true);
		return sb;
	}
	// Possibly should have second block class for this function...
	uint16_t GetOutputFromSecondBlock(MD3BlockData &SecondBlock) const
	{
		return SecondBlock.GetFirstWord();
	}

	// Check the second block against the first (this one) to see if we have a valid POM command
	bool VerifyAgainstSecondBlock(MD3BlockData &SecondBlock) const
	{
		// Check that the complements and the originals match
		if (GetStationAddress() != (~SecondBlock.GetByte(3) & 0x7F)) // Is the station address correct?
			return false;

		if ((GetModuleAddress() & 0x0ff) != (~GetByte(3) & 0x0FF)) // Is the module address correct?
			return false;

		uint8_t checkdata = (SecondBlock.GetFirstWord() & 0x0FF) + ((SecondBlock.GetFirstWord() >> 8) & 0x0FF);
		if (checkdata != SecondBlock.GetByte(2))
			return false;

		return true;
	}
};
// Input point Control
class MD3BlockFn20MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn20MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn20MtoS(uint8_t StationAddress, uint8_t ModuleAddress,  uint8_t ChannelSelection, uint8_t ControlSelection)
	{
		bool lastblock = false;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert((ControlSelection & 0x0F) == ControlSelection);
		assert((ChannelSelection & 0x0F) == ChannelSelection);

		data = CombineFormattedBytes(mastertostation, StationAddress, INPUT_POINT_CONTROL, ModuleAddress, (((ControlSelection << 4) & 0xF0) | (ChannelSelection & 0x0f)));

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetChannel() const
	{
		return (data & 0x0FF);
	}
	uint8_t GetModuleAddress() const
	{
		return (data >> 8) & 0x0FF;
	}
	DIMControlSelectionType GetControlSelection() const
	{
		return static_cast<DIMControlSelectionType>((data >> 4) & 0x0F);
	}
	uint8_t GetChannelSelection() const
	{
		return data & 0x0F;
	}
	// The second block in the message only contains a different format of the information in the first
	MD3BlockData GenerateSecondBlock(bool IsLastPacket) const
	{
		bool mastertostation = true;
		uint8_t SelectionByte = GetControlSelection() << 4 | GetChannelSelection();
		uint32_t seconddata = CombineFormattedBytes(mastertostation, ~GetStationAddress(), ~GetModuleAddress(), uint8_t(~SelectionByte),SelectionByte); // The last byte here is indicated as reserved, but this value seems to be ok.

		MD3BlockData sb(seconddata, IsLastPacket);
		return sb;
	}
	// The Third block is optional depending on what you are planning to do.
	MD3BlockData GenerateThirdBlock(uint16_t OutputData) const
	{
		uint32_t thirddata = ShiftLeftResult32Bits(OutputData & 0x0FFF, 16) | (~OutputData & 0x0FFF);

		MD3BlockData sb(thirddata, true); // Last Block
		return sb;
	}

	// Checks that the value and its complement match - so we can use the value. If not there has been a problem.
	bool VerifyThirdBlock(MD3BlockData &ThirdBlock) const
	{
		return (ThirdBlock.GetFirstWord() & 0x0FFF) == (~ThirdBlock.GetSecondWord() & 0x0FFF);
	}
	uint16_t GetOutputFromThirdBlock(MD3BlockData &ThirdBlock) const
	{
		return ThirdBlock.GetFirstWord() & 0x0FFF;
	}

	// Check the second block against the first (this one) to see if we have a valid command
	bool VerifyAgainstSecondBlock(MD3BlockData &SecondBlock) const
	{
		// Check that the complements and the originals match

		if (GetStationAddress() != (~SecondBlock.GetByte(0) & 0x07F)) // Is the station address correct?
			return false;

		if (GetModuleAddress() != (~SecondBlock.GetByte(1) & 0x0FF)) // Is the module address correct?
			return false;

		if ((GetChannelSelection() & 0x0f) != (~SecondBlock.GetByte(2) & 0x0F)) // Is the channel selection correct?
			return false;

		if ((GetControlSelection() & 0x0f) != (~(SecondBlock.GetByte(2) >> 4) & 0x0F)) // Is the control selection correct?
			return false;
		// Last byte we dont care - it is reserved.

		return true;
	}
};
// AOM Control
class MD3BlockFn23MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn23MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn23MtoS(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channel)
	{
		bool lastblock = false;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, AOM_TYPE_CONTROL, ModuleAddress, Channel);

		SetEndByte(FormattedBlock, lastblock);
	}
	uint8_t GetChannel() const
	{
		return (data & 0x0FF);
	}
	uint8_t GetModuleAddress() const
	{
		return (data >> 8) & 0x0FF;
	}
	// The second block in the message only contains a different format of the information in the first
	MD3BlockData GenerateSecondBlock(uint16_t OutputData) const
	{
		uint16_t hiword = ShiftLeftResult16Bits(~GetChannel(), 12) | (OutputData & 0xFFF);

		uint32_t checkdata = (hiword & 0x0FF) + ((hiword >> 8) & 0x0FF);

		uint32_t seconddata = ShiftLeftResult32Bits(hiword, 16) | ShiftLeftResult32Bits(checkdata, 8) | (numeric_cast<uint32_t>(~GetModuleAddress()) & 0x0FF);
		MD3BlockData sb(seconddata, true);
		return sb;
	}
	// Possibly should have second block class for this function...
	uint16_t GetOutputFromSecondBlock(MD3BlockData &SecondBlock) const
	{
		return SecondBlock.GetFirstWord() & 0x0FFF;
	}

	// Check the second block against the first (this one) to see if we have a valid AOM command
	bool VerifyAgainstSecondBlock(MD3BlockData &SecondBlock) const
	{
		// Check that the complements and the originals match
		if ((GetChannel() & 0x0f) != (~(SecondBlock.GetByte(0) >> 4) & 0x0F)) // Is the channel correct?
			return false;

		if ((GetModuleAddress() & 0x0ff) != (~SecondBlock.GetByte(3) & 0x0FF)) // Is the module address correct?
			return false;

		uint8_t checkdata = (SecondBlock.GetFirstWord() & 0x0FF) + ((SecondBlock.GetFirstWord() >> 8) & 0x0FF);
		if (checkdata != SecondBlock.GetByte(2))
			return false;

		return true;
	}
};
class MD3BlockFn30StoM: public MD3BlockFormatted
{
public:
	MD3BlockFn30StoM(MD3BlockData& parent, bool APL = false, bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// This Blockformat is a copy of the originating block header data, with the function code changed.
		// We change the function code, change the direction bit, mark as last block and recalc the checksum.
		data = parent.GetData();

		bool mastertostation = false;
		bool lastblock = true; // Only this block will be returned.

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HRP ? HRPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data &= numeric_cast<uint32_t>(~(APLBIT | RSFBIT | HRPBIT | DCPBIT)); // Clear the bits we are going to set.
		data |= flags;

		data &= ~(ShiftLeftResult32Bits(0x0FF,16) | DIRECTIONBIT);                       // Clear the bits we are going to set. SJECHECK
		data |= ShiftLeftResult32Bits(CONTROL_OR_SCAN_REQUEST_REJECTED, 16) | direction; // Set the function code

		// Regenerate the last byte
		SetEndByte(FormattedBlock, lastblock);
	}
};

class MD3BlockFn40MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn40MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	explicit MD3BlockFn40MtoS(uint8_t StationAddress)
	{
		bool lastblock = true;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, SYSTEM_SIGNON_CONTROL, numeric_cast<uint8_t>(0x80 | ~StationAddress), numeric_cast<uint8_t>(~SYSTEM_SIGNON_CONTROL));

		SetEndByte(FormattedBlock, lastblock);
	}
	bool IsValid() const
	{
		// Check that the complements and the originals match
		if (((data >> 24) & 0x7f) != ((~data >> 8) & 0x7F)) // Is the station address correct?
			return false;
		if (((data >> 16) & 0x0ff) != ((~data) & 0x0FF)) // Is the function code correct?
			return false;

		return true;
	}
};
class MD3BlockFn40StoM: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn40StoM(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	explicit MD3BlockFn40StoM(uint8_t StationAddress)
	{
		bool lastblock = true;
		bool mastertostation = false;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		data = CombineFormattedBytes(mastertostation, StationAddress, SYSTEM_SIGNON_CONTROL, numeric_cast<uint8_t>(0x80 | ~StationAddress), numeric_cast<uint8_t>(~SYSTEM_SIGNON_CONTROL));

		SetEndByte(FormattedBlock, lastblock);
	}
	bool IsValid() const
	{
		// Check that the complements and the originals match
		if (((data >> 24) & 0x7f) != ((~data >> 8) & 0x7F)) // Is the station address correct?
			return false;
		if (((data >> 16) & 0x0ff) != ((~data) & 0x0FF)) // Is the function code correct?
			return false;

		return true;
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn43MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn43MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn43MtoS(uint8_t StationAddress, uint16_t milliseconds)
	{
		bool lastblock = false; // Must always be followed by another data block
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert(milliseconds < 1000);                       // Max 10 bits;
		milliseconds &= 0x03FF;                            // Limited to these bits

		data = CombineFormattedBytes(mastertostation, StationAddress, SYSTEM_SET_DATETIME_CONTROL,milliseconds >> 8, milliseconds & 0x0FF);

		SetEndByte(FormattedBlock, lastblock);
	}

	uint16_t GetMilliseconds() const
	{
		return (data & 0x03FF);
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn44MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn44MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn44MtoS(uint8_t StationAddress, uint16_t milliseconds)
	{
		bool lastblock = false; // Must always be followed by another data block
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;
		assert(milliseconds < 1000);                       // Max 10 bits;
		milliseconds &= 0x03FF;                            // Limited to these bits

		data = CombineFormattedBytes(mastertostation, StationAddress, SYSTEM_SET_DATETIME_CONTROL_NEW, milliseconds >> 8, milliseconds & 0x0FF);

		SetEndByte(FormattedBlock, lastblock);
	}

	uint16_t GetMilliseconds() const
	{
		return (data & 0x03FF);
	}
};
class MD3BlockFn52MtoS: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn52MtoS(const MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	explicit MD3BlockFn52MtoS(uint8_t StationAddress)
	{
		bool lastblock = true;
		bool mastertostation = true;

		assert((StationAddress & 0x7F) == StationAddress); // Max of 7 bits;

		uint16_t lowword = 0; // All flags zero on master send, outstation will set...
		data = CombineFormattedBytes(mastertostation, StationAddress, SYSTEM_FLAG_SCAN, lowword >> 8, lowword & 0x0FF);

		SetEndByte(FormattedBlock, lastblock);
	}
};
class MD3BlockFn52StoM: public MD3BlockFormatted
{
public:
	explicit MD3BlockFn52StoM(const MD3BlockData& parent)
	{
		// This Block is a copy of the originating block header data, with the function code changed.
		// We change the function code, change the direction bit, mark as last block and recalc the checksum.
		data = parent.GetData();

		bool lastblock = true; // Only this block will be returned.
		bool mastertostation = false;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;
		data = (data & ~DIRECTIONBIT) | direction;

		// Regenerate the last byte
		SetEndByte(FormattedBlock, lastblock);
	}
	void SetSystemFlags(bool SPU, bool STI)
	{
		// The whole bottom word of the block should probably be zero, but we don't know what it is or how to use it, so leave alone.
		uint32_t flags = 0;
		flags |= SPU ? SPUBIT : 0x0000;
		flags |= STI ? STIBIT : 0x0000;
		data &= numeric_cast<uint32_t>(~(SPUBIT | STIBIT )); // Clear the bits we are going to set.
		data |= flags;

		endbyte &= 0xC0;         // Clear the CRC bits
		endbyte |= MD3CRC(data); // Max 6 bits returned
	}
	bool GetSystemPoweredUpFlag()
	{
		return (data & SPUBIT) == SPUBIT;
	}
	bool GetSystemTimeIncorrectFlag()
	{
		return (data & STIBIT) == STIBIT;
	}
	void SetFlags(bool RSF = false, bool HRP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
#endif
