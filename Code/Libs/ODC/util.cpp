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
 * util.cpp
 *
 *  Created on: 14/03/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/util.h>
#include <opendatacon/Platform.h>
#include <regex>
#include <utility>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <exception>
#include <filesystem>
#include <atomic>

const std::size_t bcd_pack_size = 4;

namespace odc
{
static std::string ConfigVersion = "None";
std::string GetConfigVersion()
{
	return ConfigVersion;
}
void SetConfigVersion(const std::string& Version)
{
	ConfigVersion = Version;
}

static std::atomic_size_t LogRefreshSequenceNum = 0;
size_t GetLogRefreshSequenceNum(){ return LogRefreshSequenceNum.load(); }
void BumpLogRefreshSequenceNum(){ LogRefreshSequenceNum++; }

void spdlog_init_thread_pool(size_t q_size, size_t thread_count)
{
	spdlog::init_thread_pool(q_size, thread_count);
}

std::shared_ptr<spdlog::details::thread_pool> spdlog_thread_pool()
{
	return spdlog::thread_pool();
}

void spdlog_flush_all()
{
	spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& l) {l->flush(); });
}

void spdlog_flush_every(std::chrono::seconds interval)
{
	spdlog::flush_every(interval);
}

void spdlog_apply_all(const std::function<void(std::shared_ptr<spdlog::logger>)> &fun)
{
	spdlog::apply_all(fun);
}

void spdlog_register_logger(std::shared_ptr<spdlog::logger> logger)
{
	return spdlog::register_logger(std::move(logger));
}

std::shared_ptr<spdlog::logger> spdlog_get(const std::string &name)
{
	return spdlog::get(name);
}

void spdlog_drop(const std::string &name)
{
	spdlog::drop(name);
}

void spdlog_drop_all()
{
	spdlog::drop_all();
}

void spdlog_shutdown()
{
	spdlog::shutdown();
}

bool getline_noncomment(std::istream& is, std::string& line)
{
	//chew up blank lines and comments
	do
	{
		std::getline(is, line);
	} while(std::regex_match(line,std::regex("^[:space:]*#.*|^[^_[:alnum:]]*$")) && !is.eof());

	//fail if we hit the end
	if(is.eof())
		return false;
	//success!
	return true;
}

bool extract_delimited_string(std::istream& ist, std::string& extracted)
{
	extracted.clear();
	char delim;
	//The first non-whitespace char is the delimiter
	ist>>std::ws; //eat whitspace
	if((delim = ist.peek()) == EOF)
		return false; //nothing to extract - return failed (no delimetered string)

	auto reset_pos = ist.tellg();
	size_t offset = 1;
	while(ist.seekg(1,std::ios_base::cur))
	{
		if(ist.peek() == delim)
		{
			ist.seekg(reset_pos);
			char ch = '\0';
			ist.get(ch); //start delim
			while(--offset)
			{
				ist.get(ch);
				extracted.push_back(ch);
			}
			ist.get(ch); //end delim
			return true;
		}
		offset++;
	}
	//if we get to here, something has gone wrong
	ist.seekg(reset_pos);
	return false;
}

bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted)
{
	extracted.clear();
	char delim;
	//The first non-whitespace char is the delimiter
	ist>>std::ws; //eat whitspace
	if((delim = ist.peek()) == EOF)
		return false; //nothing to extract - return failed

	if(delims.find(delim) == std::string::npos)
	{
		/* not delimited so just extract until we get to a space or end of string */
		ist>>extracted;
		return true;
	}

	return extract_delimited_string(ist,extracted);
}

std::string to_lower(const std::string& str)
{
	std::string lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return lower;
}

/*
  function    : to_decimal
  description : convert binary to decimal
                for example
                1001 --> 9
                1111 --> 15
                0101 --> 5
  param       : binary, binary encoded string
  return      : decimal value of binary encoded string
*/
std::size_t to_decimal(const std::string& binary)
{
	std::size_t n = 0;
	for (int i = binary.size() - 1; i >= 0; --i)
		if (binary[i] == '1')
			n += (1 << (binary.size() - 1 - i));
	return n;
}

/*
  function    : to_binary
  description : this function converts a decimal value to a binary string
                this function also takes the size to have initial zero's
                for example 6 can be written in binary as 110.
                but if someone wants 6 in 4 bits we have to return 0110
                therefore second param size is required to let us know how big binary coded you want
  param       : n, the number to be converted to binary
  param       : size, the size of the binary enocded string
  return      : binary encoded string
*/
std::string to_binary(std::size_t n, std::size_t size)
{
	std::string binary(size, '0');
	for (int i = size - 1; i >= 0; --i)
	{
		binary[i] = (n & 1) + '0';
		n >>= 1;
	}
	return binary;
}

/*
  function    : bcd_encoded_to_decimal
  description : this function converts bcd encoded binary string to decmial value
                for example
                0101 0001 --> 51
                  01 0011 --> 13
                1000 0001 --> 81
  param       : str, binary encoded bcd string
  return      : decimal value
*/
std::size_t bcd_encoded_to_decimal(const std::string& str)
{
	// Now encode the bcd of packed 4 bits
	std::size_t decimal = 0;
	const std::size_t start = str.size() % bcd_pack_size;
	if (start)
	{
		std::string pack;
		for (std::size_t i = 0; i < start; ++i)
			pack += str[i];
		decimal = to_decimal(pack);
	}

	for (std::size_t i = 0; i < (str.size() / bcd_pack_size); ++i)
	{
		std::string pack;
		for (std::size_t j = 0; j < bcd_pack_size; ++j)
			pack += str[i * bcd_pack_size + j + start];
		decimal = (decimal * 10) + to_decimal(pack);
	}
	return decimal;
}

/*
  function    : decimal_to_bcd_encoded_string
  description : this function converts decimal encoded binary string
                for example
                0101 0001 --> 51
                  01 0011 --> 13
                1000 0001 --> 81
  param       : str, binary encoded bcd string
  return      : decimal value
*/
std::string decimal_to_bcd_encoded_string(std::size_t n, std::size_t size)
{
	const std::size_t sz = static_cast<std::size_t>(std::ceil(size / static_cast<double>(bcd_pack_size)) * bcd_pack_size);
	std::string decimal(sz, '0');
	int i = sz - bcd_pack_size;
	while (n)
	{
		const std::string s = to_binary(n % 10, bcd_pack_size);
		for (int j = i; j < i + static_cast<int>(bcd_pack_size); ++j)
			decimal[j] = s[j - i];
		n /= 10;
		i -= bcd_pack_size;
	}
	return decimal.substr(decimal.size() - size, size);
}

std::string since_epoch_to_datetime(msSinceEpoch_t milliseconds, std::string format, bool UTC)
{
	auto tm = UTC
	  ? spdlog::details::os::gmtime(milliseconds / 1000)
	  : spdlog::details::os::localtime(milliseconds / 1000);

	//do milliseconds ourself
	std::string milli_padded = "000";
	std::snprintf(milli_padded.data(),milli_padded.size()+1,"%03d",static_cast<int>(milliseconds % 1000));
	auto milli_pos = format.find("%e");
	while(milli_pos != format.npos)
	{
		format.erase(milli_pos, 2);
		format.insert(milli_pos, milli_padded);
		milli_pos = format.find("%e",milli_pos);
	}

	std::ostringstream ss;
	ss << std::put_time(&tm, format.c_str());

	return ss.str();
}

msSinceEpoch_t datetime_to_since_epoch(std::string date_str, std::string format, bool UTC)
{
	//do milliseconds ourself
	size_t msec = 0;
	auto milli_pos = format.find("%e");
	if(milli_pos != format.npos)
	{
		if(milli_pos != format.size()-2)
			throw std::runtime_error("datetime_to_since_epoch("+format+"): %e (millisecond) format only supported as suffix");
		format.resize(format.size()-2);
		//remove "xxx" milliseconds from the end
		auto msec_str = date_str.substr(date_str.size()-3);
		date_str.resize(date_str.size()-3);
		try
		{
			msec = std::stoi(msec_str);
		}
		catch(const std::exception& e)
		{
			throw std::runtime_error("datetime_to_since_epoch("+format+"): Error parsing milliseconds '"+msec_str+"'.");
		}
	}

	//parse the rest using iomanip
	std::istringstream date_iss(date_str);
	std::tm tm;
	date_iss >> std::get_time(&tm, format.c_str());
	if(date_iss.fail())
		throw std::runtime_error("datetime_to_since_epoch("+format+"): Error parsing datetime '"+date_str+"'.");

	//get_time leaves isdst undefined. -1 means let mktime decide
	tm.tm_isdst = -1;
	auto rawtime = time_t_from_tm_tz(tm,UTC);
	if (rawtime == -1)
		throw std::runtime_error("datetime_to_since_epoch("+format+"): Error converting to time_t.");

	auto time_point = std::chrono::system_clock::from_time_t(rawtime);
	return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count()+msec;
}

static const auto sec_from_fsepoch_to_sysepoch = []()
								 {
									 auto fs_now = std::filesystem::file_time_type::clock::now();
									 auto sys_now = std::chrono::system_clock::now();

									 auto fs_since_epoch = fs_now.time_since_epoch();
									 auto sys_since_epoch = sys_now.time_since_epoch();

									 //how long fs epoch is since system epoch - assume a round number of seconds
									 return std::chrono::round<std::chrono::seconds>(sys_since_epoch - fs_since_epoch);
								 } ();

std::chrono::system_clock::time_point fs_to_sys_time_point(const std::filesystem::file_time_type& fstime)
{
	auto time_since_fsepoch = fstime.time_since_epoch();
	auto converted = std::chrono::duration_cast<std::chrono::system_clock::duration>(time_since_fsepoch + sec_from_fsepoch_to_sysepoch);

	return std::chrono::system_clock::time_point(converted);
}

std::filesystem::file_time_type sys_to_fs_time_point(const std::chrono::system_clock::time_point& systime)
{
	auto time_since_sysepoch = systime.time_since_epoch();
	auto converted = std::chrono::duration_cast<std::filesystem::file_time_type::clock::duration>(time_since_sysepoch - sec_from_fsepoch_to_sysepoch);

	return std::filesystem::file_time_type(converted);
}

//this is faster than using stringstream to format to hex - minimises allocations
constexpr char hexnibble[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
std::string buf2hex(const uint8_t* data, size_t size)
{
	std::string str(size*2,'.');
	for (size_t i = 0; i < size; i++)
	{
		str[2*i] = hexnibble[(data[i] & 0xF0) >> 4];
		str[2*i+1] = hexnibble[data[i] & 0x0F];
	}
	return str;
}
uint8_t hexchar2byte(const char& hexChar)
{
	if(hexChar >= '0' && hexChar <= '9')
		return hexChar-'0';
	if(hexChar >= 'a' && hexChar <= 'f')
		return hexChar-'a'+10;
	if(hexChar >= 'A' && hexChar <= 'F')
		return hexChar-'A'+10;
	throw std::invalid_argument("hexchar2byte(): require characters [0-9a-fA-F]");
}
std::vector<uint8_t> hex2buf(const std::string& hexStr)
{
	if(hexStr.size()%2)
		throw std::invalid_argument("hex2buf(): require even number of hex nibbles to covert to bytes");
	const size_t bufSize = hexStr.size()/2;
	std::vector<uint8_t> buf(bufSize);
	for(size_t i = 0; i < bufSize; i++)
		buf[i] = (hexchar2byte(hexStr[i*2]) << 4) + hexchar2byte(hexStr[i*2+1]);
	return buf;
}

uint16_t crc_ccitt(const uint8_t* const data, const size_t length, uint16_t crc, const uint16_t poly)
{
	for(size_t i = 0; i < length; i++)
	{
		crc ^= static_cast<uint16_t>(data[i]) << 8;
		for(auto j = 0; j < 8; j++)
			crc = (crc & 0x8000) ? (crc << 1) ^ poly : crc << 1;
	}
	return crc;
}

//Base64 code
//	From https://stackoverflow.com/a/37109258/6754618
static const char* B64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int B64index[256] =
{
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
	0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
	0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

const std::string b64encode(const void* data, const size_t &len)
{
	std::string result((len + 2) / 3 * 4, '=');
	unsigned char *p = (unsigned char*) data;
	char *str = &result[0];
	size_t j = 0, pad = len % 3;
	const size_t last = len - pad;

	for (size_t i = 0; i < last; i += 3)
	{
		int n = int(p[i]) << 16 | int(p[i + 1]) << 8 | p[i + 2];
		str[j++] = B64chars[n >> 18];
		str[j++] = B64chars[n >> 12 & 0x3F];
		str[j++] = B64chars[n >> 6 & 0x3F];
		str[j++] = B64chars[n & 0x3F];
	}
	if (pad) /// Set padding
	{
		int n = --pad ? int(p[last]) << 8 | p[last + 1] : p[last];
		str[j++] = B64chars[pad ? n >> 10 & 0x3F : n >> 2];
		str[j++] = B64chars[pad ? n >> 4 & 0x03F : n << 4 & 0x3F];
		str[j++] = pad ? B64chars[n << 2 & 0x3F] : '=';
	}
	return result;
}

const std::string b64decode(const void* data, const size_t &len)
{
	if (len == 0) return "";

	unsigned char *p = (unsigned char*) data;
	size_t j = 0,
	       pad1 = len % 4 || p[len - 1] == '=',
	       pad2 = pad1 && (len % 4 > 2 || p[len - 2] != '=');
	const size_t last = (len - pad1) / 4 << 2;
	std::string result(last / 4 * 3 + pad1 + pad2, '\0');
	unsigned char *str = (unsigned char*) &result[0];

	for (size_t i = 0; i < last; i += 4)
	{
		int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
		str[j++] = n >> 16;
		str[j++] = n >> 8 & 0xFF;
		str[j++] = n & 0xFF;
	}
	if (pad1)
	{
		int n = B64index[p[last]] << 18 | B64index[p[last + 1]] << 12;
		str[j++] = n >> 16;
		if (pad2)
		{
			n |= B64index[p[last + 2]] << 6;
			str[j++] = n >> 8 & 0xFF;
		}
	}
	return result;
}

std::string b64encode(const std::string& str)
{
	return b64encode(str.c_str(), str.size());
}

std::string b64decode(const std::string& str64)
{
	return b64decode(str64.c_str(), str64.size());
}

//Define a little shared pointer factory
//this is used make shared pointers that will both construct and destroy from in libODC
//useful for EventInfo payloads that need shared pointers - because they will be passed accross shared lib / DLL boundaries
template<typename T>
std::shared_ptr<T> make_shared(T&& X)
{
	static auto del = [](T* ptr){delete ptr;};
	return std::shared_ptr<T>(new T{std::move(X)}, del);
}
#define MAKE_SHARED(T) template std::shared_ptr<T> make_shared(T&&)
//only clunky bit is that we have to instantiate for all the required underlying types
//add more here if you get linker errors
MAKE_SHARED(std::string);
MAKE_SHARED(std::vector<char>);
MAKE_SHARED(std::vector<uint8_t>);

} // namespace odc
