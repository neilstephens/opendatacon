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
 * Session.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8327-1 / ITU-T X.225 session protocol subset.
 */

#include "Session.h"
#include <stdexcept>
#include <utility>

namespace iccp
{
namespace session
{

namespace
{
//parameter identifiers
constexpr uint8_t PgiConnectionIdentifier = 1;
constexpr uint8_t PgiConnectAcceptItem = 5;
constexpr uint8_t PiTransportDisconnect = 17;
constexpr uint8_t PiProtocolOptions = 19;
constexpr uint8_t PiRequirements = 20;
constexpr uint8_t PiTsduMaxSize = 21;
constexpr uint8_t PiVersion = 22;
constexpr uint8_t PiTokenSetting = 26;
constexpr uint8_t PiReasonCode = 50;
constexpr uint8_t PiCallingSelector = 51;
constexpr uint8_t PiCalledSelector = 52;
constexpr uint8_t PiUserData = 193;
constexpr uint8_t PiExtendedUserData = 194;

constexpr uint8_t SiData = 1;

//Read a 1 or 3 octet length at data[pos], advancing pos
size_t ReadLength(const uint8_t* data, size_t end, size_t& pos)
{
	if(pos >= end)
		throw DecodeError("Session: truncated length");
	const uint8_t first = data[pos++];
	if(first != 0xFF)
		return first;
	if(end - pos < 2)
		throw DecodeError("Session: truncated extended length");
	const size_t n = (static_cast<size_t>(data[pos]) << 8) | data[pos + 1];
	pos += 2;
	return n;
}

template<typename T>
void SetOnce(std::optional<T>& field, T value)
{
	if(field)
		throw DecodeError("Session: duplicate parameter");
	field = std::move(value);
}

void RequireLength(size_t actual, size_t expected)
{
	if(actual != expected)
		throw DecodeError("Session: bad parameter length");
}

void ParseParameters(const uint8_t* data, size_t pos, size_t end, bool allowGroups, Spdu& out)
{
	while(pos < end)
	{
		const uint8_t id = data[pos++];
		const size_t len = ReadLength(data, end, pos);
		if(end - pos < len)
			throw DecodeError("Session: parameter overruns SPDU");
		const uint8_t* value = data + pos;
		const size_t valueStart = pos;
		pos += len;

		switch(id)
		{
			case PgiConnectionIdentifier:
			case PgiConnectAcceptItem:
				if(allowGroups) //groups do not nest; a group inside a group is skipped
					ParseParameters(data, valueStart, valueStart + len, false, out);
				break;
			case PiTransportDisconnect:
				RequireLength(len, 1);
				SetOnce<uint8_t>(out.transportDisconnect, value[0]);
				break;
			case PiProtocolOptions:
				RequireLength(len, 1);
				SetOnce<uint8_t>(out.protocolOptions, value[0]);
				break;
			case PiRequirements:
				RequireLength(len, 2);
				SetOnce<uint16_t>(out.requirements, static_cast<uint16_t>((value[0] << 8) | value[1]));
				break;
			case PiTsduMaxSize:
			{
				RequireLength(len, 4);
				TsduMaxSize s;
				s.initiatorToResponder = static_cast<uint16_t>((value[0] << 8) | value[1]);
				s.responderToInitiator = static_cast<uint16_t>((value[2] << 8) | value[3]);
				SetOnce<TsduMaxSize>(out.tsduMaxSize, s);
				break;
			}
			case PiVersion:
				RequireLength(len, 1);
				SetOnce<uint8_t>(out.version, value[0]);
				break;
			case PiTokenSetting:
				RequireLength(len, 1);
				SetOnce<uint8_t>(out.tokenSetting, value[0]);
				break;
			case PiReasonCode:
				if(len < 1)
					throw DecodeError("Session: empty reason code");
				SetOnce<uint8_t>(out.reasonCode, value[0]);
				out.reasonText = len > 1 ? value + 1 : nullptr;
				out.reasonTextLength = len - 1;
				break;
			case PiCallingSelector:
			case PiCalledSelector:
				if(len > MaxSelectorLength)
					throw DecodeError("Session: selector too long");
				SetOnce<Bytes>(id == PiCallingSelector ? out.callingSelector : out.calledSelector, Bytes(value, value + len));
				break;
			case PiUserData:
			case PiExtendedUserData:
				if(out.hasUserData)
					throw DecodeError("Session: duplicate user data");
				out.hasUserData = true;
				out.userData = value;
				out.userDataLength = len;
				break;
			default:
				break; //unknown parameter (connection identifier items, enclosure, ...)
		}
	}
}

void AppendLength(Bytes& out, size_t n)
{
	if(n < 0xFF)
	{
		out.push_back(static_cast<uint8_t>(n));
		return;
	}
	if(n > MaxLength)
		throw std::length_error("Session: parameter too long");
	out.push_back(0xFF);
	out.push_back(static_cast<uint8_t>(n >> 8));
	out.push_back(static_cast<uint8_t>(n & 0xFF));
}

void AppendParameter(Bytes& out, uint8_t id, const uint8_t* value, size_t len)
{
	out.push_back(id);
	AppendLength(out, len);
	if(len > 0)
		out.insert(out.end(), value, value + len);
}

void AppendParameter(Bytes& out, uint8_t id, uint8_t value)
{
	AppendParameter(out, id, &value, 1);
}

void AppendSelector(Bytes& out, uint8_t id, const std::optional<Bytes>& selector)
{
	if(!selector)
		return;
	if(selector->size() > MaxSelectorLength)
		throw std::invalid_argument("Session: selector longer than 16 octets");
	AppendParameter(out, id, selector->data(), selector->size());
}
}

Spdu Decode(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("Session: null data");
	if(length < 2)
		throw DecodeError("Session: SPDU too short");

	Spdu s;
	switch(data[0])
	{
		case SiData:
			//only the concatenated GT + DT pair, both with no parameters, is supported
			if(length < 4 || data[1] != 0 || data[2] != SiData || data[3] != 0)
				throw DecodeError("Session: data SPDU is not GT+DT");
			s.type = SpduType::Data;
			s.hasUserData = true;
			s.userData = data + 4;
			s.userDataLength = length - 4;
			return s;

		case static_cast<uint8_t>(SpduType::Finish):
		case static_cast<uint8_t>(SpduType::Disconnect):
		case static_cast<uint8_t>(SpduType::Refuse):
		case static_cast<uint8_t>(SpduType::Connect):
		case static_cast<uint8_t>(SpduType::Accept):
		case static_cast<uint8_t>(SpduType::Abort):
		case static_cast<uint8_t>(SpduType::AbortAccept):
			s.type = static_cast<SpduType>(data[0]);
			break;

		default:
			throw DecodeError("Session: unsupported SPDU type");
	}

	size_t pos = 1;
	const size_t li = ReadLength(data, length, pos);
	if(length - pos < li)
		throw DecodeError("Session: truncated SPDU");
	const size_t end = pos + li;

	ParseParameters(data, pos, end, true, s);

	if(end < length) //user information following the parameter field
	{
		if(s.hasUserData)
			throw DecodeError("Session: user data given twice");
		s.hasUserData = true;
		s.userData = data + end;
		s.userDataLength = length - end;
	}
	if(s.type == SpduType::Refuse && !s.hasUserData && s.reasonTextLength > 0)
	{
		//a real-world stack sends the refusal's presentation PPDU inside the reason code parameter, with no user data parameter
		s.hasUserData = true;
		s.userData = s.reasonText;
		s.userDataLength = s.reasonTextLength;
	}
	return s;
}

Bytes Encode(const Spdu& s)
{
	if(s.userDataLength > 0 && s.userData == nullptr)
		throw std::invalid_argument("Session: null user data");

	if(s.type == SpduType::Data)
		return EncodeData(s.userData, s.userDataLength);

	Bytes body;

	//connect/accept item group
	Bytes item;
	if(s.protocolOptions)
		AppendParameter(item, PiProtocolOptions, *s.protocolOptions);
	if(s.tsduMaxSize)
	{
		const uint8_t v[4] = {static_cast<uint8_t>(s.tsduMaxSize->initiatorToResponder >> 8),
			                static_cast<uint8_t>(s.tsduMaxSize->initiatorToResponder & 0xFF),
			                static_cast<uint8_t>(s.tsduMaxSize->responderToInitiator >> 8),
			                static_cast<uint8_t>(s.tsduMaxSize->responderToInitiator & 0xFF)};
		AppendParameter(item, PiTsduMaxSize, v, 4);
	}
	if(s.version)
		AppendParameter(item, PiVersion, *s.version);
	if(s.tokenSetting)
		AppendParameter(item, PiTokenSetting, *s.tokenSetting);
	if(!item.empty())
		AppendParameter(body, PgiConnectAcceptItem, item.data(), item.size());

	if(s.transportDisconnect)
		AppendParameter(body, PiTransportDisconnect, *s.transportDisconnect);
	if(s.reasonCode)
		AppendParameter(body, PiReasonCode, *s.reasonCode);
	if(s.requirements)
	{
		const uint8_t v[2] = {static_cast<uint8_t>(*s.requirements >> 8), static_cast<uint8_t>(*s.requirements & 0xFF)};
		AppendParameter(body, PiRequirements, v, 2);
	}
	AppendSelector(body, PiCallingSelector, s.callingSelector);
	AppendSelector(body, PiCalledSelector, s.calledSelector);

	if(s.hasUserData)
	{
		if(s.userDataLength <= MaxUserData)
			AppendParameter(body, PiUserData, s.userData, s.userDataLength);
		else if(s.type == SpduType::Connect && s.userDataLength <= MaxExtendedUserData)
			AppendParameter(body, PiExtendedUserData, s.userData, s.userDataLength);
		else if(s.type == SpduType::Accept && s.userDataLength <= MaxExtendedUserData)
			AppendParameter(body, PiUserData, s.userData, s.userDataLength); //a real-world stack sends a long accept in PI 193 (not 194), with the 3 octet length [V]
		else
			throw std::invalid_argument("Session: user data too large for this SPDU");
	}

	Bytes out;
	out.reserve(body.size() + 4);
	out.push_back(static_cast<uint8_t>(s.type));
	AppendLength(out, body.size());
	out.insert(out.end(), body.begin(), body.end());
	return out;
}

Bytes EncodeData(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("Session: null data");
	Bytes out;
	out.reserve(4 + length);
	out.push_back(SiData);
	out.push_back(0);
	out.push_back(SiData);
	out.push_back(0);
	if(length > 0)
		out.insert(out.end(), data, data + length);
	return out;
}

} // namespace session
} // namespace iccp
