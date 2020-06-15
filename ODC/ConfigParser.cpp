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
 * ConfigParser.cpp
 *
 *  Created on: 05/08/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <fstream>
#include <iostream>
#include <opendatacon/ConfigParser.h>
#include <opendatacon/util.h>

std::unordered_map<std::string,Json::Value> ConfigParser::JSONCache;

ConfigParser::ConfigParser(const std::string& aConfFilename, const Json::Value& aConfOverrides):
	ConfFilename(aConfFilename),
	ConfOverrides(aConfOverrides)
{}

void ConfigParser::ProcessInherits(const std::string& FileName)
{
	Json::Value* pJSONRoot;
	pJSONRoot = RecallOrCreate(FileName);
	if(pJSONRoot != nullptr)
	{
		if(!(*pJSONRoot)["Inherits"].isNull())
			for(Json::ArrayIndex n=0; n<(*pJSONRoot)["Inherits"].size(); n++)
				ProcessInherits((*pJSONRoot)["Inherits"][n].asString());
		ProcessElements(*pJSONRoot);
	}
}

void ConfigParser::ProcessFile()
{
	if(!ConfFilename.empty())
		ProcessInherits(ConfFilename);
	if(!ConfOverrides.isNull())
		ProcessElements(ConfOverrides);
}

Json::Value* ConfigParser::RecallOrCreate(const std::string& FileName)
{
	if(!(JSONCache.count(FileName))) //not cached - read it in
	{
		std::ifstream fin(FileName);
		if (fin.fail())
		{
			std::string msg("Config file " + FileName + " open fail.");
			if(auto log = odc::spdlog_get("opendatacon"))
				log->error(msg);
			else
				std::cerr << "ERROR: " << msg << std::endl;
			return nullptr;
		}
		Json::CharReaderBuilder JSONReader;
		std::string err_str;
		bool parse_success = Json::parseFromStream(JSONReader,fin, &JSONCache[FileName], &err_str);
		if (!parse_success)
		{
			std::string msg("Failed to parse configuration from '" + FileName + "' : " + err_str);
			if(auto log = odc::spdlog_get("opendatacon"))
				log->error(msg);
			else
				std::cerr << "ERROR: " << msg <<std::endl;
			return nullptr;
		}
	}
	return &JSONCache[FileName];
}

const Json::Value ConfigParser::GetConfiguration(const std::string& pFileName)
{
	if(JSONCache.count(pFileName))
	{
		return JSONCache[pFileName];
	}

	return Json::Value();
}

void ConfigParser::AddInherits(Json::Value& JSONRoot, const Json::Value& Inherits)
{
	for(auto& InheritFile : Inherits)
	{
		Json::Value InheritRoot = ConfigParser::GetConfiguration(InheritFile.asString());
		JSONRoot[InheritFile.asString()] = InheritRoot;
		if (InheritRoot.isMember("Inherits"))
		{
			AddInherits(JSONRoot, InheritRoot["Inherits"]);
		}
	}
}

const Json::Value ConfigParser::GetConfiguration() const
{
	Json::Value JSONRoot;
	JSONRoot[ConfFilename] = GetConfiguration(ConfFilename);
	if(JSONRoot[ConfFilename].isMember("Inherits"))
	{
		AddInherits(JSONRoot, JSONRoot[ConfFilename]["Inherits"]);
	}
	JSONRoot["ConfigOverrides"] = ConfOverrides;

	return JSONRoot;
}
