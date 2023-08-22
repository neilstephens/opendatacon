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
 * IOTypesJSON.h
 *
 *  Created on: 7/7/2023
 *      Author: Neil Stephens
 */
#ifndef IOTYPESJSON_H
#define IOTYPESJSON_H

#include <json/json.h>
#include <opendatacon/IOTypes.h>

namespace odc
{

std::shared_ptr<EventInfo> EventInfoFromJson(const std::string& event_json_str);
std::shared_ptr<EventInfo> EventInfoFromJson(Json::Value JEvtInfo);
void PayloadFromJson(const Json::Value& JLoad, std::shared_ptr<EventInfo> event);

template <typename T> T PayloadFromJson(const Json::Value& JLoad);
template <> bool PayloadFromJson(const Json::Value& JLoad);
template<> DBB PayloadFromJson(const Json::Value& JLoad);
template<> double PayloadFromJson(const Json::Value& JLoad);
template<> uint32_t PayloadFromJson(const Json::Value& JLoad);
template<> CommandStatus PayloadFromJson(const Json::Value& JLoad);
template<> OctetStringBuffer PayloadFromJson(const Json::Value& JLoad);
template<> TAI PayloadFromJson(const Json::Value& JLoad);
template<> SS PayloadFromJson(const Json::Value& JLoad);
template<> ControlRelayOutputBlock PayloadFromJson(const Json::Value& JLoad);
template<> AO16 PayloadFromJson(const Json::Value& JLoad);
template<> AO32 PayloadFromJson(const Json::Value& JLoad);
template<> AOF PayloadFromJson(const Json::Value& JLoad);
template<> AOD PayloadFromJson(const Json::Value& JLoad);
template<> QualityFlags PayloadFromJson(const Json::Value& JLoad);
template<> char PayloadFromJson(const Json::Value& JLoad);
template<> ConnectState PayloadFromJson(const Json::Value& JLoad);

} //namespace odc

#endif // IOTYPESJSON_H
