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
 * JSONPointConf.h
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONPOINTCONF_H_
#define JSONPOINTCONF_H_

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>
#include <json/json.h>
#include "JSONOutputTemplate.h"

class JSONPointConf: public ConfigParser
{
public:
	JSONPointConf(const std::string& FileName, const Json::Value& ConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) override;

	std::map<uint16_t, Json::Value> Binaries;
	std::map<uint16_t, Json::Value> Analogs;
	std::map<uint16_t, Json::Value> Controls;
	std::map<uint16_t, Json::Value> AnalogControls;
	Json::Value TimestampPath;
	std::unique_ptr<JSONOutputTemplate> pJOT;
};

#endif /* JSONPOINTCONF_H_ */
