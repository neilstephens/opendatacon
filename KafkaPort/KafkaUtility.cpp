/*	opendatacon
*
*	Copyright (c) 2019:
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
* KafkaUtility.cpp
*
*  Created on: 16/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "Kafka.h"
#include "KafkaUtility.h"


std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

// Case insensitive equals
bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b)
		{
			return tolower(a) == tolower(b);
		});
}

KafkaTime KafkaNow()
{
	// To get the time to pass through ODC events. Kafka Uses UTC time in commands - as you would expect.
	return static_cast<KafkaTime>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Create an ASCII string version of the time from the Kafka time - which is msec since epoch.
std::string to_timestringfromKafkatime(KafkaTime _time)
{
	time_t tp = _time / 1000; // time_t is normally seconds since epoch. We deal in msec!

	std::tm* t = std::localtime(&tp);
	if (t != nullptr)
	{
		char timestr[100];
		std::strftime(timestr, 100, "%c %z", t); // Local time format, with offset from UTC
		return std::string(timestr);
	}
	return "Time Conversion Problem";
}
void to_hhmmssmmfromKafkatime(KafkaTime _time, uint8_t &hh, uint8_t &mm, uint8_t &ss, uint16_t &msec)
{
	time_t tp = _time / 1000; // time_t is normally seconds since epoch. We deal in msec!
	msec = _time % 1000;

	std::tm* t = std::gmtime(&tp);
	if (t != nullptr)
	{
		hh = numeric_cast<uint8_t>(t->tm_hour);
		mm = numeric_cast<uint8_t>(t->tm_min);
		ss = numeric_cast<uint8_t>(t->tm_sec);
	}
}

int tz_offset()
{
	time_t when = std::time(nullptr);
	auto const tm = *std::localtime(&when);
	char tzoffstr[6];
	std::strftime(tzoffstr, 6, "%z", &tm);
	std::string s(tzoffstr);
	int h = std::stoi(s.substr(0, 3), nullptr, 10);
	int m = std::stoi(s[0] + s.substr(3), nullptr, 10);

	return h * 60 + m;
}
// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a hex string in the format of "FF120D567200" and turns it into the actual hex equivalent string
std::string BuildBinaryStringFromASCIIHexString(const std::string &as)
{
	assert(as.size() % 2 == 0); // Must be even length

	// Create, we know how big it will be
	auto res = std::string(as.size() / 2, 0);

	// Take chars in chunks of 2 and convert to a hex equivalent
	for (size_t i = 0; i < (as.size() / 2); i++)
	{
		auto hexpair = as.substr(i * 2, 2);
		res[i] = static_cast<char>(std::stol(hexpair, nullptr, 16));
	}
	return res;
}
// A little helper function to make the formatting of the required strings simpler, so we can cut and paste from WireShark.
// Takes a binary string, and produces an ascii hex string in the format of "FF120D567200"g
std::string BuildASCIIHexStringfromBinaryString(const std::string &bs)
{
	// Create, we know how big it will be
	auto res = std::string(bs.size() * 2, 0);

	constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7','8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	for (size_t i = 0; i < bs.size(); i++)
	{
		res[2 * i] = hexmap[(bs[i] & 0xF0) >> 4];
		res[2 * i + 1] = hexmap[bs[i] & 0x0F];
	}

	return res;
}
