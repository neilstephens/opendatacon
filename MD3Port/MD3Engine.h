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
* MD3Engine.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3ENGINE_H_
#define MD3ENGINE_H_

// We want to use the asiosocketmanager for communications, but wrap it in a class derived from a "general" comms interface class.This way can add serial later if necessary.
// Also this will allow us to more easily mock the comms layer for unit testing.

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>

#include "MD3.h"

#define EOMBIT 0x40
#define FOMBIT 0x80
#define APLBIT 0x0080
#define RSFBIT 0x0040
#define HCPBIT 0x0020
#define DCPBIT 0x0010
#define DIRECTIONBIT 0x80000000
#define MOREEVENTSBIT 0x0800

const int MD3BlockArraySize = 6;
typedef  std::array<uint8_t, MD3BlockArraySize> MD3BlockArray;

uint8_t MD3CRC(const uint32_t data);
bool MD3CRCCompare(const uint8_t crc1, const uint8_t crc2);


// Every block in MD3 is 5 bytes, plus one zero byte. If we dont have 5 bytes, we dont have a block. The last block is marked as such.
// We will create a class to load the 6 byte array into, then we can just ask it to return the information in the variety of ways we require,
// depending on the block content.
// There are two ways this class is used - one for decoding, one for encoding.
// Create child classes for each of the functions where the data block has specific layout and meaning.
class MD3BlockData
{
public:
	MD3BlockData() {};

	// We have received 6 bytes (a block on a stream now we need to decode it)
	MD3BlockData(const MD3BlockArray _data)
	{
		data = _data[0] << 24 | _data[1] << 16 | _data[2] << 8 | _data[3];
		endbyte = _data[4];
		assert(_data[5] == 0x00);	// Sixth byte should always be zero.
	}

	MD3BlockData(uint16_t firstword, uint16_t secondword, bool lastblock = false)
	{
		data = (uint32_t)firstword << 16 | (uint32_t)secondword;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block, must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	MD3BlockData(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, bool lastblock = false)
	{
		data = (((uint32_t)b1 & 0x0FF) << 24) | (((uint32_t)b2 & 0x0FF) << 16) | (((uint32_t)b3 & 0x0FF) << 8) | ((uint32_t)b4 & 0x0FF);

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block, must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	MD3BlockData(uint32_t _data, bool lastblock = false)
	{
		data = _data;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	bool IsEndOfMessageBlock()
	{
		return ((endbyte & EOMBIT) == EOMBIT);
	}
	void MarkAsEndOfMessageBlock()
	{
		endbyte |= EOMBIT;	// Does not change CRC
	}
	bool IsFormattedBlock()
	{
		return !((endbyte & FOMBIT) == FOMBIT);	// 0 is Formatted, 1 is Unformatted
	}

	bool CheckSumPasses()
	{
		uint8_t calc = MD3CRC(data);
		return MD3CRCCompare(calc, endbyte);
	}
	uint8_t GetByte(int b)
	{
		assert((b >= 0) && (b < 4));
		return ((data >> (8 * (3 - b))) & 0x0FF);
	}
	uint16_t GetFirstWord()
	{
		return (data >> 16);
	}
	uint16_t GetSecondWord()
	{
		return (data & 0xFFFF);
	}
	// The extended address would be retrived from the correct block with this call.
	const uint32_t GetData()
	{
		return data;
	}
	const uint8_t GetEndByte()
	{
		return endbyte;
	}
	std::string ToBinaryString()
	{
		std::ostringstream oss;

		oss.put(data >> 24);
		oss.put((data >> 16) & 0x0FF);
		oss.put((data >> 8) & 0x0FF);
		oss.put(data & 0x0FF);
		oss.put(endbyte);
		oss.put(0x00);

		return oss.str();
	}
	std::string ToString()
	{
		std::ostringstream oss;

		oss << (data >> 24);
		oss << ',';
		oss << ((data >> 16) & 0x0FF);
		oss << ',';
		oss << ((data >> 8) & 0x0FF);
		oss << ',';
		oss << (data & 0x0FF);
		oss << ',';
		oss << (endbyte);
		oss << ',';
		oss << (0x00);
		return oss.str();
	}

protected:
	uint32_t data = 0;
	uint8_t endbyte = 0;
};
class MD3BlockFormatted : public MD3BlockData
{
public:
	MD3BlockFormatted() {};
	MD3BlockFormatted(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	// Create a formatted block including checksum
	// Note if the station address is set to 0x7F (MD3_EXTENDED_ADDRESS_MARKER), then the next data block contains the address.
	MD3BlockFormatted(uint8_t stationaddress, bool mastertostation, MD3_FUNCTION_CODE functioncode, uint8_t moduleaddress, uint8_t channels, bool lastblock = false,
		bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// Channels -> 0 on the wire == 1 channel, 15 == 16
		channels--;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert((channels & 0x0F) == channels);	// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)functioncode << 16 | (uint32_t)moduleaddress << 8 | flags | channels;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	// Apply to formatted blocks only!
	bool IsMasterToStationMessage()
	{
		return !((data & DIRECTIONBIT) == DIRECTIONBIT);
	}
	uint8_t GetStationAddress()
	{
		// TODO: SJE Extended Addressing - Address 0x7F indicates an extended address.
		return (data >> 24) & 0x7F;
	}
	uint8_t GetModuleAddress()
	{
		return (data >> 8) & 0xFF;
	}
	uint8_t GetFunctionCode()
	{
		// How do we check it is a valid function code first?
		// There are a few complex ways to check the values of an enum - mostly by creating a new class.
		// As we are likely to have some kind of function dispatcher class anyway - dont check here.
		return (data >> 16) & 0xFF;
	}
	uint8_t GetChannels()
	{
		// 0 on the wire is 1 channel
		return (data & 0x0F)+1;
	}

	// Set the RSF flag. This comes from a global flag we maintain, and we set just before we send.
	// Some of the child classes DONT have this flag, so we need to override to stop them from corrupting data.
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		uint32_t flags = 0;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;
		data &= ~(RSFBIT|HCPBIT|DCPBIT);	// Clear the bits we are going to set.
		data |= flags;

		endbyte &= 0xC0;			// Clear the CRC bits
		endbyte |= MD3CRC(data);	// Max 6 bits returned
	}
	bool GetAPL()
	{
		return ((data & APLBIT) == APLBIT);
	}
	bool GetRSF()
	{
		return ((data & RSFBIT) == RSFBIT);
	}
	bool GetHCP()
	{
		return ((data & HCPBIT) == HCPBIT);
	}
	bool GetDCP()
	{
		return ((data & DCPBIT) == DCPBIT);
	}
};

// The datablocks can take on different formats where the bytes and buts in the block change meaning.
// Use child classes to separate out this on a function by function basis
class MD3BlockFn9 : public MD3BlockFormatted
{
public:
	MD3BlockFn9(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn9(uint8_t stationaddress, bool mastertostation, uint8_t sequencenumber, uint8_t maximumevents, bool moreevents, bool lastblock = false)
	{
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;
		uint32_t mevbit = moreevents ? MOREEVENTSBIT : 0;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert((sequencenumber & 0x0F) == sequencenumber);	// Max 4 bits;

		data = direction | mevbit | (uint32_t)stationaddress << 24 | (uint32_t)HRER_LIST_SCAN << 16 | ((uint32_t)sequencenumber & 0x0F) << 12 | maximumevents;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	void SetEventCountandMoreEventsFlag(uint8_t events, bool moreevents )
	{
		data &= ~(MOREEVENTSBIT | 0x0FF);	// Clear the bits we are going to set.

		uint32_t mevbit = moreevents ? MOREEVENTSBIT : 0;
		data |=  mevbit | events;

		endbyte &= 0xC0;			// Clear the CRC bits
		endbyte |= MD3CRC(data);	// Max 6 bits returned
	}
	bool GetMoreEventsFlag()
	{
		return (data & MOREEVENTSBIT) == MOREEVENTSBIT;
	}
	uint8_t GetSequenceNumber()
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 12) & 0x0F);
	}
	uint8_t GetEventCount()
	{
		return (data & 0x0FF);
	}
	static uint16_t MilliSecondsPacket( uint16_t allmsec)
	{
		assert(allmsec < 31999);
		uint16_t deltasec = allmsec / 1000;
		uint16_t msec = allmsec % 1000;

		return  0x8000 | (deltasec & 0x1F) << 10 | (msec & 0x00FF);
	}
	static uint16_t HREREventPacket(uint8_t bitstate, uint8_t channel, uint8_t moduleaddress)
	{
		return ((uint16_t)bitstate << 14) | ((uint16_t)channel << 8) | (uint16_t)moduleaddress;
	}
	static uint16_t FillerPacket()
	{
		return 0;
	}
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn10 : public MD3BlockFormatted
{
public:
	MD3BlockFn10(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn10(uint8_t stationaddress, bool mastertostation, uint8_t moduleaddress, uint8_t modulecount, bool lastblock, bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert((modulecount & 0x0F) == modulecount);	// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_CHANGE_OF_STATE << 16 | (uint32_t)moduleaddress << 8 | flags | (modulecount & 0x0F);

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	uint8_t GetModuleCount()
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetModuleCount(uint8_t modulecount)
	{
		data &= ~(0x0F);	// Clear the bits we are going to set.
		data |= modulecount;

		endbyte &= 0xC0;			// Clear the CRC bits
		endbyte |= MD3CRC(data);	// Max 6 bits returned
	}
};

class MD3BlockFn11MtoS : public MD3BlockFormatted
{
public:
	MD3BlockFn11MtoS(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn11MtoS(uint8_t stationaddress, uint8_t taggedeventcount, uint8_t digitalsequencenumber, uint8_t modulecount, bool lastblock = false)
	{
		bool mastertostation = true;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);					// Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);				// Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber);	// Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);						// Max 4 bits;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_CHANGE_OF_STATE_TIME_TAGGED << 16 | ((uint32_t)taggedeventcount & 0x0F) << 12 | ((uint32_t)digitalsequencenumber & 0x0F) << 4 | modulecount;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	uint8_t GetTaggedEventCount()
	{
		// Can be a zero event count - in the spec 1's numbered. (0's numbered is when 0 is equivalent to 1 - 0 is not valid)
		return (data >> 12) & 0x0F;
	}
	uint8_t GetDigitalSequenceNumber()
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 4) & 0x0F);
	}
	uint8_t GetModuleCount()
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn11StoM : public MD3BlockFormatted
{
public:
	MD3BlockFn11StoM(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn11StoM(uint8_t stationaddress, uint8_t taggedeventcount, uint8_t digitalsequencenumber, uint8_t modulecount, bool lastblock = false,
		bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		bool mastertostation = false;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);					// Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);				// Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber);	// Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);						// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_CHANGE_OF_STATE_TIME_TAGGED << 16 | ((uint32_t)taggedeventcount & 0x0F) << 12 | ((uint32_t)digitalsequencenumber & 0x0F) << 8 | flags | modulecount;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	uint8_t GetTaggedEventCount()
	{
		// Can be a zero event count - in the spec 1's numbered. (0's numbered is when 0 is equivalent to 1 - 0 is not valid)
		return (data >> 12) & 0x0F;
	}
	void SetTaggedEventCount(uint8_t eventcount)
	{
		data &= ~(0x0F << 12);	// Clear the bits we are going to set.
		data |= (uint32_t)eventcount << 12;

		endbyte &= 0xC0;			// Clear the CRC bits
		endbyte |= MD3CRC(data);	// Max 6 bits returned
	}
	uint8_t GetDigitalSequenceNumber()
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 8) & 0x0F);
	}
	uint8_t GetModuleCount()
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
	void SetModuleCount(uint8_t modulecount)
	{
		data &= ~(0x0F);	// Clear the bits we are going to set.
		data |= modulecount;

		endbyte &= 0xC0;			// Clear the CRC bits
		endbyte |= MD3CRC(data);	// Max 6 bits returned
	}
	static uint16_t FillerPacket()
	{
		return 0;
	}
	static uint16_t MilliSecondsDiv256OffsetPacket(uint16_t allmsec)
	{
		// There is a msec in the data packet, this is to allow us extra offset - should be divisible by 256 for accuracy
		return  allmsec/256;
	}
};
// The reply to Fn12 is actually a Fn11 packet
class MD3BlockFn12MtoS : public MD3BlockFormatted
{
public:
	MD3BlockFn12MtoS( MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn12MtoS(uint8_t stationaddress, uint8_t startmoduleaddress, uint8_t digitalsequencenumber, uint8_t modulecount, bool lastblock = false)
	{
		bool mastertostation = true;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);					// Max of 7 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber);	// Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);						// Max 4 bits;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_UNCONDITIONAL << 16 | ((uint32_t)startmoduleaddress) << 8 | ((uint32_t)digitalsequencenumber & 0x0F) << 4 | modulecount;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	uint8_t GetStartingModuleAddress()
	{
		return (data >> 8) & 0xFF;
	}
	uint8_t GetDigitalSequenceNumber()
	{
		return ((data >> 4) & 0x0F);
	}
	uint8_t GetModuleCount()
	{
		// 1's numbered
		return (data & 0x0F);
	}
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};


class MD3BlockFn14StoM : public MD3BlockFormatted
{
public:
	MD3BlockFn14StoM(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn14StoM(uint8_t stationaddress, uint8_t moduleaddress, uint8_t modulecount, bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		bool lastblock = true;
		bool mastertostation = false;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert((modulecount & 0x0F) == modulecount);	// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_NO_CHANGE_REPLY << 16 | (uint32_t)moduleaddress << 8 | flags | (modulecount & 0x0F);

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	MD3BlockFn14StoM(uint8_t stationaddress,  uint8_t digitalsequencenumber, bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		bool lastblock = true;
		bool mastertostation = false;
		uint8_t taggedeventcount = 0;	// Not returning anything..
		uint8_t modulecount = 0;		// Not returning anything..

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);					// Max of 7 bits;
		assert((taggedeventcount & 0x0F) == taggedeventcount);				// Max 4 bits;
		assert((digitalsequencenumber & 0x0F) == digitalsequencenumber);	// Max 4 bits;
		assert((modulecount & 0x0F) == modulecount);						// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)DIGITAL_NO_CHANGE_REPLY << 16 | ((uint32_t)taggedeventcount & 0x0F) << 12 | ((uint32_t)digitalsequencenumber & 0x0F) << 8 | flags | modulecount;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	uint8_t GetDigitalSequenceNumber()
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return ((data >> 8) & 0x0F);
	}
	uint8_t GetModuleCount()
	{
		// Can be a zero module count
		return (data & 0x0F);
	}
};

class MD3BlockFn15StoM : public MD3BlockFormatted
{
public:
	MD3BlockFn15StoM(MD3BlockData& parent)
	{
		// This Blockformat is a copy of the orginating block header data, with the function code changed.
		//
		// We change the function code, change the direction bit, mark as last block and recalc the checksum.
		data = parent.GetData();

		bool mastertostation = false;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		data &= ~((uint32_t)0x0FF << 16 | DIRECTIONBIT);			// Clear the bits we are going to set.
		data |= (uint32_t)CONTROL_REQUEST_OK << 16 | direction;	// Set the function code and direction

		// Regenerate the last byte
		bool lastblock = true;	// Only this block will be returned.
		endbyte = MD3CRC(data);	// Max 6 bits returned
								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		assert(false);
		// No flags in this formatted packet
	}
};
class MD3BlockFn17MtoS : public MD3BlockFormatted
{
public:
	MD3BlockFn17MtoS(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}

	MD3BlockFn17MtoS(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t OutputSelection)
	{
		bool lastblock = false;
		bool mastertostation = true;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((StationAddress & 0x7F) == StationAddress);	// Max of 7 bits;

		uint16_t lowword = ((uint16_t)ModuleAddress << 8) | ((uint16_t)OutputSelection & 0x00FF);
		data = direction | (uint32_t)StationAddress << 24 | (uint32_t)POM_TYPE_CONTROL << 16 | (uint32_t)lowword;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	uint8_t GetOutputSelection()
	{
		return data & 0x0FF;
	}
	// The second block in the message only contains a different format of the information in the first
	MD3BlockData GenerateSecondBlock()
	{
		uint16_t lowword = 1 << GetOutputSelection();
		uint32_t direction = GetData() & 0x8000;
		uint32_t seconddata = direction | ((uint32_t)~GetStationAddress() & 0x07f) << 24 | (((uint32_t)~GetModuleAddress() & 0x0FF) << 16) | (uint32_t)lowword;
		MD3BlockData sb(seconddata, true);
		return sb;
	}
	// Check the second block against the first (this one) to see if we have a valid POM command
	bool VerifyAgainstSecondBlock(MD3BlockData &SecondBlock)
	{
		// Check that the complements and the orginals match
		if (GetStationAddress() != ((~SecondBlock.GetData() >> 24) & 0x7F))	// Is the station address correct?
			return false;

		if ((GetModuleAddress() & 0x0ff) != ((~SecondBlock.GetData() >> 16) & 0x0FF))	// Is the function code correct?
			return false;

		uint16_t lowword = 1 << GetOutputSelection();
		if (lowword != SecondBlock.GetSecondWord())
			return false;

		return true;
	}

	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
class MD3BlockFn30StoM : public MD3BlockFormatted
{
public:
	MD3BlockFn30StoM(MD3BlockData& parent, bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// This Blockformat is a copy of the orginating block header data, with the function code changed.
		//
		// We change the function code, change the direction bit, mark as last block and recalc the checksum.
		data = parent.GetData();

		bool mastertostation = false;
		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data &= ~(APLBIT | RSFBIT | HCPBIT | DCPBIT);	// Clear the bits we are going to set.
		data |= flags;

		data &= ~((uint32_t)0x0FF << 16 | DIRECTIONBIT);			// Clear the bits we are going to set.
		data |= (uint32_t)CONTROL_OR_SCAN_REQUEST_REJECTED << 16 | direction;	// Set the function code

																// Regenerate the last byte
		bool lastblock = true;	// Only this block will be returned.
		endbyte = MD3CRC(data);	// Max 6 bits returned
								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
};

class MD3BlockFn40 : public MD3BlockFormatted
{
public:
	MD3BlockFn40(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn40(uint8_t stationaddress, bool mastertostation = true)
	{
		bool lastblock = true;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;

		uint16_t lowword = 0x8000 | (uint16_t)(~stationaddress) << 8 | ((uint16_t)(~SYSTEM_SIGNON_CONTROL) & 0x00FF);
		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)SYSTEM_SIGNON_CONTROL << 16 | (uint32_t)lowword;

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	bool IsValid()
	{
		// Check that the complements and the orginals match
		if (((data >> 24) & 0x7f) != ((~data >> 8) & 0x7F))	// Is the station address correct?
			return false;
		if (((data >> 16) & 0x0ff) != ((~data) & 0x0FF))	// Is the function code correct?
			return false;

		return true;
	}
};
class MD3BlockFn43MtoS : public MD3BlockFormatted
{
public:
	MD3BlockFn43MtoS(MD3BlockData& parent)
	{
		data = parent.GetData();
		endbyte = parent.GetEndByte();
	}
	MD3BlockFn43MtoS(uint8_t stationaddress, uint16_t milliseconds)
	{
		bool lastblock = false;	// Must always be followed by another data block
		bool mastertostation = true;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert(milliseconds < 1000);	// Max 10 bits;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)SYSTEM_SET_DATETIME_CONTROL << 16 | ((uint32_t)milliseconds & 0x03FF);

		endbyte = MD3CRC(data);	// Max 6 bits returned

								// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	uint16_t GetMilliseconds()
	{
		// Function 11 and 12 This is the same part of the packet that is used for the 4 flags in other commands
		return (data & 0x03FF);
	}
	void SetFlags(bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// No flags in this formatted packet
		assert(false);
	}
};
#endif