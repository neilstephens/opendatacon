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
  *  Created on:
  *      Author: Scott Ellis
  */

#include <regex>
#include <algorithm>
#include <memory>
#include <map>
#include "MD3PointConf.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

MD3PointConf::MD3PointConf(std::string FileName, const Json::Value& ConfOverrides) :
	ConfigParser(FileName, ConfOverrides)
{
	ProcessFile();
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if (!JSONRoot.isObject()) return;

	// Root level Configuration values

	if (JSONRoot.isMember("Analogs"))
	{
		const auto BinaryControls = JSONRoot["Analogs"];
		ProcessPoints(BinaryControls, AnalogMD3PointMap, AnalogODCPointMap);
	}

	if (JSONRoot.isMember("Binaries"))
	{
		const auto BinaryControls = JSONRoot["Binaries"];
		ProcessPoints(BinaryControls, BinaryMD3PointMap, BinaryODCPointMap);
	}

	if (JSONRoot.isMember("BinaryControls"))
	{
		const auto BinaryControls = JSONRoot["BinaryControls"];
		ProcessPoints(BinaryControls, BinaryControlMD3PointMap, BinaryControlODCPointMap);
	}
}

void MD3PointConf::ProcessPoints(const Json::Value& JSONNode, std::map<uint16_t, std::shared_ptr<MD3Point>> &MD3PointMap,
																std::map<uint32_t, std::shared_ptr<MD3Point>> &ODCPointMap)
{
	for (Json::ArrayIndex n = 0; n < JSONNode.size(); ++n)
	{
		bool error = false;

		size_t start, stop;
		if (JSONNode[n].isMember("Index"))
		{
			start = stop = JSONNode[n]["Index"].asUInt();
		}
		else if (JSONNode[n]["Range"].isMember("Start") && JSONNode[n]["Range"].isMember("Stop"))
		{
			start = JSONNode[n]["Range"]["Start"].asUInt();
			stop = JSONNode[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			start = 1;
			stop = 0;
			error = true;
		}

		uint32_t module = 0;
		uint32_t offset = 0;

		if (JSONNode[n].isMember("Module"))
			module = JSONNode[n]["Module"].asUInt();
		else
		{
			std::cout << "A point needs an \"Module\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (JSONNode[n].isMember("Offset"))
			offset = JSONNode[n]["Offset"].asUInt();
		else
		{
			std::cout << "A point needs an \"Offset\" : '" << JSONNode[n].toStyledString() << "'" << std::endl;
			error = true;
		}

		if (!error)
		{
			for (auto index = start; index <= stop; index++)
			{
				uint16_t moduleaddress = module + (index - start + offset) / 16;
				uint8_t channel = (uint8_t)((offset + (index-start)) % 16);

				uint16_t md3index = (moduleaddress << 8) | channel;

				if (MD3PointMap.find(md3index) != MD3PointMap.end())
				{
					std::cout << "Duplicate MD3 Index '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else if (ODCPointMap.find(index) != ODCPointMap.end())
				{
					std::cout << "Duplicate ODC Index : '" << JSONNode[n].toStyledString() << "'" << std::endl;
				}
				else
				{
					auto pt = std::make_shared<MD3Point>(index, moduleaddress, channel);
					MD3PointMap[md3index] = pt;
					ODCPointMap[index] = pt;
				}
			}
		}
	}
}



