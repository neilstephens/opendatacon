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

void ExportIOTypeWrappersToLua(lua_State* const L)
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

