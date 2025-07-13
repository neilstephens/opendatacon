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
 * TestDNP3Helpers.h
 *
 *  Created on: 01/09/2020
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ODCTESTHELPERS_H
#define ODCTESTHELPERS_H

#include <opendatacon/LogHelpers.h>
#include <opendatacon/util.h>
#include <opendatacon/Platform.h>
#include <spdlog/sinks/stdout_color_sinks.h>

extern spdlog::level::level_enum log_level;
static inline odc::LogHelpers Log{"opendatacon"};

inline void TestSetup()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);
}
inline void TestTearDown()
{
	odc::spdlog_flush_all();
	odc::spdlog_drop_all(); // Close off everything
}

#endif // ODCTESTHELPERS_H

