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

#include <filesystem>
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
void spdlog_flush_every(std::chrono::seconds interval);
void spdlog_apply_all(const std::function<void(std::shared_ptr<spdlog::logger>)> &fun);
void spdlog_register_logger(std::shared_ptr<spdlog::logger> logger);
std::shared_ptr<spdlog::logger> spdlog_get(const std::string &name);
void spdlog_drop(const std::string &name);
void spdlog_drop_all();
void spdlog_shutdown();

bool getline_noncomment(std::istream& is, std::string& line);
bool extract_delimited_string(std::istream& ist, std::string& extracted);
bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted);
std::string to_lower(const std::string& str);
std::string GetConfigVersion();
void SetConfigVersion(const std::string& Version);
std::size_t to_decimal(const std::string& binary);
std::string to_binary(std::size_t n, std::size_t size);
std::size_t bcd_encoded_to_decimal(const std::string& str);
std::string decimal_to_bcd_encoded_string(std::size_t n, std::size_t size);

typedef uint64_t msSinceEpoch_t;
inline msSinceEpoch_t msSinceEpoch()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>
		       (std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string since_epoch_to_datetime(msSinceEpoch_t milliseconds, std::string format = "%Y-%m-%d %H:%M:%S.%e", bool UTC = false);
msSinceEpoch_t datetime_to_since_epoch(std::string date_str, std::string format = "%Y-%m-%d %H:%M:%S.%e", bool UTC = false);
std::chrono::system_clock::time_point fs_to_sys_time_point(const std::filesystem::file_time_type& fstime);
std::filesystem::file_time_type sys_to_fs_time_point(const std::chrono::system_clock::time_point& systime);

std::string buf2hex(const uint8_t *data, size_t size);
std::vector<uint8_t> hex2buf(const std::string& hexStr);

uint16_t crc_ccitt(const uint8_t* const data, const size_t length, uint16_t crc = 0xFFFF, const uint16_t poly = 0x1021);

//Base64 code
//	From https://stackoverflow.com/a/37109258/6754618
const std::string b64encode(const void* data, const size_t &len);
const std::string b64decode(const void* data, const size_t &len);
std::string b64encode(const std::string& str);
std::string b64decode(const std::string& str64);

template<typename T>
std::shared_ptr<T> make_shared(T&& X);

} //namspace odc

#endif //UTIL_H
