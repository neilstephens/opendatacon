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
 * DNP3Log2spdlog.cpp
 *
 *  Created on: 2018-06-19
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "DNP3Log2spdlog.h"
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <opendnp3/logging/LogLevels.h>

DNP3Log2spdlog::DNP3Log2spdlog()
{}

void DNP3Log2spdlog::log(opendnp3::ModuleId module, const char* id, opendnp3::LogLevel level, char const* location, char const* message)
{
	auto DNP3LevelName = opendnp3::LogFlagToString(level);
	spdlog::level::level_enum spdlevel;
	if(level == opendnp3::flags::EVENT)
		spdlevel = spdlog::level::critical;
	else if(level == opendnp3::flags::ERR)
		spdlevel = spdlog::level::err;
	else if(level == opendnp3::flags::WARN)
		spdlevel = spdlog::level::warn;
	else if(level == opendnp3::flags::INFO)
		spdlevel = spdlog::level::info;
	else if(level == opendnp3::flags::DBG)
		spdlevel = spdlog::level::debug;
	else
		spdlevel = spdlog::level::trace;

	if(auto log = odc::spdlog_get("DNP3Port"))
		log->log(spdlevel, "{} - {} - {} - {} - {}",DNP3LevelName,module.value,id,location,message);
}
