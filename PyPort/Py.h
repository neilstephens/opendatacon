/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 * Py.h
 *
 *  Created on: 26/6/2019
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef PY_H_
#define PY_H_

#include <Python.h>
//#include "../submodules/asio/asio/include/asio/io_service.hpp"
#include <opendatacon/util.h>

// If we are compiling for external testing (or production) define this.
// If we are using VS and its test framework, don't define this.
// The test cases do not destruct everthing correctly, they run individually ok, but one after the other they fail.
#define NONVSTESTING

// A cheat for it to find my Python module py files... comment out normally!
#define SCOTTPYTHONCODEPATH

// Hide some of the code to make Logging cleaner
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->debug(__VA_ARGS__);
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->error(__VA_ARGS__);
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("PyPort"))  \
		log->warn(__VA_ARGS__);
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("PyPort")) \
		log->info(__VA_ARGS__);

#endif /* PY_H_ */
