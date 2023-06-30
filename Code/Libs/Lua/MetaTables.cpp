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
 * MetaTables.cpp
 *
 *  Created on: 29/6/2023
 *      Author: Neil Stephens
 */

#include <Lua/CLua.h>
#include <memory>
#include <iostream>

void ExportMetaTables(lua_State* const L)
{
	luaL_newmetatable(L,"std_weak_ptr_void");
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto ppWeakVoid = static_cast<std::weak_ptr<void>*>(lua_touserdata(L,1));
			std::destroy_at(ppWeakVoid);
			return 0;
		});
	lua_setfield(L,-2,"__gc");

	luaL_newmetatable(L,"std_shared_ptr_void");
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto ppSharedVoid = static_cast<std::shared_ptr<void>*>(lua_touserdata(L,1));
			std::destroy_at(ppSharedVoid);
			return 0;
		});
	lua_setfield(L,-2,"__gc");

	//pop the meta tables back off to return the stack to where it started
	lua_pop(L,2);
}
