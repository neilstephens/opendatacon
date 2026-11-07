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
 *  TranslationWrappers.cpp
 */

#include "TranslationWrappers.h"
#include "IOTypeWrappers.h"
#include <opendatacon/TemplateTranslation.h>
#include <opendatacon/CBORTranslation.h>
#include <string>

namespace
{

odc::ExtraPointFields ExtraFieldsFromLua(lua_State* const L, int idx)
{
	odc::ExtraPointFields extra;
	if(idx < 0)
		idx = lua_gettop(L) + (idx+1);
	if(!lua_istable(L,idx))
		return extra;
	lua_pushnil(L);
	while(lua_next(L, idx) != 0)
	{
		//copy the key, so lua_tostring doesn't confuse lua_next
		lua_pushvalue(L,-2);
		const char* k = lua_tostring(L,-1);
		const char* v = lua_tostring(L,-2);
		if(k && v)
			extra[k] = v;
		lua_pop(L,2);
	}
	return extra;
}

enum class Kind { Template, CBOR };

//Does the work, and returns false (with an error message) rather than throwing, because
//	the caller has to raise the Lua error outside the scope of any C++ objects
bool Translate(lua_State* const L, Kind kind, std::string& err)
{
	try
	{
		auto pConf = static_cast<const odc::TranslationConfig*>(lua_touserdata(L, lua_upvalueindex(1)));
		size_t spec_len;
		const char* spec = luaL_checklstring(L, 1, &spec_len);
		auto event = EventInfoFromLua(L, "odc translation wrapper", pConf->LogName, 2);
		if(!event)
		{
			err = "The event argument is not a valid EventInfo";
			return false;
		}
		const char* sender = luaL_optstring(L, 3, "");
		auto extra = ExtraFieldsFromLua(L, 4);

		if(kind == Kind::Template)
		{
			auto str = odc::TemplateSerialiser::Fill(std::string(spec, spec_len), event, sender, extra, pConf->DateTimeFormat, pConf->DateTimeIsUTC, pConf->OctetStringFormat);
			lua_pushlstring(L, str.data(), str.size());
		}
		else
		{
			odc::CBORSerialiser ser(std::string(spec, spec_len), *pConf);
			auto bytes = ser.Serialise(event, sender, extra);
			lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
		}
		return true;
	}
	catch(const std::exception& e)
	{
		err = e.what();
		return false;
	}
}

template <Kind K>
int LuaTranslate(lua_State* L)
{
	{
		std::string err;
		if(Translate(L, K, err))
			return 1;
		lua_pushstring(L, err.c_str());
	}
	return lua_error(L);
}

}

void ExportTranslationWrappers(lua_State* const L, const odc::TranslationConfig* pConf)
{
	static const odc::TranslationConfig default_config;
	if(!pConf)
		pConf = &default_config;

	lua_getglobal(L,"odc");
	lua_pushlightuserdata(L, const_cast<odc::TranslationConfig*>(pConf));
	lua_pushcclosure(L, LuaTranslate<Kind::Template>, 1);
	lua_setfield(L,-2,"FillTemplate");
	lua_pushlightuserdata(L, const_cast<odc::TranslationConfig*>(pConf));
	lua_pushcclosure(L, LuaTranslate<Kind::CBOR>, 1);
	lua_setfield(L,-2,"CBOREncode");
	lua_pop(L,1);
}
