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
#include "ConfigParser.h"

std::unordered_map<std::string,Json::Value> ConfigParser::JSONCache;

/*
 load main config
 process each includes (recurses)
 apply main config
 apply overrides
 */

Json::Value* ConfigParser::ProcessInherit(const std::string& pFileName)
{
    Json::Value* CurrentConfig;
    if((CurrentConfig = RecallOrCreate(pFileName)))
	{
		if(!(*CurrentConfig)["Inherits"].isNull())
		{
            for(Json::Value InheritFile : (*CurrentConfig)["Inherits"])
			{
                ProcessInherit(InheritFile.asString());
			}
		}
		ProcessElements(*CurrentConfig);
	}
    return CurrentConfig;
}

void ConfigParser::ProcessFile(const std::string& pFileName, const std::string& overrides)
{
    FileName = pFileName;
    ConfigBase = ProcessInherit(FileName);
	if(overrides != "")
	{
		Json::Reader JSONReader;
		bool parse_success = JSONReader.parse(overrides,ConfigOverrides);
		if (!parse_success)
		{
			std::cout  << "Failed to parse configuration from '"<<overrides<<"' conf overrides\n"
					<< JSONReader.getFormattedErrorMessages()<<std::endl;
			return;
		}
		ProcessElements(ConfigOverrides);
	}
}

Json::Value* ConfigParser::RecallOrCreate(const std::string& pFileName)
{
	std::string Err;
	if(!(JSONCache.count(pFileName))) //not cached - read it in
	{
        Json::Value NewConfig;
		std::ifstream fin(pFileName);
		if (fin.fail())
		{
			std::cout << "WARNING: Config file " << pFileName << " open fail." << std::endl;
			return nullptr;
		}
		Json::Reader JSONReader;
		bool parse_success = JSONReader.parse(fin,NewConfig);
		if (!parse_success)
		{
			std::cout  << "Failed to parse configuration from '"<<pFileName<<"'\n"
					<< JSONReader.getFormattedErrorMessages()<<std::endl;
			return nullptr;
		}
		JSONCache[pFileName] = NewConfig;
	}
	return &JSONCache[pFileName];
}

Json::Value ConfigParser::GetConfiguration(const std::string& pFileName)
{
	if(JSONCache.count(pFileName))
    {
        return JSONCache[pFileName];
    }
    return Json::Value();
}

void ConfigParser::AddInherits(Json::Value& JSONRoot, const Json::Value& Inherits)
{
    for(Json::Value InheritFile : Inherits)
    {
        Json::Value InheritRoot = ConfigParser::GetConfiguration(InheritFile.asString());
        JSONRoot[InheritFile.asString()] = InheritRoot;
        if (!(InheritRoot["Inherits"].isNull()))
        {
            AddInherits(JSONRoot, InheritRoot["Inherits"]);
        }
    }
}

Json::Value ConfigParser::GetConfiguration() const
{
    Json::Value JSONRoot;
    if (nullptr != ConfigBase)
    {
        JSONRoot[FileName] = *ConfigBase;
		if(!(*ConfigBase)["Inherits"].isNull())
		{
            AddInherits(JSONRoot, (*ConfigBase)["Inherits"]);
		}
    }
    JSONRoot["ConfigOverrides"] = ConfigOverrides;
    
    return JSONRoot;
}
