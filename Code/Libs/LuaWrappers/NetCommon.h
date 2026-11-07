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
 * NetCommon.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  What the network objects of NetWrappers.cpp and HttpWrappers.cpp share: how they reach the port's Lua state, strand and logger.
 */
#ifndef NETCOMMON_H
#define NETCOMMON_H

#include <Lua/CLua.h>
#include <opendatacon/asio.h>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <memory>
#include <string>

namespace odcnet
{
//The port a network object belongs to. L is the port's main Lua state: only touched on the strand, while the tracker can be locked.
struct PortInfo
{
	lua_State* L;
	std::weak_ptr<odc::strand_t> strand;
	std::weak_ptr<void> tracker;
	std::string name;
	std::string logname;
};

inline void LogMsg(const std::string& logname, spdlog::level::level_enum level, const std::string& name, const std::string& msg)
{
	if(auto log = odc::spdlog_get(logname))
		if(log->should_log(level))
			log->log(level,"{}: {}",name,msg);
}
}

#endif // NETCOMMON_H
