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
#define NONVSTESTING

// This will make the code actually send the Kafka messages. Without  it - it processes them but does not send them.
#define DOPOST

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

void CommandLineLoggingSetup(spdlog::level::level_enum log_level);
void CommandLineLoggingCleanup();

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> pTimer_t;


// We use for signed/unsigned conversions, where we know we will not have problems.
// Static casting all over the place still produces a lot of gcc warning messages.
// Also we can put checks in here if required.
template <class OT, class ST>
OT numeric_cast(const ST value)
{
	return static_cast<OT>(value);
}

// Quality Flags use ToString(quality) to turn into a string representation.

enum PointType { Binary, Analog };
static std::string GetKafkaKey(std::string & EQType, PointType pt, size_t ODCIndex) { return EQType + ((pt == Binary) ? "|BIN|" : "|ANA|") + std::to_string(ODCIndex); }
static std::string GetPointTypeString(PointType pt) { return (pt == PointType::Binary) ? "Binary" : "Analog"; }

class KafkaEvent
{
public:
	KafkaEvent(): Key(""), Value("")
	{};
	KafkaEvent(const std::string& key, const std::string& value): Key(key), Value(value)
	{};

	std::string Key;
	std::string Value;
};
#endif  Kafka_H_
