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
 * LuaPort.h
 *
 *  Created on: 17/06/2023
 *      Author: Neil Stephens
 */

#ifndef LuaPort_H_
#define LuaPort_H_

#include <Lua/DynamicSymbols.h>
#include <Lua/CLua.h>
#include <opendatacon/DataPort.h>

using namespace odc;

class LuaPort: public DataPort
{
public:
	LuaPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
	~LuaPort();

	void Enable() override
	{
		pLuaSyncStrand->post([this,h{handler_tracker}](){Enable_();});
	}

	void Disable() override
	{
		pLuaSyncStrand->post([this,h{handler_tracker}](){Disable_();});
	}

	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override
	{
		pLuaSyncStrand->post([=,h{handler_tracker}](){Event_(event,SenderName,pStatusCallback);});
	}

	void ProcessElements(const Json::Value& JSONRoot) override;

private:
	Lua::DynamicSymbols lua_syms; //in case lua modules need to resolve symbols

	//copy this to posted handlers so we can manage lifetime
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	std::shared_ptr<asio::io_service::strand> pLuaSyncStrand = pIOS->make_strand();

	//synchronised versions of pubil counterparts above
	void Enable_();
	void Disable_();
	void Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback);

	void ExportLuaPublishEvent();
	void ExportLuaInDemand();
	void CallLuaGlobalVoidVoidFunc(const std::string& FnName);

	lua_State* LuaState = luaL_newstate();
	Json::Value JSONConf;
};

#endif /* LuaPort_H_ */
