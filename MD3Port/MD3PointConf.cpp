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
 /*
  * MD3PointConf.cpp
  *
  *  Created on: 16/10/2014
  *      Author: Alan Murray
  */

#include <regex>
#include <algorithm>
#include "MD3PointConf.h"
#include "MD3PortConf.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

MD3PointConf::MD3PointConf(std::string FileName) :
	ConfigParser(FileName)
{
	ProcessFile();
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values
	if (JSONRoot.isMember("LinkNumRetry"))
		LinkNumRetry = JSONRoot["LinkNumRetry"].asUInt();
	if (JSONRoot.isMember("LinkTimeoutms"))
		LinkTimeoutms = JSONRoot["LinkTimeoutms"].asUInt();

	if (JSONRoot.isMember("Analogs"))
	{
		ProcessAnalogs(JSONRoot);
	}

	if (JSONRoot.isMember("Binaries"))
	{
		ProcessBinaries(JSONRoot);
	}

	if (JSONRoot.isMember("BinaryControls"))
	{
		ProcessBinaryControls(JSONRoot);
	}
}

void MD3PointConf::ProcessBinaryControls(const Json::Value& JSONRoot)
{
	const auto BinaryControls = JSONRoot["BinaryControls"];
	for (Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
	{
		size_t start, stop;
		if (BinaryControls[n].isMember("Index"))
			start = stop = BinaryControls[n]["Index"].asUInt();
		else if (BinaryControls[n]["Range"].isMember("Start") && BinaryControls[n]["Range"].isMember("Stop"))
		{
			start = BinaryControls[n]["Range"]["Start"].asUInt();
			stop = BinaryControls[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << BinaryControls[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
		}
		for (auto index = start; index <= stop; index++)
		{
			bool exists = false;
			for (auto existing_index : ControlIndicies)
				if (existing_index == index)
					exists = true;

			if (!exists)
				ControlIndicies.push_back(index);
		}
	}
	std::sort(ControlIndicies.begin(), ControlIndicies.end());
}

void MD3PointConf::ProcessBinaries(const Json::Value& JSONRoot)
{
	const auto Binaries = JSONRoot["Binaries"];
	for (Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
	{
		size_t start, stop;
		if (Binaries[n].isMember("Index"))
			start = stop = Binaries[n]["Index"].asUInt();
		else if (Binaries[n]["Range"].isMember("Start") && Binaries[n]["Range"].isMember("Stop"))
		{
			start = Binaries[n]["Range"]["Start"].asUInt();
			stop = Binaries[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << Binaries[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
		}
		for (auto index = start; index <= stop; index++)
		{

			bool exists = false;
			for (auto existing_index : BinaryIndicies)
				if (existing_index == index)
					exists = true;

			if (!exists)
				BinaryIndicies.push_back(index);
		}
	}
	std::sort(BinaryIndicies.begin(), BinaryIndicies.end());
}

void MD3PointConf::ProcessAnalogs(const Json::Value& JSONRoot)
{
	const auto Analogs = JSONRoot["Analogs"];
	for (Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
	{
		size_t start, stop;
		if (Analogs[n].isMember("Index"))
			start = stop = Analogs[n]["Index"].asUInt();
		else if (Analogs[n]["Range"].isMember("Start") && Analogs[n]["Range"].isMember("Stop"))
		{
			start = Analogs[n]["Range"]["Start"].asUInt();
			stop = Analogs[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << Analogs[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
		}
		for (auto index = start; index <= stop; index++)
		{
			bool exists = false;
			for (auto existing_index : AnalogIndicies)
				if (existing_index == index)
					exists = true;

			if (!exists)
				AnalogIndicies.push_back(index);
		}
	}
	std::sort(AnalogIndicies.begin(), AnalogIndicies.end());
}



