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
 * IOTypeWrappers.h
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */

#ifndef IOTYPEWRAPPERS_H
#define IOTYPEWRAPPERS_H

#include "include/Lua/CLua.h"
#include <opendatacon/IOTypes.h>

void ExportEventTypes(lua_State* const L);
void ExportQualityFlags(lua_State* const L);
void ExportCommandStatus(lua_State* const L);
void ExportControlCodes(lua_State* const L);
void ExportConnectStates(lua_State* const L);
void ExportPayloadFactory(lua_State* const L);
void ExportToStringFunctions(lua_State* const L);

void PushPayload(lua_State* const L, odc::EventType evt_type, std::shared_ptr<const odc::EventInfo> event = nullptr);
void PushPayload(lua_State* const L, bool payload);
void PushPayload(lua_State* const L, odc::DBB payload);
void PushPayload(lua_State* const L, double payload);
void PushPayload(lua_State* const L, uint32_t payload);
void PushPayload(lua_State* const L, odc::CommandStatus payload);
void PushPayload(lua_State* const L, odc::OctetStringBuffer payload);
void PushPayload(lua_State* const L, odc::TAI payload);
void PushPayload(lua_State* const L, odc::SS payload);
void PushPayload(lua_State* const L, odc::ControlRelayOutputBlock payload);
void PushPayload(lua_State* const L, odc::AO16 payload);
void PushPayload(lua_State* const L, odc::AO32 payload);
void PushPayload(lua_State* const L, odc::AOF payload);
void PushPayload(lua_State* const L, odc::AOD payload);
void PushPayload(lua_State* const L, odc::QualityFlags payload);
void PushPayload(lua_State* const L, char payload);
void PushPayload(lua_State* const L, odc::ConnectState payload);

void PushEventInfo(lua_State* const L, std::shared_ptr<const odc::EventInfo> event);
std::shared_ptr<odc::EventInfo> EventInfoFromLua(lua_State* const L, const std::string& Name, const std::string& LogName, int idx);

void PopPayload(lua_State* const L, std::shared_ptr<odc::EventInfo> event);
template <typename T> T PopPayload(lua_State* const L);

#endif // IOTYPEWRAPPERS_H
