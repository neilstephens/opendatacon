/*	opendatacon
 *
 *	Copyright (c) 2018:
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
  * ODCLogMacros.h
  *
  *  Created on: 11-09-2018
  *      Author: Scott Ellis - scott.ellis@novatex.com.au
  */

#ifndef ODCLOGMACROS
#define ODCLOGMACROS

#include <opendatacon/util.h>

using namespace odc;

#define LOGTRACE(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
	log->trace(__VA_ARGS__)
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
	log->debug(__VA_ARGS__)
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
	log->error(__VA_ARGS__)
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("opendatacon"))  \
	log->warn(__VA_ARGS__)
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
	log->info(__VA_ARGS__)
#define LOGCRITICAL(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
	log->critical(__VA_ARGS__)

#endif