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

#ifndef LuaTransform_H_
#define LuaTransform_H_

#include <Lua/CLua.h>
#include <opendatacon/Transform.h>
#include <opendatacon/asio.h>

using namespace odc;

class LuaTransform: public Transform
{
public:
	LuaTransform(const std::string& Name, const Json::Value& params);
	~LuaTransform();

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		pLuaSyncStrand->post([this,event,pAllow,h{handler_tracker}]()
			{
				Event_(event,pAllow);
			});
	}

private:
	std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();
	//copy this to posted handlers so we can manage lifetime
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	std::shared_ptr<asio::io_service::strand> pLuaSyncStrand = pIOS->make_strand();

	//synchronised versions of pubilic counterpart above
	void Event_(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow);

	lua_State* LuaState = luaL_newstate();
};

#endif /* LuaTransform_H_ */
