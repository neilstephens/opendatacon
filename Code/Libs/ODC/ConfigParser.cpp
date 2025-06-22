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

std::unordered_map<std::string,std::shared_ptr<Json::Value>> ConfigParser::JSONFileCache;

ConfigParser::ConfigParser(const std::string& aConfFilename, const Json::Value& aConfOverrides):
	ConfFilename(aConfFilename),
	ConfOverrides(aConfOverrides)
{}

void ConfigParser::ProcessInherits(const std::string& FileName)
{
	auto pJSONRoot = RecallOrCreate(FileName);
	if(pJSONRoot)
	{
		if(pJSONRoot->isMember("Inherits") && (*pJSONRoot)["Inherits"].isArray())
			for(Json::ArrayIndex n=0; n<(*pJSONRoot)["Inherits"].size(); n++)
				ProcessInherits((*pJSONRoot)["Inherits"][n].asString());
		ProcessElements(*pJSONRoot);
	}
}

void ConfigParser::ProcessFile()
{
	try
	{
		if(!ConfFilename.empty())
			ProcessInherits(ConfFilename);
	}
	catch(const std::exception& e)
	{
		std::string msg("Exception processing configuration file '" + ConfFilename + "': " + e.what());
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error(msg);
		else
			std::cerr << "ERROR: " << msg << std::endl;
		throw std::runtime_error(msg);
	}

	try
	{
		if(!ConfOverrides.isNull())
			ProcessElements(ConfOverrides);
	}
	catch(const std::exception& e)
	{
		std::string msg("Exception processing configuration overrides: '"+ ConfOverrides.toStyledString() +"' :" + e.what());
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error(msg);
		else
			std::cerr << "ERROR: " << msg << std::endl;
		throw std::runtime_error(msg);
	}
}

//static
std::shared_ptr<const Json::Value> ConfigParser::RecallOrCreate(const std::string& FileName)
{
	if(FileName.empty())
		return std::make_shared<Json::Value>();
	if(JSONFileCache.find(FileName) == JSONFileCache.end()) //not cached - read it in
	{
		std::ifstream fin(FileName);
		if (fin.fail())
		{
			std::string msg("Config file " + FileName + " open fail.");
			if(auto log = odc::spdlog_get("opendatacon"))
				log->error(msg);
			else
				std::cerr << "ERROR: " << msg << std::endl;
			return std::make_shared<Json::Value>();
		}
		Json::CharReaderBuilder JSONReader;
		std::string err_str;
		JSONFileCache[FileName] = std::make_shared<Json::Value>();
		bool parse_success = Json::parseFromStream(JSONReader,fin, JSONFileCache[FileName].get(), &err_str);
		if (!parse_success)
		{
			std::string msg("Failed to parse configuration from '" + FileName + "' : " + err_str);
			if(auto log = odc::spdlog_get("opendatacon"))
				log->error(msg);
			else
				std::cerr << "ERROR: " << msg <<std::endl;
			return std::make_shared<Json::Value>();
		}
	}
	return JSONFileCache[FileName];
}

//static
void ConfigParser::AddInherits(Json::Value& JSONRoot, const Json::Value& Inherits)
{
	for(auto& InheritFile : Inherits)
	{
		auto Filename = InheritFile.asString();
		JSONRoot[Filename] = *RecallOrCreate(Filename);
		if (JSONRoot[Filename].isMember("Inherits"))
			AddInherits(JSONRoot, JSONRoot[Filename]["Inherits"]);
	}
}

//static
Json::Value ConfigParser::GetConfiguration(const std::string& aConfFilename, const Json::Value& aConfOverrides)
{
	Json::Value JSONRoot;
	JSONRoot[aConfFilename] = *RecallOrCreate(aConfFilename);
	if(JSONRoot[aConfFilename].isMember("Inherits"))
	{
		AddInherits(JSONRoot, JSONRoot[aConfFilename]["Inherits"]);
	}
	JSONRoot["ConfOverrides"] = aConfOverrides;

	return JSONRoot;
}

Json::Value ConfigParser::GetConfiguration() const
{
	return GetConfiguration(ConfFilename, ConfOverrides);
}
