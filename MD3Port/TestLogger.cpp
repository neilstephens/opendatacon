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
* TEstLogger.cpp
*
*  Created on: 06/06/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/



#include <sstream>
#include <ctime>
#include <opendnp3/LogLevels.h>
#include "TestLogger.h"

namespace platformtime
{
	tm localtime(const std::time_t* time)
	{
		std::tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
		localtime_s(&tm_snapshot, time);
#else
		localtime_r(time, &tm_snapshot); // POSIX
#endif
		return tm_snapshot;
	}
	std::string time_string()
	{
		using namespace std::chrono;

		// For windows systems the time_since_epoch for high_resolution_clock is the time since it booted up. Linux is since the "normal" epoch.
		// From StackOverflow
		// There is no de facto standard for high_resolution_clock.
		// On gcc high_resolution_clock is a typedef for system_clock, and so on gcc platforms, you'll notice perfect synchronization.
		// On VS and libc++ high_resolution_clock is a typedef for steady_clock - this is spcial as it cannot go backwards.
		// So just use system_clock
		//TODO: SJE make sure system_clock change makes it back into LogToFile.cpp

		system_clock::time_point time = std::chrono::system_clock::now();

		milliseconds ms = duration_cast<milliseconds>(time.time_since_epoch());
		seconds s = duration_cast<seconds>(ms);
		std::time_t t = s.count();
		std::size_t fractional_seconds = ms.count() % 1000;
		auto local_time = platformtime::localtime(&t);
		char time_formatted[25];
		std::strftime(time_formatted, 25, "%Y-%m-%d %H:%M:%S", &local_time);
		return std::string(time_formatted) + "." + std::to_string(fractional_seconds);
	}
}
bool TestLogger::GetNextLine(std::string &str)
{
	if (readindex < LogLines.size())
	{
		str = LogLines[readindex++];
		return true;
	}
	return false;
}
void TestLogger::ResetLineReads()
{
	readindex = 0;
}
void TestLogger::Log(const openpal::LogEntry& arEntry)
{
	std::string time_str = platformtime::time_string();

	std::lock_guard<std::mutex> get_lock(mMutex);

	std::stringstream ss;

	ss << time_str << " - " << FilterToString(arEntry.GetFilters()) << " - " << arEntry.GetAlias();
	if (mPrintLocation && !arEntry.GetLocation())
		ss << " - " << arEntry.GetLocation();
	ss << " - " << arEntry.GetMessage();
	if (arEntry.GetErrorCode() != -1)
		ss << " - " << arEntry.GetErrorCode();
	ss << std::endl;

	// Add to the list of logged strings.
	LogLines.push_back(ss.str());
}

std::string TestLogger::FilterToString(const openpal::LogFilters& filters)
{
	switch (filters.GetBitfield())
	{

	case (opendnp3::flags::EVENT):
		return "EVENT";
	case (opendnp3::flags::ERR):
		return "ERROR";
	case (opendnp3::flags::WARN):
		return "WARN";
	case (opendnp3::flags::INFO):
		return "INFO";
	case (opendnp3::flags::DBG):
		return "DEBUG";
	case (opendnp3::flags::LINK_RX):
	case (opendnp3::flags::LINK_RX_HEX):
		return "<--LINK-";
	case (opendnp3::flags::LINK_TX):
	case (opendnp3::flags::LINK_TX_HEX):
		return "-LINK-->";
	case (opendnp3::flags::TRANSPORT_RX):
		return "<--TRANS-";
	case (opendnp3::flags::TRANSPORT_TX):
		return "-TRANS-->";
	case (opendnp3::flags::APP_HEADER_RX):
	case (opendnp3::flags::APP_OBJECT_RX):
		return "<--APP-";
	case (opendnp3::flags::APP_HEADER_TX):
	case (opendnp3::flags::APP_OBJECT_TX):
		return "-APP-->";
	default:
		return "UNKNOWN";
	}
}

void TestLogger::SetPrintLocation(bool print_loc)
{
	mPrintLocation = print_loc;
}

