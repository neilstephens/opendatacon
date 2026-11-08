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
 *  LuaTranslation.cpp
 */

#include <Lua/Translation.h>
#include <Lua/Wrappers.h>
#include <Lua/DynamicSymbols.h>
#include "TranslationWrappers.h"
#include <opendatacon/Translation.h>
#include <opendatacon/asio.h>
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{

class LuaTranslator: public odc::ISerialiser, public odc::IDeserialiser
{
public:
	LuaTranslator(const odc::TranslationFactory::Spec& spec, const odc::TranslationConfig& config, const odc::TranslationRole role):
		Config(config),
		Role(role),
		Name("LuaTranslation("+spec.Origin+")"),
		Log(config.LogName),
		pStrand(odc::make_shared(odc::asio_service::Get()->make_strand()))
	{
		L = luaL_newstate();
		if(!L)
			throw std::runtime_error(Name+": Failed to create Lua state");
		try
		{
			lua_newtable(L);
			lua_pushstring(L,Name.c_str());
			lua_setfield(L,-2,"Name");
			lua_setglobal(L,"odc");

			luaL_openlibs(L);
			ExportWrappersToLua(L,pStrand,handler_tracker,Name,config.LogName,false);
			//override the default wrapper config
			ExportTranslationWrappers(L,&Config);

			const std::string chunkname = spec.Origin.empty() || spec.Origin == "inline Lua" ? "=inline Lua" : "@"+spec.Origin;
			if(luaL_loadbufferx(L, spec.Content.data(), spec.Content.size(), chunkname.c_str(), nullptr) != LUA_OK
			   || lua_pcall(L,0,0,0) != LUA_OK)
			{
				std::string err = lua_tostring(L,-1) ? lua_tostring(L,-1) : "unknown error";
				throw std::runtime_error(Name+": Failed to load Lua code: "+err);
			}
		}
		catch(...)
		{
			lua_close(L);
			throw;
		}
	}
	~LuaTranslator() override
	{
		std::scoped_lock lck(mtx);
		lua_close(L);
	}

	void RequireFunction(const std::string& func_name)
	{
		std::scoped_lock lck(mtx);
		lua_getglobal(L,func_name.c_str());
		bool is_func = lua_isfunction(L,-1);
		lua_pop(L,1);
		if(!is_func)
			throw std::runtime_error(Name+": Lua code doesn't have a '"+func_name+"' function.");
	}

	std::vector<uint8_t> Serialise(const std::shared_ptr<const odc::EventInfo>& event, const std::string& SenderName, const odc::ExtraPointFields& extra_fields) const override
	{
		std::scoped_lock lck(mtx);
		StackGuard guard(L);

		const char* func_name = Role == odc::TranslationRole::Key ? "key" : "serialise";
		lua_getglobal(L,func_name);
		PushEventInfo(L,event);
		lua_pushlstring(L,SenderName.data(),SenderName.size());
		lua_createtable(L,0,extra_fields.size());
		for(const auto& [k,v] : extra_fields)
		{
			lua_pushlstring(L,v.data(),v.size());
			lua_setfield(L,-2,k.c_str());
		}

		if(lua_pcall(L,3,1,0) != LUA_OK)
			throw std::runtime_error(Name+": Lua "+func_name+"() error: "+ErrString());
		if(!lua_isstring(L,-1))
			throw std::runtime_error(Name+": Lua "+func_name+"() didn't return a string");
		size_t len;
		auto str = lua_tolstring(L,-1,&len);
		return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(str),reinterpret_cast<const uint8_t*>(str)+len);
	}

	std::shared_ptr<odc::EventInfo> Deserialise(const uint8_t* data, size_t size, const odc::KeyContext& key) override
	{
		std::scoped_lock lck(mtx);
		StackGuard guard(L);

		lua_getglobal(L,"deserialise");
		lua_pushlstring(L,reinterpret_cast<const char*>(data),size);
		if(key.Key)
			lua_pushlstring(L,reinterpret_cast<const char*>(key.Key),key.KeySize);
		else
			lua_pushnil(L);
		if(key.Hint)
		{
			lua_createtable(L,0,3);
			lua_pushinteger(L,static_cast<lua_Integer>(key.Hint->Type));
			lua_setfield(L,-2,"EventType");
			lua_pushinteger(L,static_cast<lua_Integer>(key.Hint->Index));
			lua_setfield(L,-2,"Index");
			if(key.Hint->SourcePort)
			{
				lua_pushstring(L,key.Hint->SourcePort->c_str());
				lua_setfield(L,-2,"SourcePort");
			}
		}
		else
			lua_pushnil(L);

		if(lua_pcall(L,3,1,0) != LUA_OK)
		{
			Log.Error("{}: Lua deserialise() error: {}",Name,ErrString());
			return nullptr;
		}
		auto event = EventInfoFromLua(L,Name,Config.LogName,-1);
		if(!event)
			return nullptr;

		if(key.Hint)
		{
			if(event->GetEventType() != key.Hint->Type)
			{
				Log.Error("{}: Lua deserialise() returned EventType {}, but the key says {}. Dropping.",Name,odc::ToString(event->GetEventType()),odc::ToString(key.Hint->Type));
				return nullptr;
			}
			if(event->GetIndex() != key.Hint->Index)
			{
				Log.Warn("{}: Lua deserialise() returned Index {}, but the key says {}. Using the key's.",Name,event->GetIndex(),key.Hint->Index);
				event->SetIndex(key.Hint->Index);
			}
			if(key.Hint->SourcePort)
			{
				if(event->GetSourcePort() != *key.Hint->SourcePort && !event->GetSourcePort().empty())
					Log.Warn("{}: Lua deserialise() returned SourcePort '{}', but the key says '{}'. Using the key's.",Name,event->GetSourcePort(),*key.Hint->SourcePort);
				event->SetSource(*key.Hint->SourcePort);
			}
		}
		return event;
	}

private:
	//restore the Lua stack size at the end of a call
	struct StackGuard
	{
		explicit StackGuard(lua_State* L): L(L), top(lua_gettop(L)) {}
		~StackGuard(){ lua_settop(L,top); }
		lua_State* L;
		int top;
	};
	std::string ErrString() const
	{
		auto err = lua_tostring(L,-1);
		return err ? err : "unknown error";
	}

	const odc::TranslationConfig Config;
	const odc::TranslationRole Role;
	const std::string Name;
	odc::LogHelpers Log;
	Lua::DynamicSymbols lua_syms; //in case lua modules need to resolve symbols
	std::shared_ptr<void> handler_tracker = odc::make_shared(char {});
	std::shared_ptr<odc::strand_t> pStrand;
	mutable std::mutex mtx;
	lua_State* L = nullptr;
};

}

extern "C" void RegisterLuaTranslation()
{
	odc::TranslationFactory::Register(odc::EventTranslationMethod::Lua,
		[](const odc::TranslationFactory::Spec& spec, const odc::TranslationConfig& config, odc::TranslationRole role)->std::shared_ptr<odc::ISerialiser>
		{
			auto pTranslator = std::make_shared<LuaTranslator>(spec,config,role);
			pTranslator->RequireFunction(role == odc::TranslationRole::Key ? "key" : "serialise");
			return pTranslator;
		},
		[](const odc::TranslationFactory::Spec& spec, const odc::TranslationConfig& config)->std::shared_ptr<odc::IDeserialiser>
		{
			auto pTranslator = std::make_shared<LuaTranslator>(spec,config,odc::TranslationRole::Payload);
			pTranslator->RequireFunction("deserialise");
			return pTranslator;
		});
}
