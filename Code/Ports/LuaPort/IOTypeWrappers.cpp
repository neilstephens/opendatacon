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

#include "CLua.h"
#include <opendatacon/IOTypes.h>

void ExportEventTypes(lua_State* const L)
{
	auto event_type = odc::EventType::BeforeRange;
	while((event_type+1) != odc::EventType::AfterRange)
	{
		event_type = event_type+1;
		auto lua_return = static_cast< std::underlying_type_t<odc::EventType> >(event_type);

		//push value to be captured by closure (called upvalue)
		lua_pushinteger(L,lua_return);
		lua_pushcclosure(L, [](lua_State* const L) -> int
			{
				lua_pushvalue(L, lua_upvalueindex(1));
				return 1; //number of lua ret vals pushed onto the stack
			}, 1);

		//give the closure a name to call from lau
		lua_setglobal(L, (ToString(event_type)+"EventType").c_str());
	}
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

