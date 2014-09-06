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
 * ConfigParser.h
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef CONFIGPARSER_H_
#define CONFIGPARSER_H_

#include <unordered_map>
#include <json/json.h>

class ConfigParser
{
public:
	ConfigParser(){};
	virtual ~ConfigParser(){};
	void ProcessFile(const std::string& filename, const std::string& overrides = "");

	virtual void ProcessElements(const Json::Value& JSONRoot)=0;
    
    static Json::Value GetConfiguration(const std::string& FileName);
    
    Json::Value GetConfiguration() const;

private:
	static Json::Value* RecallOrCreate(const std::string& FileName);
    static void AddInherits(Json::Value& JSONRoot, const Json::Value& Inherits);

    Json::Value* ProcessInherit(const std::string& pFileName);

    std::string FileName;
    Json::Value* ConfigBase;
    Json::Value ConfigOverrides;
	static std::unordered_map<std::string,Json::Value> JSONCache;
};

#endif /* CONFIGPARSER_H_ */
