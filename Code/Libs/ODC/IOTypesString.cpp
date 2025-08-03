/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License")
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
 * IOTypesString.cpp
 *
 *  Created on: 2024-12-10
 *      Author: Neil Stephens
 */

#include "opendatacon/util.h"
#include <cstdint>
#include <exception>
#include <opendatacon/IOTypes.h>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <string>
#include <sys/types.h>

namespace odc
{

bool ExtractBool(std::istringstream& is)
{
	auto result = false;
	is.setf(std::ios_base::boolalpha);
	is >> result;

	if (is.fail())
	{
		is.clear();
		is.unsetf(std::ios_base::boolalpha);
		is >> result;
		if (is.fail())
			throw std::invalid_argument("Payload string is not convertable to bool: "+is.str());
	}

	return result;
}

template <> bool PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	return ExtractBool(is);
}

template<> DBB PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	auto b1 = ExtractBool(is);
	auto b2 = ExtractBool(is);
	return {b1, b2};
}
template<> double PayloadFromString(const std::string& PayloadStr)
{
	try
	{
		return std::stod(PayloadStr);
	}
	catch(const std::exception& e)
	{
		throw std::invalid_argument("Payload string is not convertable to double: "+PayloadStr);
	}
}
template<> uint32_t PayloadFromString(const std::string& PayloadStr)
{
	try
	{
		return std::stoul(PayloadStr);
	}
	catch(const std::exception& e)
	{
		throw std::invalid_argument("Payload string is not convertable to uint32_t: "+PayloadStr);
	}
}
template<> CommandStatus PayloadFromString(const std::string& PayloadStr)
{
	int i;
	try
	{
		i = std::stoi(PayloadStr);
	}
	catch(const std::exception& e)
	{
		//not an int, try to match enum string
		auto cs = CommandStatusFromString(PayloadStr);
		if(cs == CommandStatus::UNDEFINED && PayloadStr != "UNDEFINED")
			throw std::invalid_argument("Payload string is not convertable to CommandStatus: "+PayloadStr);
		return cs;
	}

	auto cs = CommandStatusFromInt(i);
	if(cs == CommandStatus::UNDEFINED && i != static_cast<int>(CommandStatus::UNDEFINED))
		throw std::invalid_argument("Payload string is not convertable to CommandStatus: "+PayloadStr);
	return cs;
}
OctetStringBuffer PayloadFromString(const std::string& PayloadStr, DataToStringMethod D2S)
{
	if(D2S == DataToStringMethod::Raw)
		return OctetStringBuffer(std::string(PayloadStr));

	if(D2S == DataToStringMethod::Hex)
	{
		std::regex hex_regex("[0-9a-fA-F]+");
		if(!std::regex_match(PayloadStr, hex_regex))
			throw std::invalid_argument("Payload string is not a valid hex string: "+PayloadStr);
		return OctetStringBuffer(hex2buf(PayloadStr));
	}

	if(D2S == DataToStringMethod::Base64)
	{
		std::regex base64_regex("[A-Za-z0-9+/]+={0,2}");
		if(!std::regex_match(PayloadStr, base64_regex))
			throw std::invalid_argument("Payload string is not a valid base64 string: "+PayloadStr);
		return OctetStringBuffer(b64decode(PayloadStr));
	}

	throw std::invalid_argument("Unknown DataToStringMethod: "+std::to_string(static_cast<int>(D2S)));
}
template<> TAI PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	msSinceEpoch_t t;
	is >> t;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to TAI: "+PayloadStr);

	auto next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
		is.ignore();
	else
		throw std::invalid_argument("Payload string is not convertable to TAI: "+PayloadStr);

	uint32_t ms;
	is >> ms;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to TAI: "+PayloadStr);

	next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
		is.ignore();
	else
		throw std::invalid_argument("Payload string is not convertable to TAI: "+PayloadStr);

	uint8_t us;
	is >> us;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to TAI: "+PayloadStr);

	return {t, ms, us};
}
template<> SS PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	int16_t s1;
	is >> s1;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to SS: "+PayloadStr);

	auto next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
		is.ignore();
	else
		throw std::invalid_argument("Payload string is not convertable to SS: "+PayloadStr);

	uint32_t s2;
	is >> s2;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to SS: "+PayloadStr);

	return {s1, s2};
}
template<> AbsTime_n_SysOffs PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	msSinceEpoch_t AbsTime;
	is >> AbsTime;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to time sync payload: "+PayloadStr);

	auto next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
		is.ignore();
	else
		throw std::invalid_argument("Payload string is not convertable to time sync payload: "+PayloadStr);

	int64_t SysOffs;
	is >> SysOffs;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to time sync payload: "+PayloadStr);

	return {AbsTime, SysOffs};
}
template<> ControlRelayOutputBlock PayloadFromString(const std::string& PayloadStr)
{
	ControlCode functionCode = ControlCode::LATCH_ON;
	uint8_t count = 1;
	uint32_t onTimeMS = 100;
	uint32_t offTimeMS = 100;
	CommandStatus status = CommandStatus::SUCCESS;

	auto PayloadStrCopy = PayloadStr;
	//regex replace [:;,] with '|'
	std::replace_if(PayloadStrCopy.begin(), PayloadStrCopy.end(), [](char c) { return c == ':' || c == ';' || c == ','; }, '|');

	//now string format: |<functionCode>|[Count ]<x>|[ON ]<x>[ms]|[OFF ]<x>[ ms]|[<CommandStatus>|]

	auto regex_str = R"(\|([A-Z_]+|[0-9]+)\|(Count )?([0-9]+)\|(ON )?([0-9]+)( ms)?\|(OFF )?([0-9]+)( ms)?\|(\|([0-9]+|[A-Z_])\|)?)";
	std::regex crb_regex(regex_str);
	std::smatch match;

	// functionCode: \1
	// count: \3
	// onTime: \5
	// offTime: \8
	// status: \11

	if(std::regex_search(PayloadStrCopy, match, crb_regex))
	{
		//FnCode
		int i;
		try
		{
			i = std::stoi(match[1].str());
			functionCode = ControlCodeFromInt(i);
			if(functionCode == ControlCode::UNDEFINED && i != static_cast<int>(ControlCode::UNDEFINED))
				throw std::exception();
		}
		catch(const std::exception& e)
		{
			functionCode = ControlCodeFromString(match[1].str());
			if(functionCode == ControlCode::UNDEFINED && match[1].str() != "UNDEFINED")
				throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);
		}
		//Count
		try
		{
			count = std::stoi(match[3].str());
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);
		}
		//OnTime
		try
		{
			onTimeMS = std::stoi(match[5].str());
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);
		}
		//OffTime
		try
		{
			offTimeMS = std::stoi(match[8].str());
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);
		}
		//Status
		if(match[11].matched)
		{
			try
			{
				status = PayloadFromString<CommandStatus>(match[11].str());
			}
			catch(const std::exception& e)
			{
				throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);
			}
		}
	}
	else
		throw std::invalid_argument("Payload string is not convertable to ControlRelayOutputBlock: "+PayloadStr);

	return {functionCode, count, onTimeMS, offTimeMS, status};
}
template<> AOD PayloadFromString(const std::string& PayloadStr)
{
	std::istringstream is(PayloadStr);
	double d;
	is >> d;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to AOD: "+PayloadStr);

	CommandStatus cs = CommandStatus::SUCCESS;
	if(is.eof())
		return {d, cs}
	;

	auto next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
	{
		is.ignore();
		try
		{
			cs = PayloadFromString<CommandStatus>(is.str());
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("Payload string is not convertable to AOD: "+PayloadStr);
		}
	}
	return {d, cs};
}
template<> AOF PayloadFromString(const std::string& PayloadStr)
{
	//parse as AOD and check if the value fits into float
	AOD aod;
	try
	{
		aod = PayloadFromString<AOD>(PayloadStr);
	}
	catch(const std::exception& e)
	{
		throw std::invalid_argument("Payload string is not convertable to AOF: "+PayloadStr);
	}
	if(aod.first > std::numeric_limits<float>::max() || aod.first < std::numeric_limits<float>::lowest())
		throw std::invalid_argument("Payload string is not convertable to AOF: "+PayloadStr);
	return {aod.first, aod.second};
}
template<> AO32 PayloadFromString(const std::string& PayloadStr)
{
	//similar to AOD, but with 32 bit int
	std::istringstream is(PayloadStr);
	uint32_t i;
	is >> i;
	if(is.fail())
		throw std::invalid_argument("Payload string is not convertable to AO32: "+PayloadStr);

	CommandStatus cs = CommandStatus::SUCCESS;
	if(is.eof())
		return {i, cs}
	;

	auto next_ch = is.peek();
	if(next_ch == ',' || next_ch == ';' || next_ch == '|' || next_ch == ':' || next_ch == ' ')
	{
		is.ignore();
		try
		{
			cs = PayloadFromString<CommandStatus>(is.str());
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("Payload string is not convertable to AO32: "+PayloadStr);
		}
	}
	return {i, cs};
}
template<> AO16 PayloadFromString(const std::string& PayloadStr)
{
	//parse as AO32 and check if the value fits into 16 bit int
	AO32 ao32;
	try
	{
		ao32 = PayloadFromString<AO32>(PayloadStr);
	}
	catch(const std::exception& e)
	{
		throw std::invalid_argument("Payload string is not convertable to AO16: "+PayloadStr);
	}
	if(ao32.first > std::numeric_limits<int16_t>::max() || ao32.first < std::numeric_limits<int16_t>::lowest())
		throw std::invalid_argument("Payload string is not convertable to AO16: "+PayloadStr);
	return {static_cast<int16_t>(ao32.first), ao32.second};
}
template<> QualityFlags PayloadFromString(const std::string& PayloadStr)
{
	return QualityFlagsFromString(PayloadStr);
}
template<> char PayloadFromString(const std::string& PayloadStr)
{
	if(PayloadStr.size() != 1)
		throw std::invalid_argument("Payload string is not convertable to char: "+PayloadStr);
	return PayloadStr[0];
}
template<> ConnectState PayloadFromString(const std::string& PayloadStr)
{
	auto cs = ConnectStateFromString(PayloadStr);
	if(cs == ConnectState::UNDEFINED && PayloadStr != "UNDEFINED")
		throw std::invalid_argument("Payload string is not convertable to ConnectState: "+PayloadStr);
	return cs;
}

} //namespace odc
