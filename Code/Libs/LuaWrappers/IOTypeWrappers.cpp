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
 * IOTypeWrappers.cpp
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */

#include "IOTypeWrappers.h"
#include <Lua/CLua.h>
#include <opendatacon/IOTypes.h>

void ExportEventTypes(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table that has the values of each event type
	lua_newtable(L);
	auto event_type = odc::EventType::BeforeRange;
	while((event_type+1) != odc::EventType::AfterRange)
	{
		event_type = event_type+1;
		auto lua_val = static_cast< std::underlying_type_t<odc::EventType> >(event_type);
		lua_pushstring(L, ToString(event_type).c_str());
		lua_pushinteger(L, lua_val);
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"EventType");
}

void ExportQualityFlags(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table that has the values of each flag
	lua_newtable(L);
	for(const auto& qual :
	{
		odc::QualityFlags::NONE          ,
		odc::QualityFlags::ONLINE        ,
		odc::QualityFlags::RESTART       ,
		odc::QualityFlags::COMM_LOST     ,
		odc::QualityFlags::REMOTE_FORCED ,
		odc::QualityFlags::LOCAL_FORCED  ,
		odc::QualityFlags::OVERRANGE     ,
		odc::QualityFlags::REFERENCE_ERR ,
		odc::QualityFlags::ROLLOVER      ,
		odc::QualityFlags::DISCONTINUITY ,
		odc::QualityFlags::CHATTER_FILTER
	})
	{
		auto lua_val = static_cast< std::underlying_type_t<odc::QualityFlags> >(qual);
		lua_pushstring(L, SingleFlagString(qual).c_str());
		lua_pushinteger(L, lua_val);
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"QualityFlag");

	//Make a helper function for combining quality flags
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			odc::QualityFlags qual = odc::QualityFlags::NONE;
			auto argn = -1;
			while(lua_isinteger(L,argn))
				qual |= static_cast<odc::QualityFlags>(lua_tointeger(L,argn--));

			auto lua_return = static_cast< std::underlying_type_t<odc::EventType> >(qual);
			lua_pushinteger(L, lua_return);
			return 1; //number of lua ret vals pushed onto the stack
		});
	lua_setfield(L,-2,"QualityFlags");
}

void ExportCommandStatus(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table that has the values
	lua_newtable(L);
	for(const auto& cmd_stat :
	{
		odc::CommandStatus::SUCCESS ,
		odc::CommandStatus::TIMEOUT,
		odc::CommandStatus::NO_SELECT,
		odc::CommandStatus::FORMAT_ERROR,
		odc::CommandStatus::NOT_SUPPORTED,
		odc::CommandStatus::ALREADY_ACTIVE,
		odc::CommandStatus::HARDWARE_ERROR,
		odc::CommandStatus::LOCAL,
		odc::CommandStatus::TOO_MANY_OPS,
		odc::CommandStatus::NOT_AUTHORIZED,
		odc::CommandStatus::AUTOMATION_INHIBIT,
		odc::CommandStatus::PROCESSING_LIMITED,
		odc::CommandStatus::OUT_OF_RANGE,
		odc::CommandStatus::DOWNSTREAM_LOCAL,
		odc::CommandStatus::ALREADY_COMPLETE,
		odc::CommandStatus::BLOCKED,
		odc::CommandStatus::CANCELLED,
		odc::CommandStatus::BLOCKED_OTHER_MASTER,
		odc::CommandStatus::DOWNSTREAM_FAIL,
		odc::CommandStatus::NON_PARTICIPATING,
		odc::CommandStatus::UNDEFINED
	})
	{
		auto lua_val = static_cast< std::underlying_type_t<odc::CommandStatus> >(cmd_stat);
		lua_pushstring(L, ToString(cmd_stat).c_str());
		lua_pushinteger(L, lua_val);
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"CommandStatus");
}

void ExportControlCodes(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table that has the values
	lua_newtable(L);
	for(const auto& ctrl_code :
	{
		odc::ControlCode::NUL,
		odc::ControlCode::PULSE_ON,
		odc::ControlCode::PULSE_OFF,
		odc::ControlCode::LATCH_ON,
		odc::ControlCode::LATCH_OFF,
		odc::ControlCode::CLOSE_PULSE_ON,
		odc::ControlCode::TRIP_PULSE_ON,
		odc::ControlCode::UNDEFINED
	})
	{
		auto lua_val = static_cast< std::underlying_type_t<odc::ControlCode> >(ctrl_code);
		lua_pushstring(L, ToString(ctrl_code).c_str());
		lua_pushinteger(L, lua_val);
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"ControlCode");
}

void ExportConnectStates(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table that has the values
	lua_newtable(L);
	for(const auto& conn_st :
	{
		odc::ConnectState::PORT_UP,
		odc::ConnectState::CONNECTED,
		odc::ConnectState::DISCONNECTED,
		odc::ConnectState::PORT_DOWN
	})
	{
		auto lua_val = static_cast< std::underlying_type_t<odc::ConnectState> >(conn_st);
		lua_pushstring(L, ToString(conn_st).c_str());
		lua_pushinteger(L, lua_val);
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"ConnectState");
}

void ExportPayloadFactory(lua_State* const L)
{
	lua_getglobal(L,"odc");
	//Make a table of Payload factory functions
	//Make a table that has the values of each event type
	lua_newtable(L);
	auto event_type = odc::EventType::BeforeRange;
	while((event_type+1) != odc::EventType::AfterRange)
	{
		event_type = event_type+1;
		auto lua_et_val = static_cast< std::underlying_type_t<odc::EventType> >(event_type);

		//push table key
		lua_pushstring(L, ToString(event_type).c_str());

		//push table value - closure with one upvalues
		lua_pushinteger(L,lua_et_val);
		lua_pushcclosure(L, [](lua_State* const L) -> int
			{
				auto event_type = static_cast<odc::EventType>(lua_tointeger(L, lua_upvalueindex(1)));
				PushPayload(L,event_type);
				return 1; //number of lua ret vals pushed onto the stack
			}, 1);

		//add key value pair to table
		lua_settable(L, -3);
	}
	lua_setfield(L,-2,"MakePayload");
}

#define TOSTRING_TABLE_ENTRY(T)\
	lua_pushstring(L, #T);\
	lua_pushcfunction(L, [](lua_State* const L) -> int\
	{\
		lua_pushstring(L, ToString(static_cast<odc::T>(lua_tointeger(L,-1))).c_str());\
		return 1;\
	});\
	lua_settable(L, -3)
void ExportToStringFunctions(lua_State* const L)
{
	lua_getglobal(L,"odc");
	lua_newtable(L);
	TOSTRING_TABLE_ENTRY(EventType);
	TOSTRING_TABLE_ENTRY(QualityFlags);
	TOSTRING_TABLE_ENTRY(CommandStatus);
	TOSTRING_TABLE_ENTRY(ControlCode);
	TOSTRING_TABLE_ENTRY(ConnectState);
	lua_setfield(L,-2,"ToString");
}

#define PUSH_PAYLOAD_CASE(T)\
	case T:\
		{\
			auto payload = event ? event->GetPayload<T>()\
				   : typename odc::EventTypePayload<T>::type();\
			PushPayload(L,payload);\
			break;\
		}
void PushPayload(lua_State* const L, odc::EventType evt_type, std::shared_ptr<const odc::EventInfo> event)
{
	switch(evt_type)
	{
		PUSH_PAYLOAD_CASE(odc::EventType::Binary                   )
		PUSH_PAYLOAD_CASE(odc::EventType::DoubleBitBinary          )
		PUSH_PAYLOAD_CASE(odc::EventType::Analog                   )
		PUSH_PAYLOAD_CASE(odc::EventType::Counter                  )
		PUSH_PAYLOAD_CASE(odc::EventType::FrozenCounter            )
		PUSH_PAYLOAD_CASE(odc::EventType::BinaryOutputStatus       )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputStatus       )
		PUSH_PAYLOAD_CASE(odc::EventType::BinaryCommandEvent       )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogCommandEvent       )
		PUSH_PAYLOAD_CASE(odc::EventType::OctetString              )
		PUSH_PAYLOAD_CASE(odc::EventType::TimeAndInterval          )
		PUSH_PAYLOAD_CASE(odc::EventType::SecurityStat             )
		PUSH_PAYLOAD_CASE(odc::EventType::ControlRelayOutputBlock  )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputInt16        )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputInt32        )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputFloat32      )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputDouble64     )
		PUSH_PAYLOAD_CASE(odc::EventType::BinaryQuality            )
		PUSH_PAYLOAD_CASE(odc::EventType::DoubleBitBinaryQuality   )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogQuality            )
		PUSH_PAYLOAD_CASE(odc::EventType::CounterQuality           )
		PUSH_PAYLOAD_CASE(odc::EventType::BinaryOutputStatusQuality)
		PUSH_PAYLOAD_CASE(odc::EventType::FrozenCounterQuality     )
		PUSH_PAYLOAD_CASE(odc::EventType::AnalogOutputStatusQuality)
		PUSH_PAYLOAD_CASE(odc::EventType::OctetStringQuality       )
		PUSH_PAYLOAD_CASE(odc::EventType::FileAuth                 )
		PUSH_PAYLOAD_CASE(odc::EventType::FileCommand              )
		PUSH_PAYLOAD_CASE(odc::EventType::FileCommandStatus        )
		PUSH_PAYLOAD_CASE(odc::EventType::FileTransport            )
		PUSH_PAYLOAD_CASE(odc::EventType::FileTransportStatus      )
		PUSH_PAYLOAD_CASE(odc::EventType::FileDescriptor           )
		PUSH_PAYLOAD_CASE(odc::EventType::FileSpecString           )
		PUSH_PAYLOAD_CASE(odc::EventType::ConnectState             )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved1                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved2                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved3                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved4                )
		PUSH_PAYLOAD_CASE(odc::EventType::TimeSync                 )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved6                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved8                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved9                )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved10               )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved11               )
		PUSH_PAYLOAD_CASE(odc::EventType::Reserved12               )
		default:
			lua_pushnil(L);
			break;
	}
}

void PushPayload(lua_State* const L, odc::AbsTime_n_SysOffs payload)
{
	lua_newtable(L);
	lua_pushstring(L, "AbsTime");
	lua_pushboolean(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "SysOffs");
	lua_pushboolean(L, payload.second);
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, bool payload)
{
	lua_pushboolean(L,payload);
}
void PushPayload(lua_State* const L, odc::DBB payload)
{
	lua_newtable(L);
	lua_pushstring(L, "First");
	lua_pushboolean(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "Second");
	lua_pushboolean(L, payload.second);
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, double payload)
{
	lua_pushnumber(L,payload);
}
void PushPayload(lua_State* const L, uint32_t payload)
{
	lua_pushinteger(L,payload);
}
void PushPayload(lua_State* const L, odc::CommandStatus payload)
{
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::CommandStatus> >(payload));
}
void PushPayload(lua_State* const L, odc::OctetStringBuffer payload)
{
	lua_pushlstring(L,static_cast<const char *>(payload.data()),payload.size());
}
void PushPayload(lua_State* const L, odc::TAI payload)
{
	lua_newtable(L);
	lua_pushstring(L, "Timestamp");
	lua_pushinteger(L, std::get<0>(payload));
	lua_settable(L, -3);
	lua_pushstring(L, "Interval");
	lua_pushinteger(L, std::get<1>(payload));
	lua_settable(L, -3);
	lua_pushstring(L, "Units");
	lua_pushinteger(L, std::get<2>(payload));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::SS payload)
{
	lua_newtable(L);
	lua_pushstring(L, "First");
	lua_pushinteger(L, std::get<0>(payload));
	lua_settable(L, -3);
	lua_pushstring(L, "Second");
	lua_pushinteger(L, std::get<1>(payload));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::ControlRelayOutputBlock payload)
{
	lua_newtable(L);
	lua_pushstring(L, "ControlCode");
	lua_pushinteger(L, static_cast< std::underlying_type_t<odc::ControlCode> >(payload.functionCode));
	lua_settable(L, -3);
	lua_pushstring(L, "Count");
	lua_pushinteger(L, payload.count);
	lua_settable(L, -3);
	lua_pushstring(L, "msOnTime");
	lua_pushinteger(L, payload.onTimeMS);
	lua_settable(L, -3);
	lua_pushstring(L, "msOffTime");
	lua_pushinteger(L, payload.offTimeMS);
	lua_settable(L, -3);
	lua_pushstring(L, "CommandStatus");
	lua_pushinteger(L, static_cast< std::underlying_type_t<odc::CommandStatus> >(payload.status));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::AO16 payload)
{
	lua_newtable(L);
	lua_pushstring(L, "Value");
	lua_pushinteger(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "CommandStatus");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::CommandStatus> >(payload.second));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::AO32 payload)
{
	lua_newtable(L);
	lua_pushstring(L, "Value");
	lua_pushinteger(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "CommandStatus");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::CommandStatus> >(payload.second));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::AOF payload)
{
	lua_newtable(L);
	lua_pushstring(L, "Value");
	lua_pushnumber(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "CommandStatus");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::CommandStatus> >(payload.second));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::AOD payload)
{
	lua_newtable(L);
	lua_pushstring(L, "Value");
	lua_pushnumber(L, payload.first);
	lua_settable(L, -3);
	lua_pushstring(L, "CommandStatus");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::CommandStatus> >(payload.second));
	lua_settable(L, -3);
}
void PushPayload(lua_State* const L, odc::QualityFlags payload)
{
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::QualityFlags> >(payload));
}
void PushPayload(lua_State* const L, char payload)
{
	lua_pushinteger(L,payload);
}
void PushPayload(lua_State* const L, odc::ConnectState payload)
{
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::ConnectState> >(payload));
}

void PushEventInfo(lua_State* const L, std::shared_ptr<const odc::EventInfo> event)
{
	//make a lua table for event info on stack
	lua_newtable(L);

	//EventType
	auto lua_et = static_cast< std::underlying_type_t<odc::EventType> >(event->GetEventType());
	lua_pushstring(L, "EventType");
	lua_pushinteger(L, lua_et);
	lua_settable(L, -3);

	//Index
	lua_pushstring(L, "Index");
	lua_pushinteger(L, event->GetIndex());
	lua_settable(L, -3);

	//SourcePort
	lua_pushstring(L, "SourcePort");
	lua_pushstring(L, event->GetSourcePort().c_str());
	lua_settable(L, -3);

	//Quality
	auto lua_q = static_cast< std::underlying_type_t<odc::QualityFlags> >(event->GetQuality());
	lua_pushstring(L, "QualityFlags");
	lua_pushinteger(L, lua_q);
	lua_settable(L, -3);

	//Timestamp
	lua_pushstring(L, "Timestamp");
	lua_pushinteger(L, event->GetTimestamp());
	lua_settable(L, -3);

	//Payload
	lua_pushstring(L, "Payload");
	PushPayload(L,event->GetEventType(),event);
	lua_settable(L, -3);
}

std::shared_ptr<odc::EventInfo> EventInfoFromLua(lua_State* const L, const std::string& Name, const std::string& LogName, int idx)
{
	if(idx < 0)
		idx = lua_gettop(L) + (idx+1);

	if(lua_isnil(L,idx))
		return nullptr;

	if(!lua_istable(L,idx))
	{
		if(auto log = odc::spdlog_get(LogName))
			log->error("{}: EventInfo argument is not a table.",Name);
		return nullptr;
	}

	//EventType
	lua_getfield(L, idx, "EventType");
	if(!lua_isinteger(L,-1))
	{
		if(auto log = odc::spdlog_get(LogName))
			log->error("{}: EventInfo has invalid EventType.",Name);
		return nullptr;
	}
	auto et = static_cast<odc::EventType>(lua_tointeger(L,-1));
	auto event = std::make_shared<odc::EventInfo>(et);

	//Index
	lua_getfield(L, idx, "Index");
	if(lua_isinteger(L,-1))
		event->SetIndex(lua_tointeger(L,-1));

	//SourcePort
	lua_getfield(L, idx, "SourcePort");
	if(lua_isstring(L,-1))
		event->SetSource(lua_tostring(L,-1));

	//QualityFlags
	lua_getfield(L, idx, "QualityFlags");
	if(lua_isinteger(L,-1))
		event->SetQuality(static_cast<odc::QualityFlags>(lua_tointeger(L,-1)));

	//Timestamp
	lua_getfield(L, idx, "Timestamp");
	if(lua_isinteger(L,-1))
		event->SetTimestamp(lua_tointeger(L,-1));

	//Payload
	lua_getfield(L, idx, "Payload");
	try
	{
		PopPayload(L, event);
	}
	catch(const std::exception& e)
	{
		if(auto log = odc::spdlog_get(LogName))
			log->error("{}: Lua EventInfo Payload error: {}",Name,e.what());
		event->SetPayload();
	}

	return event;
}

template <> bool PopPayload(lua_State* const L)
{
	if(!lua_isboolean(L,-1))
		throw std::invalid_argument("Payload is not a lua boolean value.");
	return lua_toboolean(L,-1);
}
template<> odc::DBB PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'First' and 'Second' member booleans.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "First");
	if(!lua_isboolean(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "Second");
	if(!lua_isboolean(L,-1))
		throw std::invalid_argument(err);

	return { lua_toboolean(L,-2), lua_toboolean(L,-1) };
}
template<> double PopPayload(lua_State* const L)
{
	if(!lua_isnumber(L,-1))
		throw std::invalid_argument("Payload is not a lua number value.");
	return lua_tonumber(L,-1);
}
template<> uint32_t PopPayload(lua_State* const L)
{
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument("Payload is not a lua integer value.");
	return lua_tointeger(L,-1);
}
template<> odc::CommandStatus PopPayload(lua_State* const L)
{
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument("Payload is not a lua integer value.");
	return static_cast<odc::CommandStatus>(lua_tointeger(L,-1));
}
template<> odc::OctetStringBuffer PopPayload(lua_State* const L)
{
	if(!lua_isstring(L,-1))
		throw std::invalid_argument("Payload is not a lua string value.");

	size_t len;
	auto buf = lua_tolstring(L,-1,&len);
	return odc::OctetStringBuffer(std::vector(buf,buf+len));
}
template<> odc::TAI PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'Timestamp', 'Interval' and 'Units' member integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "Timestamp");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "Interval");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -3, "Units");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tointeger(L,-3), lua_tointeger(L,-2), lua_tointeger(L,-1) };
}
template<> odc::SS PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'First' and 'Second' member integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "First");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "Second");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tointeger(L,-2), lua_tointeger(L,-1) };
}
template<> odc::ControlRelayOutputBlock PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'ControlCode', 'Count', 'msOnTime', 'msOffTime' and 'CommandStatus' member integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "ControlCode");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "Count");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -3, "msOnTime");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -4, "msOffTime");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -5, "CommandStatus");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	odc::ControlRelayOutputBlock CROB;
	CROB.functionCode = static_cast<odc::ControlCode>(lua_tointeger(L,-5));
	CROB.count = lua_tointeger(L,-4);
	CROB.onTimeMS = lua_tointeger(L,-3);
	CROB.offTimeMS = lua_tointeger(L,-2);
	CROB.status = static_cast<odc::CommandStatus>(lua_tointeger(L,-1));

	return CROB;
}
template<> odc::AO16 PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'Value' and 'CommandStatus' member integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "Value");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "CommandStatus");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tointeger(L,-2), static_cast<odc::CommandStatus>(lua_tointeger(L,-1)) };
}
template<> odc::AO32 PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'Value' and 'CommandStatus' member integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "Value");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "CommandStatus");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tointeger(L,-2), static_cast<odc::CommandStatus>(lua_tointeger(L,-1)) };
}
template<> odc::AOF PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'Value' (number) and 'CommandStatus' (integer) members.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "Value");
	if(!lua_isnumber(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "CommandStatus");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tonumber(L,-2), static_cast<odc::CommandStatus>(lua_tointeger(L,-1)) };
}
template<> odc::AOD PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'Value' (number) and 'CommandStatus' (integer) members.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "Value");
	if(!lua_isnumber(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "CommandStatus");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tonumber(L,-2), static_cast<odc::CommandStatus>(lua_tointeger(L,-1)) };
}
template<> odc::QualityFlags PopPayload(lua_State* const L)
{
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument("Payload is not a lua integer value.");
	return static_cast<odc::QualityFlags>(lua_tointeger(L,-1));
}
template<> char PopPayload(lua_State* const L)
{
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument("Payload is not a lua integer value.");
	return lua_tointeger(L,-1);
}
template<> odc::ConnectState PopPayload(lua_State* const L)
{
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument("Payload is not a lua integer value.");
	return static_cast<odc::ConnectState>(lua_tointeger(L,-1));
}
template<> odc::AbsTime_n_SysOffs PopPayload(lua_State* const L)
{
	const auto err = "Payload is not a lua table with 'AbsTime' and 'SysOffs' member (un)signed integers.";

	if(!lua_istable(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -1, "AbsTime");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	lua_getfield(L, -2, "SysOffs");
	if(!lua_isinteger(L,-1))
		throw std::invalid_argument(err);

	return { lua_tointeger(L,-2), lua_tointeger(L,-1) };
}

#define POP_PAYLOAD_CASE(T)\
	case T:\
		{\
			event->SetPayload<T>(PopPayload<typename odc::EventTypePayload<T>::type>(L));\
			break;\
		}
void PopPayload(lua_State* const L, std::shared_ptr<odc::EventInfo> event)
{
	switch(event->GetEventType())
	{
		POP_PAYLOAD_CASE(odc::EventType::Binary                   )
		POP_PAYLOAD_CASE(odc::EventType::DoubleBitBinary          )
		POP_PAYLOAD_CASE(odc::EventType::Analog                   )
		POP_PAYLOAD_CASE(odc::EventType::Counter                  )
		POP_PAYLOAD_CASE(odc::EventType::FrozenCounter            )
		POP_PAYLOAD_CASE(odc::EventType::BinaryOutputStatus       )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputStatus       )
		POP_PAYLOAD_CASE(odc::EventType::BinaryCommandEvent       )
		POP_PAYLOAD_CASE(odc::EventType::AnalogCommandEvent       )
		POP_PAYLOAD_CASE(odc::EventType::OctetString              )
		POP_PAYLOAD_CASE(odc::EventType::TimeAndInterval          )
		POP_PAYLOAD_CASE(odc::EventType::SecurityStat             )
		POP_PAYLOAD_CASE(odc::EventType::ControlRelayOutputBlock  )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputInt16        )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputInt32        )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputFloat32      )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputDouble64     )
		POP_PAYLOAD_CASE(odc::EventType::BinaryQuality            )
		POP_PAYLOAD_CASE(odc::EventType::DoubleBitBinaryQuality   )
		POP_PAYLOAD_CASE(odc::EventType::AnalogQuality            )
		POP_PAYLOAD_CASE(odc::EventType::CounterQuality           )
		POP_PAYLOAD_CASE(odc::EventType::BinaryOutputStatusQuality)
		POP_PAYLOAD_CASE(odc::EventType::FrozenCounterQuality     )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputStatusQuality)
		POP_PAYLOAD_CASE(odc::EventType::OctetStringQuality       )
		POP_PAYLOAD_CASE(odc::EventType::FileAuth                 )
		POP_PAYLOAD_CASE(odc::EventType::FileCommand              )
		POP_PAYLOAD_CASE(odc::EventType::FileCommandStatus        )
		POP_PAYLOAD_CASE(odc::EventType::FileTransport            )
		POP_PAYLOAD_CASE(odc::EventType::FileTransportStatus      )
		POP_PAYLOAD_CASE(odc::EventType::FileDescriptor           )
		POP_PAYLOAD_CASE(odc::EventType::FileSpecString           )
		POP_PAYLOAD_CASE(odc::EventType::ConnectState             )
		POP_PAYLOAD_CASE(odc::EventType::Reserved1                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved2                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved3                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved4                )
		POP_PAYLOAD_CASE(odc::EventType::TimeSync                 )
		POP_PAYLOAD_CASE(odc::EventType::Reserved6                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved8                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved9                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved10               )
		POP_PAYLOAD_CASE(odc::EventType::Reserved11               )
		POP_PAYLOAD_CASE(odc::EventType::Reserved12               )
		default:
			throw std::runtime_error("Can't parse Payload for unknown EventType "+ToString(event->GetEventType()));
	}
}

