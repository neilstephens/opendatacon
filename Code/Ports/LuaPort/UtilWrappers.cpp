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
 *  UtilWrappers.cpp
 *
 *  Created on: 18/06/2023
 *      Author: Neil Stephens
 */
#include "CLua.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>
#include <json/json.h>

//Convert JSON to a Lua table on the Lua stack
void PushJSON(lua_State* const L, const Json::Value& JSON)
{
	if(JSON.isNull())
	{
		lua_pushnil(L);
		return;
	}
	if(JSON.isObject())
	{
		lua_newtable(L);
		for(const auto& m : JSON.getMemberNames())
		{
			lua_pushstring(L,m.c_str());
			PushJSON(L,JSON[m]);
			lua_settable(L,-3);
		}
		return;
	}
	if(JSON.isArray())
	{
		lua_createtable(L, JSON.size(), 0);
		for(Json::ArrayIndex n=0; n<JSON.size(); n++)
		{
			PushJSON(L,JSON[n]);
			lua_rawseti(L,-2,n+1);
		}
		return;
	}
	if(JSON.isBool())
	{
		lua_pushboolean(L,JSON.asBool());
		return;
	}
	if(JSON.isIntegral())
	{
		lua_pushinteger(L,JSON.asInt64());
		return;
	}
	if(JSON.isDouble())
	{
		lua_pushnumber(L,JSON.asDouble());
		return;
	}
	if(JSON.isString())
	{
		lua_pushstring(L,JSON.asCString());
		return;
	}
}

Json::Value JSONFromTable(lua_State* const L, int idx)
{
	if(idx < 0)
		idx = lua_gettop(L) + (idx+1);

	if(!lua_istable(L,idx))
		return Json::nullValue;

	Json::Value JSON;

	//store both keys and values until we know if the table is an array
	std::vector<std::string> keys;
	std::vector<Json::Value> vals;
	bool isArr = true;
	lua_Integer arr_idx = 1;
	// push nil for lua_next to indicate it needs to pick the first key
	lua_pushnil(L);
	while (lua_next(L, idx) != 0)
	{
		if(!isArr || !lua_isinteger(L,-2) || lua_tointeger(L,-2) != arr_idx)
			isArr = false;
		if(!lua_isstring(L,-2))
			keys.push_back("INVALID_KEY_"+std::to_string(arr_idx));
		else
		{
			//FIXME: can't use tostring because it changes numbers and confuses lua_next
			keys.push_back(lua_tostring(L,-2));
		}

		if(lua_isboolean(L,-1))
			vals.push_back(lua_toboolean(L,-1));
		else if(lua_isinteger(L,-1))
			vals.push_back(Json::Int64(lua_tointeger(L,-1)));
		else if(lua_isnumber(L,-1))
			vals.push_back(lua_tonumber(L,-1));
		else if(lua_isstring(L,-1))
			vals.push_back(Json::Int64(lua_tointeger(L,-1)));
		else
			vals.push_back(JSONFromTable(L,-1));

		lua_pop(L,1);
		arr_idx++;
	}
	if(isArr)
	{
		JSON = Json::arrayValue;
		for(Json::ArrayIndex n=0; n<vals.size(); n++)
			JSON[n] = vals[n];
		return JSON;
	}
	size_t i=0;
	for(const auto& k : keys)
		JSON[k] = vals[i];
	return JSON;
}

void ExportUtilWrappers(lua_State* const L)
{
	//Decode JSON
	//	We can already push JSON to the Lua stack for passing config
	//	So we might as well expose the jsoncpp parser too
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			if(!lua_isstring(L,1))
			{
			      lua_pushnil(L);
			      return 1;
			}
			std::istringstream json_ss(lua_tostring(L,1));
			Json::CharReaderBuilder JSONReader;
			std::string err_str;
			Json::Value JSON;
			bool parse_success = Json::parseFromStream(JSONReader,json_ss, &JSON, &err_str);
			if (!parse_success)
			{
			      lua_pushstring(L,("Failed to parse JSON: "+err_str).c_str());
			      return 1;
			}
			PushJSON(L,JSON);
			return 1;
		});
	lua_setglobal(L, "DecodeJSON");

	//EncodeJSON - wouldn't be nice to provide decode without encode
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto json = JSONFromTable(L,1);
			if(json.isNull())
				lua_pushnil(L);
			else
				lua_pushstring(L,json.toStyledString().c_str());
			return 1;
		});
	lua_setglobal(L, "EncodeJSON");

	//Expose msSinceEpoch()
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			if(lua_isstring(L,1)) //there's a datetime string to convert
			{
			      if(lua_isstring(L,2)) //there's also a format string
			      {
			            lua_pushinteger(L, odc::datetime_to_since_epoch(lua_tostring(L,1),lua_tostring(L,2)));
			            return 1;
				}
			      lua_pushinteger(L, odc::datetime_to_since_epoch(lua_tostring(L,1)));
			      return 1;
			}
			lua_pushinteger(L, odc::msSinceEpoch());
			return 1; //number of lua ret vals pushed onto the stack
		});
	lua_setglobal(L, "msSinceEpoch");

	//DateTime string conversion
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			if(!lua_isinteger(L,1))
			{
			      lua_pushnil(L);
			      return 1;
			}
			if(lua_isstring(L,2)) //there's a format string
			{
			      try
			      {
			            lua_pushstring(L, odc::since_epoch_to_datetime(lua_tointeger(L,1),lua_tostring(L,2)).c_str());
				}
			      catch(const std::exception& e)
			      {
			            lua_pushnil(L);
				}
			      return 1;
			}
			lua_pushstring(L, odc::since_epoch_to_datetime(lua_tointeger(L,1)).c_str());
			return 1; //number of lua ret vals pushed onto the stack
		});
	lua_setglobal(L, "msSinceEpochToDateTime");

	//DataToStringMethod enum
	lua_newtable(L);
	lua_pushstring(L,"Hex");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::DataToStringMethod> >(odc::DataToStringMethod::Hex));
	lua_settable(L, -3);
	lua_pushstring(L,"Raw");
	lua_pushinteger(L,static_cast< std::underlying_type_t<odc::DataToStringMethod> >(odc::DataToStringMethod::Raw));
	lua_settable(L, -3);
	lua_setglobal(L, "DataToStringMethod");

	//String2Hex
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			size_t len;
			auto buf = lua_tolstring(L,-1,&len);
			lua_pushstring(L,odc::buf2hex((uint8_t*)buf,len).c_str());
			return 1;
		});
	lua_setglobal(L, "String2Hex");

	//Hex2String
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto hex = lua_tostring(L,-1);
			try
			{
			      auto buf = odc::hex2buf(hex);
			      lua_pushlstring(L,(char*)buf.data(),buf.size());
			      return 1;
			}
			catch(const std::exception& e)
			{
			      lua_pushnil(L);
			      return 1;
			}
		});
	lua_setglobal(L, "Hex2String");
}

