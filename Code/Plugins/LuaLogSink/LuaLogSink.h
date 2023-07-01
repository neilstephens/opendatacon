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
 * LuaLogSink.h
 *
 *  Created on: 30/6/2023
 *      Author: Neil Stephens
 */
#ifndef LUALOGSINK_H
#define LUALOGSINK_H

#include <Lua/CLua.h>
#include <opendatacon/asio.h>
#include <opendatacon/Platform.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <string>

class LuaLogSink: public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
public:
	LuaLogSink(const std::string& Name, const std::string& LuaFile);
	~LuaLogSink();
	void sink_it_(const spdlog::details::log_msg &msg) final;
	void flush_() final;
private:
	const std::string Name;
	lua_State* L = luaL_newstate();
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	std::shared_ptr<asio::io_service::strand> pStrand = pIOS->make_strand();
	//copy this to posted handlers so we can manage lifetime
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
};

#endif // LUALOGSINK_H
