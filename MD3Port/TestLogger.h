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
* TestLogger.h
*
*  Created on: 06/06/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef TESTLOGGER_H_
#define TESTLOGGER_H_

#include <openpal/logging/ILogHandler.h>
#include <mutex>
#include <vector>

// This is a simple logger that you pass in the output function on creation.
// Main aim is to use catchvs INFO() macros to spit log messages to the test output log - easier to see why a test fails then.
class TestLogger : public openpal::ILogHandler
{

public:
	TestLogger(int lines) :
		mLines(lines),
		mPrintLocation(false)
	{
		LogLines.reserve(lines);		// So we dont keep resizing...
	}

	void Log(const openpal::LogEntry& arEntry) override;
	void SetPrintLocation(bool print_loc);
	bool GetNextLine(std::string &str);
	void ResetLineReads();

private:
	static std::string FilterToString(const openpal::LogFilters& filters);
	std::vector<std::string> LogLines;
	std::mutex mMutex;
	bool mPrintLocation;
	int mLines;
	int readindex = 0;
};

#endif TESTLOGGER_H_
