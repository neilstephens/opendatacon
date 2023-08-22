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

#include "LuaLogSink.h"
#include <json/json.h>

extern "C" LuaLogSink* new_LuaLogSink(const std::string& Name, const std::string& LuaFile, const Json::Value& Config)
{
	return new LuaLogSink(Name,LuaFile,Config);
}

extern "C" void delete_LuaLogSink(LuaLogSink* pLuaLogSink)
{
	delete pLuaLogSink;
	return;
}
