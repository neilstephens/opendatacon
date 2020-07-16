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

#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cstdint>
#include <opendatacon/spdlog.h>
#include <spdlog/async.h>

namespace odc
{

void spdlog_init_thread_pool(size_t q_size, size_t thread_count);
std::shared_ptr<spdlog::details::thread_pool> spdlog_thread_pool();
void spdlog_flush_all();
void spdlog_apply_all(const std::function<void(std::shared_ptr<spdlog::logger>)> &fun);
void spdlog_register_logger(std::shared_ptr<spdlog::logger> logger);
std::shared_ptr<spdlog::logger> spdlog_get(const std::string &name);
void spdlog_drop(const std::string &name);
void spdlog_drop_all();
void spdlog_shutdown();

bool getline_noncomment(std::istream& is, std::string& line);
bool extract_delimited_string(std::istream& ist, std::string& extracted);
bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted);

std::string GetConfigVersion();
void SetConfigVersion(std::string Version);

} //namspace odc

#endif //UTIL_H
