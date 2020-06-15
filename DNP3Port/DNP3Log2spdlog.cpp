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
#include <opendnp3/LogLevels.h>

DNP3Log2spdlog::DNP3Log2spdlog()
{}

void DNP3Log2spdlog::Log( const openpal::LogEntry& arEntry )
{
	auto DNP3LevelName = opendnp3::LogFlagToString(arEntry.filters.GetBitfield());
	spdlog::level::level_enum spdlevel = FilterToLevel(arEntry.filters);
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->log(spdlevel, "{} - {} - {}",
			DNP3LevelName,
			arEntry.loggerid,
			arEntry.message);
}

spdlog::level::level_enum DNP3Log2spdlog::FilterToLevel(const openpal::LogFilters& filters)
{
	switch (filters.GetBitfield())
	{

		case (opendnp3::flags::EVENT):
			return spdlog::level::critical;
		case (opendnp3::flags::ERR):
			return spdlog::level::err;
		case (opendnp3::flags::WARN):
			return spdlog::level::warn;
		case (opendnp3::flags::INFO):
			return spdlog::level::info;
		case (opendnp3::flags::DBG):
			return spdlog::level::debug;
		case (opendnp3::flags::LINK_RX):
		case (opendnp3::flags::LINK_RX_HEX):
		case (opendnp3::flags::LINK_TX):
		case (opendnp3::flags::LINK_TX_HEX):
		case (opendnp3::flags::TRANSPORT_RX):
		case (opendnp3::flags::TRANSPORT_TX):
		case (opendnp3::flags::APP_HEADER_RX):
		case (opendnp3::flags::APP_OBJECT_RX):
		case (opendnp3::flags::APP_HEX_RX):
		case (opendnp3::flags::APP_HEADER_TX):
		case (opendnp3::flags::APP_OBJECT_TX):
		case (opendnp3::flags::APP_HEX_TX):
			return spdlog::level::trace;
		default:
			return spdlog::level::critical;
	}
}
