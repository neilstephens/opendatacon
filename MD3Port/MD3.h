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

////////////////// IMPORTANT //////////////////////////////
// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
// #define CATCH_CONFIG_RUNNER
//
////////////////// IMPORTANT //////////////////////////////

#include <cstdint>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <spdlog/spdlog.h>



// Global TODO List in priority order
//TODO: Trigger quality callbacks with ONLINE for when the master has valid data for the first time??
//TODO: Master Sign on control called on ODC trigger - also send as part of poll?
//TODO: Master OLD Digital Read
//TODO: All command messages to have their own type, constructor to make understanding parameters simpler. Most already do.


// Hide some of the code to make Logging cleaner
#define LOGDEBUG(msg) \
	if (auto log = spdlog::get("MD3Port")) log->debug(msg);
#define LOGERROR(msg) \
	if (auto log = spdlog::get("MD3Port")) log->error(msg);
#define LOGWARN(msg) \
	if (auto log = spdlog::get("MD3Port")) log->warn(msg);
#define LOGINFO(msg) \
	if (auto log = spdlog::get("MD3Port")) log->info(msg);


typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;

typedef uint64_t MD3Time; // msec since epoch, utc, most time functions are uint64_t

static MD3Time MD3Now()
{
	// To get the time to pass through ODC events. MD3 Uses UTC time in commands - as you would expect.
	return static_cast<MD3Time>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}


// Note that in the message block format, these characters are not excluded from appearing - so their appearance and use is message state dependent
// THESE ARE NOT PRESENT IN THE tcp STREAMS...
#define MD3_START_OF_MESSAGE_CHAR   0x01       // SOM
#define MD3_SYNC_CHAR                     0x16 // SYN
#define MD3_END_OF_MESSAGE_CHAR           0x03 // ETX

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

#define MD3_BROADCAST_ADDRESS             0
#define MD3_EXTENDED_ADDRESS_MARKER       0x7F
#define MD3_MAXIMUM_NORMAL_ADDRESS        0x7E
#define MD3_MAXIMUM_EXTENDED_ADDRESS      0x7FFF

enum MD3_MESSAGE_DIRECTION_BIT
{
	MASTER_TO_OUTSTATION = 0,
	OUTSTATION_TO_MASTER = 1
};

enum MD3_FUNCTION_CODE
{
	ANALOG_UNCONDITIONAL = 5,      // HAS MODULE INFORMATION ATTACHED
	ANALOG_DELTA_SCAN = 6,         // HAS MODULE INFORMATION ATTACHED
	DIGITAL_UNCONDITIONAL_OBS = 7, // OBSOLETE // HAS MODULE INFORMATION ATTACHED
	DIGITAL_DELTA_SCAN = 8,        // OBSOLETE // HAS MODULE INFORMATION ATTACHED
	HRER_LIST_SCAN = 9,            // OBSOLETE
	DIGITAL_CHANGE_OF_STATE = 10,  // OBSOLETE // HAS MODULE INFORMATION ATTACHED
	DIGITAL_CHANGE_OF_STATE_TIME_TAGGED = 11,
	DIGITAL_UNCONDITIONAL = 12,
	ANALOG_NO_CHANGE_REPLY = 13,  // HAS MODULE INFORMATION ATTACHED
	DIGITAL_NO_CHANGE_REPLY = 14, // HAS MODULE INFORMATION ATTACHED
	CONTROL_REQUEST_OK = 15,
	FREEZE_AND_RESET = 16,
	POM_TYPE_CONTROL = 17,
	DOM_TYPE_CONTROL = 19, // NOT USED
	INPUT_POINT_CONTROL = 20,
	RAISE_LOWER_TYPE_CONTROL = 21,
	AOM_TYPE_CONTROL = 23,
	CONTROL_OR_SCAN_REQUEST_REJECTED = 30, // HAS MODULE INFORMATION ATTACHED
	COUNTER_SCAN = 31,                     // HAS MODULE INFORMATION ATTACHED
	SYSTEM_SIGNON_CONTROL = 40,
	SYSTEM_SIGNOFF_CONTROL = 41,
	SYSTEM_RESTART_CONTROL = 42,
	SYSTEM_SET_DATETIME_CONTROL = 43,
	SYSTEM_SET_DATETIME_CONTROL_NEW = 44,
	FILE_DOWNLOAD = 50,
	FILE_UPLOAD = 51,
	SYSTEM_FLAG_SCAN = 52,
	LOW_RES_EVENTS_LIST_SCAN = 60
};

enum PointType { Binary, Analog, Counter, BinaryControl, AnalogControl };
enum BinaryPointType { BASICINPUT, TIMETAGGEDINPUT, DOMOUTPUT, POMOUTPUT };
enum PollGroupType { BinaryPoints, AnalogPoints, TimeSetCommand, NewTimeSetCommand, SystemFlagScan };


class MD3Point // Abstract Class
{
	//TODO: Can we convert protection to strands??
	//TODO: Integrate ODC quality instead of HasBeenSet flag and Module failed flag.
	/*	NONE              = 0,
	ONLINE            = 1<<0,
	RESTART           = 1<<1,
	COMM_LOST         = 1<<2,
	REMOTE_FORCED     = 1<<3,
	LOCAL_FORCED      = 1<<4,
	OVERRANGE         = 1<<5,
	REFERENCE_ERR     = 1<<6,
	ROLLOVER          = 1<<7,
	DISCONTINUITY     = 1<<8,
	CHATTER_FILTER    = 1<<9*/

public:
	MD3Point() {}

	// We need these as we DO NOT want to copy the mutex!
	MD3Point(const MD3Point &src):
		Index(src.Index),
		ModuleAddress(src.ModuleAddress),
		ModuleFailed(src.ModuleFailed),
		Channel(src.Channel),
		PollGroup(src.PollGroup),
		ChangedTime(src.ChangedTime),
		HasBeenSet(src.HasBeenSet)
	{}
	virtual ~MD3Point() = 0; // Make the base class pure virtual

	MD3Point& operator=(const MD3Point& src)
	{
		Index = src.Index;
		ModuleAddress = src.ModuleAddress;
		Channel = src.Channel;
		PollGroup = src.PollGroup;
		ChangedTime = src.ChangedTime;
		ModuleFailed = src.ModuleFailed;
		HasBeenSet = src.HasBeenSet;
		return *this;
	}

	MD3Point(uint32_t index, uint8_t moduleaddress, uint8_t channel, MD3Time changedtime, uint8_t pollgroup):
		Index(index),
		ModuleAddress(moduleaddress),
		Channel(channel),
		PollGroup(pollgroup),
		ChangedTime(changedtime)
	{}

	// These first 4 never change, so no protection
	uint32_t GetIndex() { return Index; }
	uint8_t GetModuleAddress() { return ModuleAddress; }
	uint8_t GetChannel() { return Channel; }
	uint8_t GetPollGroup() { return PollGroup; }

	//TODO: Do we need to get them all at the same time - so one does not get changed in the middle??
	bool GetModuleFailed() { std::unique_lock<std::mutex> lck(PointMutex); return ModuleFailed; }
	MD3Time GetChangedTime() { std::unique_lock<std::mutex> lck(PointMutex); return ChangedTime; }
	bool GetHasBeenSet() { std::unique_lock<std::mutex> lck(PointMutex); return HasBeenSet; }
	//	void SetChangedTime(const MD3Time &ct) { std::unique_lock<std::mutex> lck(PointMutex); ChangedTime = ct; }; // Not needed...

protected:
	std::mutex PointMutex;
	uint32_t Index = 0;
	uint8_t ModuleAddress = 0;
	bool ModuleFailed = false; // Will be set to true if the connection to a master through ODC signals the master is not talking to its slave. For digitals we send a different response
	uint8_t Channel = 0;
	uint8_t PollGroup = 0;
	MD3Time ChangedTime = static_cast<MD3Time>(0); // msec since epoch. 1970,1,1 Only used for Fn9 and 11 queued data. TimeStamp is Uint48_t, MD3 is uint64_t but does not overflow.
	bool HasBeenSet = false;                       // To determine if we have been set since startup
};


class MD3BinaryPoint: public MD3Point
{
public:
	MD3BinaryPoint() {}
	MD3BinaryPoint(const MD3BinaryPoint &src): MD3Point(src),
		Binary(src.Binary),
		ModuleBinarySnapShot(src.ModuleBinarySnapShot),
		Changed(src.Changed),
		PointType(src.PointType)
	{}
	MD3BinaryPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup, BinaryPointType pointtype): MD3Point(index, moduleaddress, channel, (MD3Time)0, pollgroup),
		PointType(pointtype)
	{}

	MD3BinaryPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup, BinaryPointType pointtype, uint8_t binval, bool changed, MD3Time changedtime):
		MD3Point(index, moduleaddress, channel, changedtime, pollgroup),
		Binary(binval),
		Changed(changed),
		PointType(pointtype)
	{}

	~MD3BinaryPoint();

	// Needed for the templated Queue
	MD3BinaryPoint& operator=(const MD3BinaryPoint& src);

	BinaryPointType GetPointType() const { return PointType; }

	uint8_t GetBinary() { std::unique_lock<std::mutex> lck(PointMutex); return Binary; }
	uint16_t GetModuleBinarySnapShot() { std::unique_lock<std::mutex> lck(PointMutex); return ModuleBinarySnapShot; }

	bool GetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); return Changed; }
	void SetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); Changed = true; }
	bool GetAndResetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); bool res = Changed; Changed = false; return res; }

	void SetBinary(const uint8_t &b, const MD3Time &ctime) { std::unique_lock<std::mutex> lck(PointMutex); HasBeenSet = true; ChangedTime = ctime; Changed = (Binary != b); Binary = b; }
	void SetChanged(const bool &c) { std::unique_lock<std::mutex> lck(PointMutex); Changed = c; }
	void SetModuleBinarySnapShot(const uint16_t &bm) { std::unique_lock<std::mutex> lck(PointMutex); ModuleBinarySnapShot = bm; }

protected:
	// Only the values below will be changed in two places
	uint8_t Binary = 0x01;
	uint16_t ModuleBinarySnapShot = 0; // Used for the queue necessary to handle Fn11 time tagged events. Have to remember all 16 bits when the event happened
	bool Changed = true;
	BinaryPointType PointType = BASICINPUT;
};

class MD3AnalogCounterPoint: public MD3Point
{
public:
	MD3AnalogCounterPoint() {}

	MD3AnalogCounterPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, uint8_t pollgroup): MD3Point(index, moduleaddress, channel, (MD3Time)0, pollgroup)
	{}
	~MD3AnalogCounterPoint() {}

	uint16_t GetAnalog() { std::unique_lock<std::mutex> lck(PointMutex); return Analog; }
	uint16_t GetAnalogAndDelta(int &delta) { std::unique_lock<std::mutex> lck(PointMutex); delta = (int)Analog - (int)LastReadAnalog; LastReadAnalog = Analog; return Analog; }

	void SetAnalog(const uint16_t & a, const MD3Time &ctime) { std::unique_lock<std::mutex> lck(PointMutex); HasBeenSet = true; Analog = a; ChangedTime = ctime; }
	void SetLastReadAnalog(const uint16_t & a) { std::unique_lock<std::mutex> lck(PointMutex); LastReadAnalog = a; }

protected:
	uint16_t Analog = 0x8000;
	uint16_t LastReadAnalog = 0x8000;
};

typedef std::map<uint32_t, std::shared_ptr<MD3BinaryPoint>>::iterator ODCBinaryPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>>::iterator MD3BinaryPointMapIterType;
typedef std::map<uint32_t, std::shared_ptr<MD3AnalogCounterPoint>>::iterator ODCAnalogCounterPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>>::iterator MD3AnalogCounterPointMapIterType;
typedef std::map<uint8_t, uint16_t> ModuleMapType;


class MD3PollGroup
{
public:
	MD3PollGroup():
		ID(0),
		pollrate(0),
		polltype(BinaryPoints),
		ForceUnconditional(false)
	{ }

	MD3PollGroup(uint32_t ID_, uint32_t pollrate_, PollGroupType polltype_, bool forceunconditional, bool timetaggeddigital):
		ID(ID_),
		pollrate(pollrate_),
		polltype(polltype_),
		ForceUnconditional(forceunconditional),
		TimeTaggedDigital(timetaggeddigital)
	{ }

	uint32_t ID;
	uint32_t pollrate;
	PollGroupType polltype;
	bool ForceUnconditional;       // Used to prevent use of delta or COS commands.
	bool TimeTaggedDigital;        // Set to true if this poll group contains time tagged digital points.
	ModuleMapType ModuleAddresses; // The second value is for channel count
	// As we load points we will build this list
};
#endif
