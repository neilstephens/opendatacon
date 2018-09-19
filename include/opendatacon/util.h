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
#include <spdlog/spdlog.h>

//fast rough random numbers
#define CONG(jcong) (jcong = 69069*jcong+1234567)
#define ZERO_TO_ONE(a) (CONG(a)*2.328306e-10)

namespace odc
{

typedef uint32_t rand_t;

void spdlog_register_logger(std::shared_ptr<spdlog::logger> logger);
std::shared_ptr<spdlog::logger> spdlog_get(const std::string &name);
void spdlog_drop(const std::string &name);

bool getline_noncomment(std::istream& is, std::string& line);
bool extract_delimited_string(std::istream& ist, std::string& extracted);
bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted);

bool GetBool(const std::string& value);

} //namspace odc

#endif //UTIL_H
