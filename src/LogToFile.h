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
#pragma once

#include <openpal/logging/ILogHandler.h>
#include <chrono>
#include <mutex>
#include <fstream>

class LogToFile: public openpal::ILogHandler
{

public:
	LogToFile(std::string log_name):
		mPrintLocation(false),
		mLogName(log_name),
		mNumFiles(5),
		mFileSizekB(5*1024),
		mFileIndex(0)
	{}
	LogToFile(std::string log_name,size_t file_size_kb, size_t num_files):
		mPrintLocation(false),
		mLogName(log_name),
		mNumFiles(num_files),
		mFileSizekB(file_size_kb),
		mFileIndex(0)
	{}

	void Log( const openpal::LogEntry& arEntry ) override;
	void SetPrintLocation(bool print_loc);

	void SetLogFileSizekB(size_t kB);
	void SetNumLogFiles(size_t num);
	void SetLogName(std::string name);
	size_t GetLogFileSizekB();
	size_t GetNumLogFiles();
	std::string GetLogName();

private:
	static std::string FilterToString(const openpal::LogFilters& filters);

	bool mPrintLocation;
	std::string mLogName;
	size_t mNumFiles;
	size_t mFileSizekB;
	size_t mFileIndex;
	std::ofstream mLogFile;
	std::mutex mMutex;
};
