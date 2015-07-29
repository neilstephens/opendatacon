/*	opendatacon
 *
 *	Copyright (c) 2014:
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
#include "LogToFile.h"

#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <map>
#include <opendnp3/LogLevels.h>

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

	high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
	milliseconds ms = duration_cast<milliseconds>(time.time_since_epoch());
	seconds s = duration_cast<seconds>(ms);
	std::time_t t = s.count();
	std::size_t fractional_seconds = ms.count() % 1000;
	auto local_time = platformtime::localtime(&t);
	char time_formatted[25];
	std::strftime(time_formatted, 25, "%Y-%m-%d %H:%M:%S", &local_time);
	return std::string(time_formatted)+"."+std::to_string(fractional_seconds);
}
}

void LogToFile::Log( const openpal::LogEntry& arEntry )
{
	std::string time_str = platformtime::time_string();

	std::lock_guard<std::mutex> get_lock(mMutex);
	if(!mLogFile.is_open())
	{
		mLogFile.open(mLogName+std::to_string(mFileIndex)+".txt");
		if(mLogFile.fail())
			throw std::runtime_error("Failed to open log file");
	}

	mLogFile <<time_str<<" - "<< FilterToString(arEntry.GetFilters())<<" - "<<arEntry.GetAlias();
	if(mPrintLocation && !arEntry.GetLocation())
		mLogFile << " - " << arEntry.GetLocation();
	mLogFile << " - " << arEntry.GetMessage();
	if(arEntry.GetErrorCode() != -1)
		mLogFile << " - " << arEntry.GetErrorCode();
	mLogFile <<std::endl;

	if (mLogFile.tellp() > (int)mFileSizekB*1024)
	{
		mLogFile.close();
		mFileIndex++;
		mFileIndex %= mNumFiles;
	}
}

std::string LogToFile::FilterToString(const openpal::LogFilters& filters)
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

void LogToFile::SetLogFileSizekB(size_t kB)
{
	mFileSizekB = kB;
}
void LogToFile::SetNumLogFiles(size_t num)
{
	mNumFiles = num;
}
void LogToFile::SetLogName(std::string name)
{
	mLogName = name;
	mFileIndex = 0;
}
size_t LogToFile::GetLogFileSizekB()
{
	return mFileSizekB;
}
size_t LogToFile::GetNumLogFiles()
{
	return mNumFiles;
}
std::string LogToFile::GetLogName()
{
	return mLogName;
}

void LogToFile::SetPrintLocation(bool print_loc)
{
	mPrintLocation = print_loc;
}

