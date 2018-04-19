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
* MD3.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3_H_
#define MD3_H_

#include <cstdint>

// Hide some of the code to make Logging cleaner
#define LOG(logger, filters, location, msg) \
	pLoggers->Log(openpal::LogEntry(logger, filters, location, std::string(msg).c_str(),-1));

// Note that in the message block format, these characters are not excluded from appearing - so their appearance and use is message state dependent
// THESE ARE NOT PRESENT IN THE tcp STREAMS...
#define MD3_START_OF_MESSAGE_CHAR	0x01		// SOM
#define MD3_SYNC_CHAR				0x16		// SYN
#define MD3_END_OF_MESSAGE_CHAR		0x03		// ETX

// Message block format:
// Possible multiple SOH
// SOM
// DATA 1,2,3,4
// BCC - Checksum and format bits: FRM, EOM, BCC[6]
// DATA 1,2,3,4
// BCC
// ETX

// The BCC uses the polynomial x^6 + x^4 + x^3 + x + 1 for a 6 bit result.
// Dividing the polynomial above into the 32 data bits, no carry, no borrow - mutlipled by 0x40. The 6 bit remainder is inverted to give the BCC.
// So the CRC key is 1101101 - 0x6D

// The first 16 bits of the first block contain the  DIR bit, 7 Bit Station Address, 8 bit Function Code,

// The next 16 bits vary by Function Code.

// For the commands indicated below that have Module information attached, the next 16 bits are:
// 8 Bit Module address, value of 0 is reserved, APL Flag, RSF Flag, HCP Flag, DCP Flag, # of modules (or channels - this is confusing I think 1 module can have 16 channels) 4 bits.

// ASF Application/Contract Dependent Flag
// RSF Remote Status Flag Change
// HRP HRER/Tim Tagged events pending
// DCP Digital Changes Pending

// The module addressing allows up to 255 Digital and Analog Inputs, and 255  Pulsed, Digital or Analog outputs.
// But then for each module address, we can have up to 16 channels. SO 16 analogs, or 16 digital bits. We always return all 16 bits for a digital unconditional.
// For deltas we have to return the channel, the value and the time tag.
// Also a value of 0 is actually == 1. A scan of zero channels does not make sense, and allows us up to 16.
// So our Outstation data structure needs to support these modes and adressing/module limits.

enum MD3_FORMAT_BIT
{
	FORMATTED_BLOCK = 0,
	UNFORMATTED_BLOCK = 1
};

enum MD3_END_OF_MESSAGE_BIT
{
	NOT_LAST_BLOCK = 0,
	LAST_BLOCK = 1
};

#define MD3_BROADCAST_ADDRESS			0
#define MD3_EXTENDED_ADDRESS_MARKER		0x7F
#define MD3_MAXIMUM_NORMAL_ADDRESS		0x7E
#define MD3_MAXIMUM_EXTENDED_ADDRESS	0x7FFF

enum MD3_MESSAGE_DIRECTION_BIT
{
	MASTER_TO_OUTSTATION = 0,
	OUTSTATION_TO_MASTER = 1
};

enum MD3_FUNCTION_CODE
{
	ANALOG_UNCONDITIONAL = 5,	// HAS MODULE INFORMATION ATTACHED
	ANALOG_DELTA_SCAN = 6,		// HAS MODULE INFORMATION ATTACHED
	DIGITAL_UNCONDITIONAL_OBS = 7,	// OBSOLETE // HAS MODULE INFORMATION ATTACHED
	DIGITAL_DELTA_SCAN = 8,		// OBSOLETE // HAS MODULE INFORMATION ATTACHED
	HRER_LIST_SCAN = 9,			// OBSOLETE
	DIGITAL_CHANGE_OF_STATE = 10,// OBSOLETE // HAS MODULE INFORMATION ATTACHED
	DIGITAL_CHANGE_OF_STATE_TIME_TAGGED = 11,
	DIGITAL_UNCONDITIONAL = 12,
	ANALOG_NO_CHANGE_REPLY = 13,	// HAS MODULE INFORMATION ATTACHED
	DIGITAL_NO_CHANGE_REPLY = 14,	// HAS MODULE INFORMATION ATTACHED
	CONTROL_REQUEST_OK = 15,
	FREEZE_AND_RESET = 16,
	POM_TYPE_CONTROL = 17,
	DOM_TYPE_CONTROL = 19,		// NOT USED
	INPUT_POINT_CONTROL = 20,
	RAISE_LOWER_TYPE_CONTROL = 21,
	AOM_TYPE_CONTROL = 23,
	CONTROL_OR_SCAN_REQUEST_REJECTED = 30,	// HAS MODULE INFORMATION ATTACHED
	COUNTER_SCAN = 31,						// HAS MODULE INFORMATION ATTACHED
	SYSTEM_SIGNON_CONTROL = 40,
	SYSTEM_SIGNOFF_CONTROL = 41,
	SYSTEM_RESTART_CONTROL = 42,
	SYSTEM_SET_DATETIME_CONTROL = 43,
	FILE_DOWNLOAD = 50,
	FILE_UPLOAD = 51,
	SYSTEM_FLAG_SCAN = 52,
	LOW_RES_EVENTS_LIST_SCAN = 60
};

#endif