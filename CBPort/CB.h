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
* CB.h
*
*  Created on: 10/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef CB_H_
#define CB_H_

// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
#define NONVSTESTING

#include <cstdint>
#include <shared_mutex>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>

// Conitel Mosaic Data Types
//	DIG - Used
//	MCDC - Used - Same as SMCDC
//	SMCDC - Used - input binary - data and change bit - 6 channels per location. Same as MCC.
//	SOEDIG - Same as DIG, but Mosaic uses it to calculate group and index from its scan group, payload and index.
//	AHA - Used
//	SMCDA - Used - input, binary - data and change bit - 6 channels per location. Same as MCA.
//	ACC
//	SPA
//	RST - Used
//	CONTROL - Used
//	ACC24
//	MCDA - Used - Same as SMCDA
//	ANA6 - Used - Half of an analog 12 bit value... //TODO: Need to combine these and split out again.

// Microsoft Message Analyzer filter
// Virtually all the master commands are 4 bytes only. So look for these only with command # > 0
// Then we can see what has to be implemented. Fn0 is the bulk of the traffic.
// Filter for Conitel/Baker traffic - based on payload size, and Function code != 0
// (tcp.PayloadLength != 0) && ((tcp.PayloadLength % 4) == 0) && (tcp.payload[0] > 0x0F)


// Hide some of the code to make Logging cleaner
#define LOGTRACE(...) \
	if (auto log = odc::spdlog_get("CBPort")) \
	log->trace(__VA_ARGS__)
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("CBPort")) \
	log->debug(__VA_ARGS__)
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("CBPort")) \
	log->error(__VA_ARGS__)
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("CBPort"))  \
	log->warn(__VA_ARGS__)
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("CBPort")) \
	log->info(__VA_ARGS__)
#define LOGCRITICAL(...) \
	if (auto log = odc::spdlog_get("CBPort")) \
	log->critical(__VA_ARGS__)


void CommandLineLoggingSetup(spdlog::level::level_enum log_level);
void CommandLineLoggingCleanup();

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;

typedef uint64_t CBTime; // msec since epoch, utc, most time functions are uint64_t

CBTime CBNowUTC();

const CBTime CBTimeOneDay = 1000 * 60 * 60 * 24;
const CBTime CBTimeOneHour = 1000 * 60 * 60;

// Give us the time for the start of the current day in CBTime (msec since epoch)
inline CBTime GetDayStartTime(CBTime time)
{
	CBTime Days = time / CBTimeOneDay;
	return (Days * CBTimeOneDay); // msec*sec*min*hours
}
// Give us the time of day value in msec
inline CBTime GetTimeOfDayOnly(CBTime time)
{
	return time - GetDayStartTime(time);
}

// Get the hour value for the current time value 0-23
inline CBTime GetHour(CBTime time)
{
	return time / CBTimeOneHour % 24;
}
// We use for signed/unsigned conversions, where we know we will not have problems.
// Static casting all over the place still produces a lot of gcc warning messages.
// Also we can put checks in here if required.
template <class OT, class ST>
OT numeric_cast(const ST value)
{
	return static_cast<OT>(value);
}

class protected_bool
{
public:
	protected_bool(): val(false) {}
	protected_bool(bool _val): val(_val) {}
	bool getandset(bool newval)
	{
		std::unique_lock<std::shared_timed_mutex> lck(m);
		bool retval = val;
		val = newval;
		return retval;
	}
	void set(bool newval)
	{
		std::unique_lock<std::shared_timed_mutex> lck(m);
		val = newval;
	}
	bool get(void)
	{
		std::shared_lock<std::shared_timed_mutex> lck(m);
		return val;
	}
private:
	std::shared_timed_mutex m;
	bool val;
};

enum PointType { Binary, Analog, Counter, BinaryControl, AnalogControl };
enum BinaryPointType { DIG, MCA, MCB, MCC, BINCONTROL }; // Inputs and outputs
enum AnalogCounterPointType { ANA, ANA6, ACC12, ACC24, ANACONTROL };
enum PollGroupType { Scan, TimeSetCommand, SOEScan, SystemFlagScan };

#define MISSINGVALUE 0 // If we have a missing point or have no data, substitute this. MD3 it was 0x8000

// Need to hold the packet position for each point. a number 1 to 16 and one of the enum options.
enum class PayloadABType { PositionA, PositionB, Error };


class PayloadLocationType
{
public:
	PayloadLocationType(): Packet(0),Position(PayloadABType::Error) {}
	PayloadLocationType(uint8_t packet, PayloadABType position): Packet(packet), Position(position) {}

	uint8_t Packet; // Should be 1 to 16. 0 is an error - or un-initialised.
	PayloadABType Position;
	std::string to_string() const
	{
		return std::to_string(Packet) + ((Position == PayloadABType::PositionA) ? "A" : (Position == PayloadABType::PositionB) ? "B" : "Error");
	}
};

class CBPoint // Abstract Class
{
	//	NONE              = 0,
	//	ONLINE            = 1<<0,
	//	RESTART           = 1<<1,
	//	COMM_LOST         = 1<<2,
	//	REMOTE_FORCED     = 1<<3,
	//	LOCAL_FORCED      = 1<<4,
	//	OVERRANGE         = 1<<5,
	//	REFERENCE_ERR     = 1<<6,
	//	ROLLOVER          = 1<<7,
	//	DISCONTINUITY     = 1<<8,
	//	CHATTER_FILTER    = 1<<9

public:
	CBPoint() {}

	// We need these as we DO NOT want to copy the mutex!
	CBPoint(const CBPoint &src):
		Index(src.Index),
		Group(src.Group),
		Channel(src.Channel),
		PayloadLocation(src.PayloadLocation),
		ChangedTime(src.ChangedTime),
		HasBeenSet(src.HasBeenSet)
	{}
	virtual ~CBPoint(); // Make the base class pure virtual

	CBPoint& operator=(const CBPoint& src)
	{
		Index = src.Index;
		Group = src.Group;
		Channel = src.Channel;
		PayloadLocation = src.PayloadLocation;
		ChangedTime = src.ChangedTime;
		HasBeenSet = src.HasBeenSet;
		return *this;
	}

	CBPoint(uint32_t index, uint8_t moduleaddress, uint8_t channel, CBTime changedtime, PayloadLocationType PayloadLocation):
		Index(index),
		Group(moduleaddress),
		Channel(channel),
		PayloadLocation(PayloadLocation),
		ChangedTime(changedtime)
	{}

	// These first 4 never change, so no protection
	uint32_t GetIndex() { return Index; }
	uint8_t GetGroup() { return Group; }
	uint8_t GetChannel() { return Channel; } // The bit position in the payload.
	PayloadLocationType GetPayloadLocation() { return PayloadLocation; }

	CBTime GetChangedTime() { std::unique_lock<std::mutex> lck(PointMutex); return ChangedTime; }
	bool GetHasBeenSet() { std::unique_lock<std::mutex> lck(PointMutex); return HasBeenSet; }
	//	void SetChangedTime(const CBTime &ct) { std::unique_lock<std::mutex> lck(PointMutex); ChangedTime = ct; }; // Not needed...

protected:
	std::mutex PointMutex;
	uint32_t Index = 0;
	uint8_t Group = 0;
	uint8_t Channel = 0;
	PayloadLocationType PayloadLocation;         // Defaults to error value
	CBTime ChangedTime = static_cast<CBTime>(0); // msec since epoch. 1970,1,1 Only used for Fn9 and 11 queued data. TimeStamp is Uint48_t, CB is uint64_t but does not overflow.
	bool HasBeenSet = false;                     // To determine if we have been set since startup
	//TODO: Point quality to RESTART instead of HasBeenSet = false.
	//TODO: Integrate ODC quality instead of HasBeenSet flag and Module failed flag.
};


class CBBinaryPoint: public CBPoint
{
public:
	CBBinaryPoint() {}
	CBBinaryPoint(const CBBinaryPoint &src): CBPoint(src),
		Binary(src.Binary),
		Changed(src.Changed),
		MomentaryChangeStatus(src.MomentaryChangeStatus),
		PointType(src.PointType),
		SOEPoint(src.SOEPoint),
		SOEIndex(src.SOEIndex)
		// Any additions here make sure the operator= method is updated as well!!!
	{}
	// Needed for the templated Queue
	CBBinaryPoint& operator=(const CBBinaryPoint& src);

	CBBinaryPoint(uint32_t index, uint8_t group, uint8_t channel, PayloadLocationType payloadlocation, BinaryPointType pointtype, bool soepoint, uint8_t soeindex):
		CBPoint(index, group, channel, static_cast<CBTime>(0), payloadlocation),
		PointType(pointtype),
		SOEPoint(soepoint),
		SOEIndex(soeindex)
	{}

	CBBinaryPoint(uint32_t index, uint8_t group, uint8_t channel, PayloadLocationType payloadlocation, BinaryPointType pointtype,
		uint8_t binval, bool changed, CBTime changedtime, bool soepoint, uint8_t soeindex):
		CBPoint(index, group, channel, changedtime, payloadlocation),
		Binary(binval),
		Changed(changed),
		PointType(pointtype),
		SOEPoint(soepoint),
		SOEIndex(soeindex)
	{}


	~CBBinaryPoint();

	BinaryPointType GetPointType() const { return PointType; }
	std::string GetPointTypeName() const
	{
		switch (PointType)
		{
			case DIG: return "DIG";
			case MCA: return "MCA";
			case MCB: return "MCB";
			case MCC: return "MCC";
			case BINCONTROL: return "BINCONTROL";
		}
		return "UNKNOWN";
	}

	uint8_t GetBinary() { std::unique_lock<std::mutex> lck(PointMutex); return Binary; }

	bool GetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); return Changed; }
	void SetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); Changed = true; }
	bool GetAndResetChangedFlag() { std::unique_lock<std::mutex> lck(PointMutex); bool res = Changed; Changed = false; return res; }

	void GetBinaryAndMCFlagWithFlagReset(uint8_t &result, bool &MCS) { std::unique_lock<std::mutex> lck(PointMutex); result = Binary; MCS = MomentaryChangeStatus; Changed = false; MomentaryChangeStatus = false; }
	bool GetIsSOE() const { return SOEPoint;  }
	uint8_t GetSOEIndex() const { return SOEIndex; }

	void SetBinary(const uint8_t &b, const CBTime &ctime)
	{
		std::unique_lock<std::mutex> lck(PointMutex);
		HasBeenSet = true;
		ChangedTime = ctime;

		if ((PointType == MCA) && (Binary == 1) && (b == 0)) MomentaryChangeStatus = true;  // Only set on 1-->0 transition
		if ((PointType == MCB) && (Binary == 0) && (b == 1)) MomentaryChangeStatus = true;  // Only set on 0-->1 transition
		if ((PointType == MCC) && (Changed) && (Binary != b)) MomentaryChangeStatus = true; // The normal changed flag was already set, and then we got another change.

		Changed = (Binary != b);
		Binary = b;
	}
	void SetChanged(const bool &c) { std::unique_lock<std::mutex> lck(PointMutex); Changed = c; }

protected:
	// Only the values below will be changed in two places
	uint8_t Binary = 0x01;
	bool Changed = true;
	bool MomentaryChangeStatus = false; // Used only for MCA, MCB and MCC types. Not valid for other types.
	BinaryPointType PointType = DIG;
	bool SOEPoint = false;
	uint8_t SOEIndex = 0; // From 0 to 120 (7 bits), per group. The Bottom 3 bits of the group
};

class CBAnalogCounterPoint: public CBPoint
{
public:
	CBAnalogCounterPoint() {}

	CBAnalogCounterPoint(uint32_t index, uint8_t group, uint8_t channel, PayloadLocationType payloadlocation, AnalogCounterPointType pointtype): CBPoint(index, group, channel, static_cast<CBTime>(0), payloadlocation),
		PointType(pointtype)
	{}
	~CBAnalogCounterPoint();

	AnalogCounterPointType GetPointType() const { return PointType; }

	uint16_t GetAnalog() { std::unique_lock<std::mutex> lck(PointMutex); return Analog; }
	uint16_t GetAnalogAndHasBeenSet(bool &hasbeenset) { std::unique_lock<std::mutex> lck(PointMutex); hasbeenset = HasBeenSet; return Analog;}
	uint16_t GetAnalogAndDeltaAndHasBeenSet(int &delta, bool &hasbeenset) { std::unique_lock<std::mutex> lck(PointMutex); delta = static_cast<int>(Analog) - static_cast<int>(LastReadAnalog); LastReadAnalog = Analog; hasbeenset = HasBeenSet; return Analog; }

	void SetAnalog(const uint16_t & a, const CBTime &ctime) { std::unique_lock<std::mutex> lck(PointMutex); HasBeenSet = true; Analog = a; ChangedTime = ctime; }
	void ResetAnalog() { std::unique_lock<std::mutex> lck(PointMutex); HasBeenSet = false; Analog = MISSINGVALUE; ChangedTime = 0; }
	void SetLastReadAnalog(const uint16_t & a) { std::unique_lock<std::mutex> lck(PointMutex); LastReadAnalog = a; }

protected:
	uint16_t Analog = MISSINGVALUE;
	uint16_t LastReadAnalog = MISSINGVALUE;
	AnalogCounterPointType PointType = ANA;
};

typedef std::map<size_t, std::shared_ptr<CBBinaryPoint>>::iterator ODCBinaryPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<CBBinaryPoint>>::iterator CBBinaryPointMapIterType;
typedef std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>>::iterator ODCAnalogCounterPointMapIterType;
typedef std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>>::iterator CBAnalogCounterPointMapIterType;
typedef std::map<uint8_t, uint16_t> ModuleMapType;


class CBPollGroup
{
public:
	CBPollGroup():
		ID(0),
		pollrate(0),
		group(0),
		polltype(Scan),
		ForceUnconditional(false)
	{ }

	CBPollGroup(uint32_t ID_, uint32_t pollrate_, PollGroupType polltype_, uint8_t group_, bool forceunconditional):
		ID(ID_),
		pollrate(pollrate_),
		group(group_),
		polltype(polltype_),
		ForceUnconditional(forceunconditional)
	{ }

	uint32_t ID;
	uint32_t pollrate;
	uint8_t group;
	PollGroupType polltype;
	bool ForceUnconditional; // Used to prevent use of delta or COS commands.
};

#endif
