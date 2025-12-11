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
#include <whereami++.h>
#include <Lua/CLua.h>
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/Platform.h>
#include <json/json.h>

//Convert JSON to a Lua table on the Lua stack
void PushJSON(lua_State* const L, const Json::Value& JSON)
{
	if(JSON.isNull())
	{
		lua_pushstring(L,"Json::nullValue");
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
	if(JSON.isNumeric())
	{
		lua_pushnumber(L,JSON.asDouble());
		return;
	}
	if(JSON.isString())
	{
		lua_pushstring(L,JSON.asCString());
		return;
	}
	lua_pushnil(L);
	return;
}

Json::Value JSONFromLua(lua_State* const L, int idx, bool asKey = false);
Json::Value JSONFromTable(lua_State* const L, int idx)
{
	if(idx < 0)
		idx = lua_gettop(L) + (idx+1);

	if(!lua_istable(L,idx))
		return Json::Value::nullSingleton();

	//store both keys and values until we know if the table is an array
	std::vector<std::string> keys;
	std::vector<Json::Value> vals;
	bool isArr = false;
	lua_Integer arr_idx = 1;
	// push nil for lua_next to indicate it needs to pick the first key
	lua_pushnil(L);
	while (lua_next(L, idx) != 0)
	{
		isArr = ((isArr || arr_idx == 1) && lua_isinteger(L,-2) && lua_tointeger(L,-2) == arr_idx);
		keys.push_back(JSONFromLua(L,-2,true).asString());
		vals.push_back(JSONFromLua(L,-1));
		lua_pop(L,1);
		arr_idx++;
	}

	if(isArr)
	{
		Json::Value JSON(Json::arrayValue);
		for(Json::ArrayIndex n=0; n<vals.size(); n++)
			JSON[n] = vals[n];
		return JSON;
	}
	Json::Value JSON(Json::objectValue);
	size_t i=0;
	for(const auto& k : keys)
		JSON[k] = vals[i++];
	return JSON;
}

std::string ToString(const void* ptr)
{
	std::ostringstream oss;
	oss<<ptr;
	return oss.str();
}

Json::Value JSONFromLua(lua_State* const L, int idx, bool asKey)
{
	if(idx < 0)
		idx = lua_gettop(L) + (idx+1);

	//using a switch statement on lua_type()
	//	avoids accounting for lua_istype() conversion semantics (lua type coersion)
	//	especially important because this is called within lua_next (get's confused if lua_tostring converts something)
	switch(lua_type(L,idx))
	{
		case LUA_TNIL:
			return "LUA_TNIL";
		case LUA_TBOOLEAN:
			return static_cast<bool>(lua_toboolean(L,idx));
		case LUA_TNUMBER:
		{
			if(lua_isinteger(L,idx))
				return Json::Int64(lua_tointeger(L,idx));
			return lua_tonumber(L,idx);
		}
		case LUA_TSTRING:
		{
			auto s = lua_tostring(L,idx);
			if(strcmp(s,"Json::nullValue") == 0)
				return Json::Value::nullSingleton();
			return s;
		}
		case LUA_TTABLE:
		{
			if(asKey)
				return "LUA_TTABLE_"+ToString(lua_topointer(L,idx));
			return JSONFromTable(L,idx);
		}
		case LUA_TLIGHTUSERDATA:
			return "LUA_TLIGHTUSERDATA_"+ToString(lua_topointer(L,idx));
		case LUA_TFUNCTION:
			return "LUA_TFUNCTION_"+ToString(lua_topointer(L,idx));
		case LUA_TUSERDATA:
			return "LUA_TUSERDATA_"+ToString(lua_topointer(L,idx));
		case LUA_TTHREAD:
			return "LUA_TTHREAD_"+ToString(lua_topointer(L,idx));
		default:
			return "LUA_UNKNOWN_TYPE";
	}
}

// Helper to create a Lua FILE* userdata (like io.open does)
inline void PushFile(lua_State* L, FILE* fp)
{
	if (!fp) { lua_pushnil(L); return; }

	// Allocate userdata of the correct size (luaL_Stream contains FILE* and int closef)
	luaL_Stream *p = (luaL_Stream*)lua_newuserdata(L, sizeof(luaL_Stream));
	p->f = fp;
	p->closef = [](lua_State* const L) -> int
			{
				luaL_Stream* p = (luaL_Stream*)lua_touserdata(L, 1);
				if (p && p->f)
				{
					fclose(p->f);
					p->f = nullptr;
				}
				return 0;
			};

	// set the standard file metatable
	luaL_setmetatable(L, LUA_FILEHANDLE);
}

// Helper to repeatedly call a coroutine,
//  using the return/yield value as number of ms before calling again
//  the loop stops on a negative return or asio error (cancelled timer)
inline void CoroutineLoop(lua_State* const L,
	const std::string& Name,
	const std::string& LogName,
	const int LuaCRref,
	std::shared_ptr<asio::steady_timer> pTimer,
	std::shared_ptr<asio::io_service::strand> pSyncStrand)
{
	//get coroutine from the registry back on stack
	lua_rawgeti(L, LUA_REGISTRYINDEX, LuaCRref);

	//and call it
	const int argc = 0; const int retc = 1;
	auto ret = lua_pcall(L,argc,retc,0);
	if(ret == LUA_OK)
	{
		if(lua_isinteger(L,-1))
		{
			auto ms = lua_tointeger(L,-1);
			lua_pop(L,retc);
			if(ms >= 0)
			{
				pTimer->expires_from_now(std::chrono::milliseconds(ms));
				pTimer->async_wait(pSyncStrand->wrap([=](asio::error_code err)
					{
						if(!err)
							CoroutineLoop(L,Name,LogName,LuaCRref,pTimer,pSyncStrand);
					}));
				return;
			}
		}
		else
			lua_pop(L,retc);
	}
	else
	{
		std::string call_err = lua_tostring(L, -1);
		if(auto log = odc::spdlog_get(LogName))
			log->error("{}: Error calling lua coroutine: {}",Name,call_err);
		lua_pop(L,1);
	}
	//release the reference to the callback
	luaL_unref(L, LUA_REGISTRYINDEX, LuaCRref);
}

extern "C" void ExportUtilWrappers(lua_State* const L,
	std::shared_ptr<asio::io_service::strand> pSyncStrand,
	std::shared_ptr<void> handler_tracker,
	const std::string& Name,
	const std::string& LogName)
{
	lua_getglobal(L,"odc");

	//Decode JSON
	//	We can already push JSON to the Lua stack for passing config
	//	So we might as well expose the jsoncpp parser too
	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(!lua_isstring(L,1))
			{
				if(auto log = odc::spdlog_get(logname))
					log->error("{}: DecodeJSON() called by lua; Argument not string.",name);
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
				if(auto log = odc::spdlog_get(logname))
					log->error("{}: Failed to parse JSON: '{}'.",name,err_str);
				lua_pushnil(L);
				return 1;
			}
			PushJSON(L,JSON);
			return 1;
		},2);
	lua_setfield(L,-2,"DecodeJSON");

	//EncodeJSON - wouldn't be nice to provide decode without encode
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			auto json = JSONFromLua(L,1);

			Json::StreamWriterBuilder wbuilder;
			lua_getglobal(L,"PrettyPrintJSON");
			wbuilder["indentation"] = (lua_isboolean(L,-1) && lua_toboolean(L,-1)) ? "\t" : "";

			lua_pushstring(L,Json::writeString(wbuilder,json).c_str());
			return 1;
		});
	lua_setfield(L,-2,"EncodeJSON");

	//Expose msSinceEpoch()
	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			if(lua_isstring(L,1)) //there's a datetime string to convert
			{
				try
				{
					if(lua_isstring(L,2)) //there's also a format string
					{
						lua_pushinteger(L, odc::datetime_to_since_epoch(lua_tostring(L,1),lua_tostring(L,2)));
						return 1;
					}
					lua_pushinteger(L, odc::datetime_to_since_epoch(lua_tostring(L,1)));
					return 1;
				}
				catch(const std::exception& e)
				{
					std::string name(lua_tostring(L, lua_upvalueindex(1)));
					std::string logname(lua_tostring(L, lua_upvalueindex(2)));
					if(auto log = odc::spdlog_get(logname))
						log->error("{}: msSinceEpoch() called from lua; Exception: '{}'.",name,e.what());
					lua_pushnil(L);
					return 1;
				}
			}
			lua_pushinteger(L, odc::msSinceEpoch());
			return 1; //number of lua ret vals pushed onto the stack
		},2);
	lua_setfield(L,-2,"msSinceEpoch");

	//DateTime string conversion
	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(!lua_isinteger(L,1))
			{
				if(auto log = odc::spdlog_get(logname))
					log->error("{}: msSinceEpochToDateTime() called from lua; Argument not integer.",name);
				lua_pushnil(L);
				return 1;
			}
			try
			{
				if(lua_isstring(L,2)) //there's a format string
					lua_pushstring(L, odc::since_epoch_to_datetime(lua_tointeger(L,1),lua_tostring(L,2)).c_str());
				else
					lua_pushstring(L, odc::since_epoch_to_datetime(lua_tointeger(L,1)).c_str());
			}
			catch(const std::exception& e)
			{
				if(auto log = odc::spdlog_get(logname))
					log->error("{}: msSinceEpochToDateTime() called from lua; Exception '{}'.",name,e.what());
				lua_pushnil(L);
			}
			return 1; //number of lua ret vals pushed onto the stack
		},2);
	lua_setfield(L,-2,"msSinceEpochToDateTime");

	//GetPath
	static auto path_from_args = [](lua_State* const L) -> auto
					     {
						     int idx = 1;
						     std::filesystem::path result;
						     while(lua_isstring(L,idx))
							     result /= std::filesystem::path(lua_tostring(L,idx++));
						     return result;
					     };
	lua_newtable(L);
	{
		//WorkingDir
		lua_pushcfunction(L, [](lua_State* const L) -> int
			{
				auto result = std::filesystem::canonical(std::filesystem::current_path())/path_from_args(L);
				lua_pushstring(L, result.string().c_str());
				return 1;
			});
		lua_setfield(L,-2,"WorkingDir");

		//LibraryDir
		lua_pushcfunction(L, [](lua_State* const L) -> int
			{
				auto result = std::filesystem::canonical(whereami::getModulePath().dirname())/path_from_args(L);
				lua_pushstring(L, result.string().c_str());
				return 1;
			});
		lua_setfield(L,-2,"LibraryDir");

		//ExecutableDir
		lua_pushcfunction(L, [](lua_State* const L) -> int
			{
				auto result = std::filesystem::canonical(whereami::getExecutablePath().dirname())/path_from_args(L);
				lua_pushstring(L, result.string().c_str());
				return 1;
			});
		lua_setfield(L,-2,"ExecutableDir");

		//ScriptDir
		lua_pushstring(L,Name.c_str());
		lua_pushstring(L,LogName.c_str());
		lua_pushcclosure(L, [](lua_State* const L) -> int
			{
				std::string name(lua_tostring(L, lua_upvalueindex(1)));
				std::string logname(lua_tostring(L, lua_upvalueindex(2)));

				//populate info for the stack-frame 1 down (our caller)
				lua_Debug ar;
				lua_getstack(L,1,&ar);
				lua_getinfo(L,"S",&ar);
				if(ar.source[0] == '@')
				{
					try
					{

						auto result = std::filesystem::canonical(std::string(ar.source).substr(1)).parent_path()/path_from_args(L);
						lua_pushstring(L, result.string().c_str());
						return 1;
					}
					catch(const std::exception& e)
					{
						if(auto log = odc::spdlog_get(logname))
							log->error("{}: GetPath.ScriptDir() called from lua; Exception '{}'.",name,e.what());
					}
				}
				else
				{
					if(auto log = odc::spdlog_get(logname))
						log->error("{}: GetPath.ScriptDir() called from lua without source file.",name);
				}
				lua_pushnil(L);
				return 1;
			},2);
		lua_setfield(L,-2,"ScriptDir");
	}
	lua_setfield(L,-2,"GetPath");

	//String2Hex
	lua_pushcfunction(L, [](lua_State* const L) -> int
		{
			size_t len;
			auto buf = lua_tolstring(L,-1,&len);
			lua_pushstring(L,odc::buf2hex((uint8_t*)buf,len).c_str());
			return 1;
		});
	lua_setfield(L,-2,"String2Hex");

	//Hex2String
	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
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
				std::string name(lua_tostring(L, lua_upvalueindex(1)));
				std::string logname(lua_tostring(L, lua_upvalueindex(2)));
				if(auto log = odc::spdlog_get(logname))
					log->error("{}: Hex2String() called from lua; Exception '{}'.",name,e.what());
				lua_pushnil(L);
				return 1;
			}
		},2);
	lua_setfield(L,-2,"Hex2String");


	//msTimerCallback() to get a callback after x ms
	//Two weak_ptr user data objects, plus two strings, makes 4 upvalues
	auto p = lua_newuserdatauv(L,sizeof(std::weak_ptr<void>),0);
	new(p) std::weak_ptr<void>(pSyncStrand);
	luaL_getmetatable(L, "std_weak_ptr_void");
	lua_setmetatable(L, -2);

	p = lua_newuserdatauv(L,sizeof(std::weak_ptr<void>),0);
	new(p) std::weak_ptr<void>(handler_tracker);
	luaL_getmetatable(L, "std_weak_ptr_void");
	lua_setmetatable(L, -2);

	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, ([](lua_State* const L) -> int
				   {
					   auto ppSync = static_cast<std::weak_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
					   auto sync = std::static_pointer_cast<asio::io_service::strand>(ppSync->lock());
					   auto ppTracker = static_cast<std::weak_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(2)));
					   auto tracker = ppTracker->lock();
					   std::string name(lua_tostring(L, lua_upvalueindex(3)));
					   std::string logname(lua_tostring(L, lua_upvalueindex(4)));

					   if(!tracker || !sync)
					   {
						   std::string msg("Something is terribly wrong. The Lua sync strand or the handler_tracker has been destroyed, while Lua is executing!");
						   if(auto log = odc::spdlog_get(logname))
							   log->error("{}: {}",name,msg);
						   throw std::runtime_error(msg);
					   }

					   if(lua_gettop(L) != 2 || !lua_isinteger(L,1) || !lua_isfunction(L,2))
					   {
						   if(auto log = odc::spdlog_get(logname))
							   log->error("{}: msTimerCallback() expects integer(ms) and function as parameters",name);
						   lua_pushnil(L);
						   return 1;
					   }
					   auto timer_ms = lua_tointeger(L,1);
					   auto LuaCBref = luaL_ref(L, LUA_REGISTRYINDEX);

					   std::shared_ptr<asio::steady_timer> pTimer = odc::asio_service::Get()->make_steady_timer();
					   pTimer->expires_from_now(std::chrono::milliseconds(timer_ms));
					   pTimer->async_wait(sync->wrap([L,tracker,name,logname,LuaCBref,pTimer](asio::error_code err)
						   {
							   //get callback from the registry back on stack
							   lua_rawgeti(L, LUA_REGISTRYINDEX, LuaCBref);
							   lua_pushstring(L,err.message().c_str());

							   //now call
							   const int argc = 1; const int retc = 0;
							   auto ret = lua_pcall(L,argc,retc,0);
							   if(ret != LUA_OK)
							   {
								   std::string call_err = lua_tostring(L, -1);
								   if(auto log = odc::spdlog_get(logname))
									   log->error("{}: Error calling timer lua callback: {}",name,call_err);
								   lua_pop(L,1);
							   }
							   //release the reference to the callback
							   luaL_unref(L, LUA_REGISTRYINDEX, LuaCBref);
						   }));
					   //return a cancel function
					   auto p = lua_newuserdatauv(L,sizeof(std::weak_ptr<void>),0);
					   new(p) std::weak_ptr<void>(pTimer);
					   luaL_getmetatable(L, "std_weak_ptr_void");
					   lua_setmetatable(L, -2);
					   lua_pushcclosure(L, [](lua_State* const L) -> int
						   {
							   auto ppTimer = static_cast<std::weak_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
							   auto pTimer = std::static_pointer_cast<asio::steady_timer>(ppTimer->lock());
							   if(pTimer) pTimer->cancel();
							   return 0;
						   },1);
					   return 1;
				   }),4);
	lua_setfield(L,-2,"msTimerCallback");

	//msCoroutineLoop() to run a coroutine in a timer loop
	//One weak_ptr user data object, plus two strings, makes 3 upvalues
	auto pUD = lua_newuserdatauv(L,sizeof(std::weak_ptr<void>),0);
	new(pUD) std::weak_ptr<void>(pSyncStrand);
	luaL_getmetatable(L, "std_weak_ptr_void");
	lua_setmetatable(L, -2);

	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, ([](lua_State* const L) -> int
				   {
					   auto ppSync = static_cast<std::weak_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
					   auto sync = std::static_pointer_cast<asio::io_service::strand>(ppSync->lock());
					   std::string name(lua_tostring(L, lua_upvalueindex(2)));
					   std::string logname(lua_tostring(L, lua_upvalueindex(3)));

					   if(!sync)
					   {
						   std::string msg("Something is terribly wrong. The Lua sync strand has been destroyed, while Lua is executing!");
						   if(auto log = odc::spdlog_get(logname))
							   log->error("{}: {}",name,msg);
						   throw std::runtime_error(msg);
					   }

					   if(lua_gettop(L) != 1 || !lua_isfunction(L,1))
					   {
						   if(auto log = odc::spdlog_get(logname))
							   log->error("{}: msCoroutineLoop() expects a single function/coroutine (that returns/yields ms to wait before re-calling/resuming)",name);
						   lua_pushnil(L);
						   return 1;
					   }
					   auto LuaCRref = luaL_ref(L, LUA_REGISTRYINDEX);

					   std::shared_ptr<asio::steady_timer> pTimer = odc::asio_service::Get()->make_steady_timer();
					   sync->post([=]()
						   {
							   CoroutineLoop(L,name,logname,LuaCRref,pTimer,sync);
						   });

					   //return a cancel function
					   auto p = lua_newuserdatauv(L,sizeof(std::weak_ptr<void>),0);
					   new(p) std::weak_ptr<void>(pTimer);
					   luaL_getmetatable(L, "std_weak_ptr_void");
					   lua_setmetatable(L, -2);
					   lua_pushcclosure(L, [](lua_State* const L) -> int
						   {
							   auto ppTimer = static_cast<std::weak_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
							   auto pTimer = std::static_pointer_cast<asio::steady_timer>(ppTimer->lock());
							   if(pTimer) pTimer->cancel();
							   return 0;
						   },1);
					   return 1;
				   }),3);
	lua_setfield(L,-2,"msCoroutineLoop");

	//SpawnDetached
	lua_pushstring(L,Name.c_str());
	lua_pushstring(L,LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string err_msg;
			int idx = 1;
			if(lua_isstring(L,idx))
			{
				std::string cmd = lua_tostring(L,idx++);
				std::vector<std::string> args;
				while(lua_isstring(L,idx))
					args.push_back(lua_tostring(L,idx++));
				try
				{
					lua_pushinteger(L,spawn_detached(cmd,args));
					return 1;
				}
				catch(const std::exception& e)
				{
					err_msg = e.what();
				}
			}
			else
				err_msg = "No command string provided.";

			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(auto log = odc::spdlog_get(logname))
				log->error("{}: SpawnDetached() called from lua; Exception '{}'.",name,err_msg);
			lua_pushnil(L);
			return 1;
		},2);
	lua_setfield(L,-2,"SpawnDetached");

	//SpawnAttached
	lua_pushstring(L, Name.c_str());
	lua_pushstring(L, LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string err_msg;
			int idx = 1;
			if(lua_isstring(L, idx))
			{
				std::string cmd = lua_tostring(L, idx++);
				std::vector<std::string> args;
				while(lua_isstring(L, idx))
					args.push_back(lua_tostring(L, idx++));
				try
				{
					auto result = spawn_attached(cmd, args);

					// Push pid
					lua_pushinteger(L, result.pid);

					// Create Lua file userdata objects from FILE*
					PushFile(L, result.stdin_file);
					PushFile(L, result.stdout_file);
					PushFile(L, result.stderr_file);

					return 4; // pid, stdin, stdout, stderr
				}
				catch(const std::exception& e)
				{
					err_msg = e.what();
				}
			}
			else
				err_msg = "No command string provided.";

			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(auto log = odc::spdlog_get(logname))
				log->error("{}: SpawnAttached() called from lua; Exception '{}'.", name, err_msg);
			lua_pushnil(L);
			lua_pushnil(L);
			lua_pushnil(L);
			lua_pushnil(L);
			return 4;
		}, 2);
	lua_setfield(L, -2, "SpawnAttached");

	// Lua binding for KillPid
	// KillPid(pid, [signal]) - send signal to process (default: 0 to check if alive)
	// Returns: true on success, false on fail
	lua_pushstring(L, Name.c_str());
	lua_pushstring(L, LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string err_msg;
			if(lua_isinteger(L, 1))
			{
				int pid = lua_tointeger(L, 1);
				int sig = 0; // Default: check if process exists
				if(lua_isinteger(L, 2))
					sig = lua_tointeger(L, 2);
				try
				{
					spawn_kill(pid, sig);
					lua_pushboolean(L, 1);
					return 1;
				}
				catch(const std::exception& e)
				{
					err_msg = e.what();
				}
			}
			else
			{
				err_msg = "First argument must be a PID (integer)";
			}

			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(auto log = odc::spdlog_get(logname))
				log->error("{}: Kill() called from lua; Error '{}'.", name, err_msg);
			lua_pushboolean(L, 0);
			return 1;
		}, 2);
	lua_setfield(L, -2, "KillPid");

	//KillSignal
	lua_newtable(L);
	struct SigDef { const char* name; int value; };
	const SigDef sigs[] =
	{
		#ifdef SIGSTOP
		{ "SIGSTOP", SIGSTOP },
		#endif
		#ifdef SIGCONT
		{ "SIGCONT", SIGCONT },
		#endif
		#ifdef SIGKILL
		{ "SIGKILL", SIGKILL },
		#endif
		#ifdef SIGQUIT
		{ "SIGQUIT", SIGQUIT },
		#endif
		#ifdef SIGHUP
		{ "SIGHUP", SIGHUP },
		#endif
		{ "SIGINT",  SIGINT  },
		{ "SIGABRT", SIGABRT },
		{ "SIGTERM", SIGTERM },
		{ "SIGEXIT", 0 },
	};

	for (const auto& s : sigs)
	{
		lua_pushstring(L, s.name);
		lua_pushinteger(L, s.value);
		lua_settable(L, -3);
	}
	lua_setfield(L, -2, "Kill");

	// Lua binding for WaitPid
	// WaitPid(pid, [nohang]) - wait for process to exit
	// If nohang is true, returns immediately if child hasn't exited
	// Returns on success: true, exit_code
	// Returns if not exited yet (with nohang): false, nil
	// Returns on error: nil, nil
	lua_pushstring(L, Name.c_str());
	lua_pushstring(L, LogName.c_str());
	lua_pushcclosure(L, [](lua_State* const L) -> int
		{
			std::string err_msg;
			if(lua_isinteger(L, 1))
			{
				int pid = lua_tointeger(L, 1);
				bool nohang = 0; // Default: wait
				if(lua_isboolean(L, 2))
					nohang = lua_toboolean(L, 2);
				try
				{
					auto [exited,status] = spawn_wait(pid,nohang);
					lua_pushboolean(L,exited);
					if(exited)
						lua_pushinteger(L,status);
					else
						lua_pushnil(L);
					return 2;
				}
				catch(const std::exception& e)
				{
					err_msg = e.what();
				}
			}
			else
			{
				err_msg = "First argument must be a PID (integer)";
			}

			std::string name(lua_tostring(L, lua_upvalueindex(1)));
			std::string logname(lua_tostring(L, lua_upvalueindex(2)));
			if(auto log = odc::spdlog_get(logname))
				log->error("{}: WaitPid() called from lua; Error '{}'.", name, err_msg);
			lua_pushnil(L);
			lua_pushstring(L, err_msg.c_str());
			return 2;
		}, 2);
	lua_setfield(L, -2, "WaitPid");
}

