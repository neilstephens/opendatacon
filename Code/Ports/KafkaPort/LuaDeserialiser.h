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
 * LuaDeserialiser.h
 *
 *  Created on: 2024-12-08
 *      Author: Neil Stephens
 */
#ifndef Lua_DESERIALISER_H
#define Lua_DESERIALISER_H

#include "Deserialiser.h"

class LuaDeserialiser: public Deserialiser
{
public:
	LuaDeserialiser(const std::string& lua_file, const std::string& lua_code, const std::string& datetime_format, const bool utc):
		Deserialiser(datetime_format,utc)
	{}
	virtual ~LuaDeserialiser() = default;

	std::shared_ptr<EventInfo> Deserialise(const KCC::ConsumerRecord& record) override
	{
		//TODO: implement Lua deserialisation
		auto val_str = record.value().toString();
		return nullptr;
	}
};

#endif // Lua_DESERIALISER_H
