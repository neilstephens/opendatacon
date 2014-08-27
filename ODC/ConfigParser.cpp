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

void ConfigParser::ProcessFile(std::string filename, std::string overrides)
{
	Json::Value JSONRoot;
	if(RecallOrCreate(filename,JSONRoot))
	{
		if(!JSONRoot["Inherits"].isNull())
		{
			for(Json::ArrayIndex n=0; n<JSONRoot["Inherits"].size(); n++)
			{
				Json::Value JSONInc;
				if(RecallOrCreate(JSONRoot["Inherits"][n].asString(),JSONInc))
					ProcessElements(JSONInc);
			}
		}
		ProcessElements(JSONRoot);
	}
	if(overrides != "")
	{
		Json::Reader JSONReader;
		bool parse_success = JSONReader.parse(overrides,JSONRoot);
		if (!parse_success)
		{
			std::cout  << "Failed to parse configuration from '"<<overrides<<"' conf overrides\n"
					<< JSONReader.getFormattedErrorMessages()<<std::endl;
			return;
		}
		ProcessElements(JSONRoot);
	}
}
bool ConfigParser::RecallOrCreate(std::string FileName, Json::Value& JSONRoot)
{
	std::string Err;
	if(!(JSONCache.count(FileName))) //not cached - read it in
	{
		std::ifstream fin(FileName);
		if (fin.fail())
		{
			std::cout << "WARNING: Config file " << FileName << " open fail." << std::endl;
			return false;
		}
		Json::Reader JSONReader;
		bool parse_success = JSONReader.parse(fin,JSONRoot);
		if (!parse_success)
		{
			std::cout  << "Failed to parse configuration from '"<<FileName<<"'\n"
					<< JSONReader.getFormattedErrorMessages()<<std::endl;
			return false;
		}
		JSONCache[FileName] = JSONRoot;
	}
	else
		JSONRoot = JSONCache[FileName];
	return true;
}


