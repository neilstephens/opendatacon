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
#include "LogToStdioAdv.h"
#include <asiodnp3/ConsoleLogger.h>

openpal::ILogHandler& LogToStdioAdv::console = asiodnp3::ConsoleLogger::Instance();

void LogToStdioAdv::Log(const openpal::LogEntry& arEntry)
{
	bool DoLog = true;
	openpal::LogEntry ToPrint(arEntry);
	for (MessageCount& reg : IgnoreRepeats)
	{
		if(std::regex_match(arEntry.GetMessage(), reg.LogRegex))
		{
			if (reg.Count > 0 && (reg.Decimate == 0 || (reg.Count%reg.Decimate) != 0) && (reg.IgnoreDuration.GetMilliseconds() == 0 || asiopal::UTCTimeSource::Instance().Now().msSinceEpoch < reg.PrintTime.msSinceEpoch+reg.IgnoreDuration.GetMilliseconds()))
			{
				DoLog = false;
			}
			++(reg.Count);
			if (reg.Count == 1 || (DoLog == true && (reg.Decimate > 0 || reg.IgnoreDuration.GetMilliseconds() > 0)))
			{
				std::string message = std::to_string(reg.Count)+" instance(s) of message - ignoring subsequent "+((reg.Decimate > 0)?std::to_string(reg.Decimate-1)+" ":"")+"similar messages"
						+((reg.IgnoreDuration.GetMilliseconds() > 0)?"for "+std::to_string(reg.IgnoreDuration.GetMilliseconds()/60000)+" minutes ":"") +":\n"+arEntry.GetMessage();
				ToPrint = openpal::LogEntry(arEntry.GetAlias(), arEntry.GetFilters(), arEntry.GetLocation(),message.c_str(),arEntry.GetErrorCode());
				reg.PrintTime = asiopal::UTCTimeSource::Instance().Now();
			}
		}
	}
	if (DoLog) console.Log(ToPrint);
}

void LogToStdioAdv::AddIngoreMultiple(const std::string& str)
{
	try
	{
		IgnoreRepeats.push_back(MessageCount(str,0));
	}
	catch(std::exception& e)
	{
		std::cout<<e.what()<<std::endl;
		return;
	}
}

void LogToStdioAdv::AddIngoreAlways(const std::string& str)
{
	try
	{
		IgnoreRepeats.push_back(MessageCount(str,1));
	}
	catch(std::exception& e)
	{
		std::cout<<e.what()<<std::endl;
		return;
	}
}

void LogToStdioAdv::AddIngoreDecimate(const std::string& str, int decimate)
{
	try
	{
		IgnoreRepeats.push_back(MessageCount(str,0, decimate));
	}
	catch(std::exception& e)
	{
		std::cout<<e.what()<<std::endl;
		return;
	}
}
void LogToStdioAdv::AddIngoreDuration(const std::string& str, openpal::TimeDuration ignore_duration)
{
	try
	{
		IgnoreRepeats.push_back(MessageCount(str,0, ignore_duration));
	}
	catch(std::exception& e)
	{
		std::cout<<e.what()<<std::endl;
		return;
	}
}
void LogToStdioAdv::RemoveIgnore(const std::string& str)
{
	for(auto ignored = IgnoreRepeats.begin(); ignored != IgnoreRepeats.end(); ignored++)
	{
		if(ignored->LogRegex_string == str)
		{
			IgnoreRepeats.erase(ignored);
			return;
		}
	}
	std::cout<<"Ignore regex '"<<str<<"' not found - not removing anything"<<std::endl;
}
void LogToStdioAdv::ShowIgnored()
{
	for(auto ignored : IgnoreRepeats)
	{
		std::cout<<ignored.LogRegex_string<<"\t\t silenced "<<ignored.Count<<" messages"<<std::endl;
	}
}
