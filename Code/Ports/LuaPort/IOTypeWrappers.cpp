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
#include "CLua.h"
#include <opendatacon/IOTypes.h>

void ExportEventTypes(lua_State* const L)
{
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
	lua_setglobal(L,"EventType");
}

void ExportQualityFlags(lua_State* const L)
{
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
	lua_setglobal(L,"QualityFlag");

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
	lua_setglobal(L, "QualityFlags");
}

void ExportCommandStatus(lua_State* const L)
{
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
	lua_setglobal(L,"CommandStatus");
}

void ExportControlCodes(lua_State* const L)
{
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
	lua_setglobal(L,"ControlCode");
}

void ExportConnectStates(lua_State* const L)
{
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
	lua_setglobal(L,"ConnectState");
}

void ExportPayloadFactory(lua_State* const L)
{
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
	lua_setglobal(L,"MakePayload");
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
	lua_newtable(L);
	TOSTRING_TABLE_ENTRY(EventType);
	TOSTRING_TABLE_ENTRY(QualityFlags);
	TOSTRING_TABLE_ENTRY(CommandStatus);
	TOSTRING_TABLE_ENTRY(ControlCode);
	TOSTRING_TABLE_ENTRY(ConnectState);
	lua_setglobal(L, "ToString");
}

#define PAYLOAD_CASE(T)\
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
		PAYLOAD_CASE(odc::EventType::Binary                   )
		PAYLOAD_CASE(odc::EventType::DoubleBitBinary          )
		PAYLOAD_CASE(odc::EventType::Analog                   )
		PAYLOAD_CASE(odc::EventType::Counter                  )
		PAYLOAD_CASE(odc::EventType::FrozenCounter            )
		PAYLOAD_CASE(odc::EventType::BinaryOutputStatus       )
		PAYLOAD_CASE(odc::EventType::AnalogOutputStatus       )
		PAYLOAD_CASE(odc::EventType::BinaryCommandEvent       )
		PAYLOAD_CASE(odc::EventType::AnalogCommandEvent       )
		PAYLOAD_CASE(odc::EventType::OctetString              )
		PAYLOAD_CASE(odc::EventType::TimeAndInterval          )
		PAYLOAD_CASE(odc::EventType::SecurityStat             )
		PAYLOAD_CASE(odc::EventType::ControlRelayOutputBlock  )
		PAYLOAD_CASE(odc::EventType::AnalogOutputInt16        )
		PAYLOAD_CASE(odc::EventType::AnalogOutputInt32        )
		PAYLOAD_CASE(odc::EventType::AnalogOutputFloat32      )
		PAYLOAD_CASE(odc::EventType::AnalogOutputDouble64     )
		PAYLOAD_CASE(odc::EventType::BinaryQuality            )
		PAYLOAD_CASE(odc::EventType::DoubleBitBinaryQuality   )
		PAYLOAD_CASE(odc::EventType::AnalogQuality            )
		PAYLOAD_CASE(odc::EventType::CounterQuality           )
		PAYLOAD_CASE(odc::EventType::BinaryOutputStatusQuality)
		PAYLOAD_CASE(odc::EventType::FrozenCounterQuality     )
		PAYLOAD_CASE(odc::EventType::AnalogOutputStatusQuality)
		PAYLOAD_CASE(odc::EventType::FileAuth                 )
		PAYLOAD_CASE(odc::EventType::FileCommand              )
		PAYLOAD_CASE(odc::EventType::FileCommandStatus        )
		PAYLOAD_CASE(odc::EventType::FileTransport            )
		PAYLOAD_CASE(odc::EventType::FileTransportStatus      )
		PAYLOAD_CASE(odc::EventType::FileDescriptor           )
		PAYLOAD_CASE(odc::EventType::FileSpecString           )
		PAYLOAD_CASE(odc::EventType::ConnectState             )
		PAYLOAD_CASE(odc::EventType::Reserved1                )
		PAYLOAD_CASE(odc::EventType::Reserved2                )
		PAYLOAD_CASE(odc::EventType::Reserved3                )
		PAYLOAD_CASE(odc::EventType::Reserved4                )
		PAYLOAD_CASE(odc::EventType::Reserved5                )
		PAYLOAD_CASE(odc::EventType::Reserved6                )
		PAYLOAD_CASE(odc::EventType::Reserved7                )
		PAYLOAD_CASE(odc::EventType::Reserved8                )
		PAYLOAD_CASE(odc::EventType::Reserved9                )
		PAYLOAD_CASE(odc::EventType::Reserved10               )
		PAYLOAD_CASE(odc::EventType::Reserved11               )
		PAYLOAD_CASE(odc::EventType::Reserved12               )
		default:
			lua_pushnil(L);
			break;
	}
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
	lua_getglobal(L,"OctetStringFormat");
	auto OSF = static_cast<odc::DataToStringMethod>(lua_tointeger(L,-1));
	lua_pop(L,1);
	auto chardata = static_cast<const char*>(payload.data());
	auto rawdata = static_cast<const uint8_t*>(payload.data());
	switch(OSF)
	{
		case odc::DataToStringMethod::Raw:
			lua_pushstring(L, std::string(chardata,payload.size()).c_str());
		case odc::DataToStringMethod::Hex:
		default:
			lua_pushstring(L, odc::buf2hex(rawdata,payload.size()).c_str());
	}
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
