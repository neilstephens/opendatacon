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
 * LuaInst.h
 *
 *  Created on: 27/6/2023
 *      Author: Neil Stephens
 */
#ifndef LUAINST_H
#define LUAINST_H

#include <Lua/CLua.h>
#include <opendatacon/asio.h>
#include <json/json.h>
#include <functional>
#include <sstream>
#include <string>
#include <atomic>

using CommandHandler = std::function<Json::Value(const std::string& responder_name, const std::string& cmd, std::stringstream& args_iss)>;

class LuaInst
{
public:
	LuaInst(const std::string& lua_code, const CommandHandler& CmdHandler, const std::string& ID, const std::string& LoggerName);
	~LuaInst();
	bool Completed();
private:
	//only call Runner() on strand
	void Runner();

	std::atomic_bool cancelled = false;
	std::atomic_bool completed = false;
	lua_State* L = luaL_newstate();
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::shared_ptr<asio::io_service::strand> pStrand = pIOS->make_strand();
	std::shared_ptr<asio::steady_timer> pSleepTimer = pIOS->make_steady_timer();
	//copy this to posted handlers so we can manage lifetime
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	const CommandHandler& CmdHandler;
	const std::string ID;
	const std::string LoggerName;
};

#endif // LUAINST_H
