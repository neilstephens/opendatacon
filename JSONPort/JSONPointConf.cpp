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
 * JSONPointConf.cpp
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <fstream>
#include <iostream>
#include <cstdint>
#include "JSONPointConf.h"

JSONPointConf::JSONPointConf(std::string FileName):
	ConfigParser(FileName)
{
	ProcessFile();
};

void JSONPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	const Json::Value PointConfs = JSONRoot["JSONPointConf"];
	for (Json::ArrayIndex n = 0; n < PointConfs.size(); ++n)  // Iterates over the sequence of point groups (grouped by type).
	{
		std::string PointType = PointConfs[n]["PointType"].asString();
		if(PointType == "Analog")
		{
			for (Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				this->Analogs[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else if(PointType == "Binary")
		{
			for(Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				this->Binaries[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else if(PointType == "Control")
		{
			for(Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				this->Binaries[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else
		{
			std::cout<<"Ignoring unrecognised PointType '"<<PointType<<"' : from '"<<PointConfs[n].toStyledString()<<"'"<<std::endl;
		}
	}
	return;
};
