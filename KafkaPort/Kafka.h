/*	opendatacon
*
*	Copyright (c) 2019:
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
* Kafka.h
*
*  Created on: 16/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef Kafka_H_
#define Kafka_H_

// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
//#define NONVSTESTING

#include <cstdint>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>


// Hide some of the code to make Logging cleaner
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("KafkaPort")) \
		log->debug(__VA_ARGS__);
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("KafkaPort")) \
		log->error(__VA_ARGS__);
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("KafkaPort"))  \
		log->warn(__VA_ARGS__);
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("KafkaPort")) \
		log->info(__VA_ARGS__);

void CommandLineLoggingSetup();
void CommandLineLoggingCleanup();

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;

typedef uint64_t KafkaTime; // msec since epoch, utc, most time functions are uint64_t

KafkaTime KafkaNow();

const KafkaTime KafkaTimeOneDay = 1000 * 60 * 60 * 24;
const KafkaTime KafkaTimeOneHour = 1000 * 60 * 60;

// Give us the time for the start of the current day in KafkaTime (msec since epoch)
inline KafkaTime GetDayStartTime(KafkaTime time)
{
	KafkaTime Days = time / KafkaTimeOneDay;
	return (Days * KafkaTimeOneDay); // msec*sec*min*hours
}
// Give us the time of day value in msec
inline KafkaTime GetTimeOfDayOnly(KafkaTime time)
{
	return time - GetDayStartTime(time);
}

// Get the hour value for the current time value 0-23
inline KafkaTime GetHour(KafkaTime time)
{
	return time / KafkaTimeOneHour % 24;
}
// We use for signed/unsigned conversions, where we know we will not have problems.
// Static casting all over the place still produces a lot of gcc warning messages.
// Also we can put checks in here if required.
template <class OT, class ST>
OT numeric_cast(const ST value)
{
	return static_cast<OT>(value);
}

// Quality Flags
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

enum PointType { Binary, Analog };
static std::string GetKafkaKey(std::string & EQType, PointType pt, size_t ODCIndex) { return EQType + ((pt == Binary) ? "|BIN|" : "|ANA|") + std::to_string(ODCIndex); }
static std::string GetPointTypeString(PointType pt) { return (pt == PointType::Binary) ? "Binary" : "Analog"; }

#endif
