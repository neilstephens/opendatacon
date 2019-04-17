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
* KafkaUtility.h
*
*  Created on: 16/04/2019
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef KafkaENGINE_H_
#define KafkaENGINE_H_

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "Kafka.h"

bool iequals(const std::string& a, const std::string& b);

inline uint16_t ShiftLeftResult16Bits( uint16_t val, uint8_t shift)
{
	return numeric_cast<uint16_t>(val << shift);
}
inline uint16_t ShiftLeft8Result16Bits( uint16_t val)
{
	return numeric_cast<uint16_t>(val << 8);
}
inline uint32_t ShiftLeftResult32Bits(uint32_t val, uint8_t shift)
{
	return numeric_cast<uint32_t>(val << shift);
}
inline char ToChar(uint8_t v)
{
	return numeric_cast<char>(v);
}
int GetBitsSet(uint32_t data, int numberofbits);
int GetSetBit(uint32_t data, int numberofbits);

std::vector<std::string> split(const std::string& s, char delimiter);



template <class T>
std::string to_hexstring(T val)
{
	std::stringstream sstream;
	sstream << std::hex << val;
	return sstream.str();
}
template <class T>
std::string to_binstring(T val)
{
	std::stringstream sstream;
	sstream << std::setbase(2) << val;
	return sstream.str();
}

// Create an ASCII string version of the time from the Kafka time - which is msec since epoch.
std::string to_timestringfromKafkatime(KafkaTime _time);

void to_hhmmssmmfromKafkatime(KafkaTime _time, uint8_t &hh, uint8_t &mm, uint8_t &ss, uint16_t &msec);

// Return the current UTC offset in minutes.
int tz_offset();

#endif
