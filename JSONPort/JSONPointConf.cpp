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

JSONPointConf::JSONPointConf(std::string FileName, const Json::Value &ConfOverrides):
	ConfigParser(FileName, ConfOverrides),
	pJOT(nullptr)
{
	ProcessFile();
}

inline bool check_index(const Json::Value& Point)
{
	if(!Point.isMember("Index"))
	{
		std::cout<<"A point needs an \"Index\" : '"<<Point.toStyledString()<<"'"<<std::endl;
		return false;
	}
	return true;
}
void JSONPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject())
		return;

	this->TimestampPath = JSONRoot["TimestampPath"];

	auto ind_marker = JSONRoot["TemplateIndex"].isString() ? JSONRoot["TemplateIndex"].asString() : "<INDEX>";
	auto name_marker = JSONRoot["TemplateName"].isString() ? JSONRoot["TemplateName"].asString() : "<NAME>";
	auto val_marker = JSONRoot["TemplateValue"].isString() ? JSONRoot["TemplateValue"].asString() : "<VALUE>";
	auto qual_marker = JSONRoot["TemplateQuality"].isString() ? JSONRoot["TemplateQuality"].asString() : "<QUALITY>";
	auto time_marker = JSONRoot["TemplateTimestamp"].isString() ? JSONRoot["TemplateTimestamp"].asString() : "<TIMESTAMP>";
	if(JSONRoot.isMember("OutputTemplate"))
	{
		pJOT.reset(new JSONOutputTemplate(JSONRoot["OutputTemplate"],ind_marker,name_marker,val_marker,qual_marker,time_marker));
	}
	else
	{
		Json::Value temp;
		temp["Index"] = ind_marker;
		temp["Name"] = name_marker;
		temp["Value"] = val_marker;
		temp["Quality"] = qual_marker;
		temp["Timestamp"] = time_marker;
		pJOT.reset(new JSONOutputTemplate(temp,ind_marker,name_marker,val_marker,qual_marker,time_marker));
	}

	const Json::Value PointConfs = JSONRoot["JSONPointConf"];
	for (Json::ArrayIndex n = 0; n < PointConfs.size(); ++n) // Iterates over the sequence of point groups (grouped by type).
	{
		std::string PointType = PointConfs[n]["PointType"].asString();
		if(PointType == "Analog")
		{
			for (Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				if(!check_index(PointConfs[n]["Points"][k]))
					continue;
				this->Analogs[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else if(PointType == "Binary")
		{
			for(Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				if(!check_index(PointConfs[n]["Points"][k]))
					continue;
				this->Binaries[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else if(PointType == "Control")
		{
			for(Json::ArrayIndex k = 0; k < PointConfs[n]["Points"].size(); ++k)
			{
				if(!check_index(PointConfs[n]["Points"][k]))
					continue;
				this->Controls[PointConfs[n]["Points"][k]["Index"].asUInt()] = PointConfs[n]["Points"][k];
			}
		}
		else
		{
			std::cout<<"Ignoring unrecognised PointType '"<<PointType<<"' : from '"<<PointConfs[n].toStyledString()<<"'"<<std::endl;
		}
	}
	return;
}
