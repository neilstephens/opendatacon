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
 * COTP.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8073 / ITU-T X.224 transport protocol, class 0, as used by RFC 1006.
 */

#include "COTP.h"
#include <algorithm>
#include <stdexcept>

namespace iccp
{
namespace cotp
{

namespace
{
//variable part parameter codes
constexpr uint8_t ParamTpduSize = 0xC0;
constexpr uint8_t ParamCallingTsap = 0xC1;
constexpr uint8_t ParamCalledTsap = 0xC2;

//ISO 8073: at most 64 octets of user data in a DR
constexpr size_t MaxDrData = 64;
//LI, code, dst-ref(2), src-ref(2), class/option or reason
constexpr size_t FixedHeaderSize = 7;

void ParseVariablePart(const uint8_t* data, size_t pos, size_t end, bool interpret, Tpdu& out)
{
	while(pos < end)
	{
		if(end - pos < 2)
			throw DecodeError("COTP: truncated parameter header");
		const uint8_t code = data[pos];
		const size_t plen = data[pos + 1];
		pos += 2;
		if(end - pos < plen)
			throw DecodeError("COTP: parameter overruns TPDU");
		const uint8_t* value = data + pos;
		pos += plen;

		if(!interpret)
			continue; //structure checked, content ignored

		switch(code)
		{
			case ParamTpduSize:
				if(out.tpduSizeCode)
					throw DecodeError("COTP: duplicate TPDU size parameter");
				if(plen != 1 || !IsValidTpduSizeCode(value[0]))
					throw DecodeError("COTP: bad TPDU size parameter");
				out.tpduSizeCode = value[0];
				break;
			case ParamCallingTsap:
			case ParamCalledTsap:
			{
				auto& target = (code == ParamCallingTsap) ? out.callingTsap : out.calledTsap;
				if(target)
					throw DecodeError("COTP: duplicate TSAP parameter");
				if(plen > MaxTsapLength)
					throw DecodeError("COTP: TSAP too long");
				target = Bytes(value, value + plen);
				break;
			}
			default:
				break; //unknown/unsupported optional parameter (checksum, alternative class, ...)
		}
	}
}

void AppendParam(Bytes& out, uint8_t code, const uint8_t* value, size_t length)
{
	out.push_back(code);
	out.push_back(static_cast<uint8_t>(length));
	out.insert(out.end(), value, value + length);
}

void AppendRefs(Bytes& out, uint16_t dstRef, uint16_t srcRef)
{
	out.push_back(static_cast<uint8_t>(dstRef >> 8));
	out.push_back(static_cast<uint8_t>(dstRef & 0xFF));
	out.push_back(static_cast<uint8_t>(srcRef >> 8));
	out.push_back(static_cast<uint8_t>(srcRef & 0xFF));
}

Bytes EncodeConnect(const Tpdu& t)
{
	for(const auto* tsap : {&t.callingTsap, &t.calledTsap})
		if(*tsap && (*tsap)->size() > MaxTsapLength)
			throw std::invalid_argument("COTP: TSAP longer than 32 octets");
	if(t.tpduSizeCode && !IsValidTpduSizeCode(*t.tpduSizeCode))
		throw std::invalid_argument("COTP: invalid TPDU size code");

	Bytes out;
	out.push_back(0); //LI, filled in below
	out.push_back(static_cast<uint8_t>(t.type));
	AppendRefs(out, t.dstRef, t.srcRef);
	out.push_back(0); //class 0, no options

	if(t.callingTsap)
		AppendParam(out, ParamCallingTsap, t.callingTsap->data(), t.callingTsap->size());
	if(t.calledTsap)
		AppendParam(out, ParamCalledTsap, t.calledTsap->data(), t.calledTsap->size());
	if(t.tpduSizeCode)
		AppendParam(out, ParamTpduSize, &*t.tpduSizeCode, 1);

	out[0] = static_cast<uint8_t>(out.size() - 1);
	return out;
}
}

Tpdu Decode(const uint8_t* data, size_t length)
{
	if(length > 0 && data == nullptr)
		throw std::invalid_argument("COTP: null data");
	if(length < 2)
		throw DecodeError("COTP: TPDU too short");

	const size_t li = data[0];
	if(li == 0xFF)
		throw DecodeError("COTP: reserved length indicator");
	if(length < li + 1)
		throw DecodeError("COTP: truncated TPDU");

	Tpdu t;
	const uint8_t code = data[1] & 0xF0; //low nibble is CDT, which is 0 in class 0 and not checked

	switch(code)
	{
		case static_cast<uint8_t>(TpduType::Data):
			if(li != 2)
				throw DecodeError("COTP: bad DT length indicator");
			t.type = TpduType::Data;
			t.eot = (data[2] & 0x80) != 0; //TPDU-NR bits are 0 in class 0 and ignored
			t.data = data + DataHeaderSize;
			t.dataLength = length - DataHeaderSize;
			return t;

		case static_cast<uint8_t>(TpduType::ConnectRequest):
		case static_cast<uint8_t>(TpduType::ConnectConfirm):
			t.type = static_cast<TpduType>(code);
			if(li < FixedHeaderSize - 1)
				throw DecodeError("COTP: CR/CC too short");
			if(length != li + 1)
				throw DecodeError("COTP: user data not permitted in class 0 CR/CC");
			t.dstRef = static_cast<uint16_t>((data[2] << 8) | data[3]);
			t.srcRef = static_cast<uint16_t>((data[4] << 8) | data[5]);
			if((data[6] >> 4) != 0)
				throw DecodeError("COTP: only transport class 0 is supported");
			ParseVariablePart(data, FixedHeaderSize, li + 1, true, t);
			return t;

		case static_cast<uint8_t>(TpduType::DisconnectRequest):
			t.type = TpduType::DisconnectRequest;
			if(li < FixedHeaderSize - 1)
				throw DecodeError("COTP: DR too short");
			t.dstRef = static_cast<uint16_t>((data[2] << 8) | data[3]);
			t.srcRef = static_cast<uint16_t>((data[4] << 8) | data[5]);
			t.reason = data[6];
			ParseVariablePart(data, FixedHeaderSize, li + 1, false, t);
			t.data = data + li + 1;
			t.dataLength = length - (li + 1);
			return t;

		default:
			throw DecodeError("COTP: unsupported TPDU type");
	}
}

Bytes Encode(const Tpdu& t)
{
	if(t.dataLength > 0 && t.data == nullptr)
		throw std::invalid_argument("COTP: null data");

	switch(t.type)
	{
		case TpduType::ConnectRequest:
		case TpduType::ConnectConfirm:
			if(t.dataLength != 0)
				throw std::invalid_argument("COTP: user data not permitted in class 0 CR/CC");
			return EncodeConnect(t);

		case TpduType::DisconnectRequest:
		{
			if(t.dataLength > MaxDrData)
				throw std::invalid_argument("COTP: DR user data over 64 octets");
			Bytes out;
			out.push_back(static_cast<uint8_t>(FixedHeaderSize - 1));
			out.push_back(static_cast<uint8_t>(TpduType::DisconnectRequest));
			AppendRefs(out, t.dstRef, t.srcRef);
			out.push_back(t.reason);
			if(t.dataLength > 0)
				out.insert(out.end(), t.data, t.data + t.dataLength);
			return out;
		}

		case TpduType::Data:
		{
			Bytes out;
			out.reserve(DataHeaderSize + t.dataLength);
			out.push_back(2);
			out.push_back(static_cast<uint8_t>(TpduType::Data));
			out.push_back(t.eot ? 0x80 : 0x00);
			if(t.dataLength > 0)
				out.insert(out.end(), t.data, t.data + t.dataLength);
			return out;
		}
	}
	throw std::invalid_argument("COTP: unknown TPDU type");
}

Bytes EncodeConnectRequest(uint16_t srcRef, const std::optional<Bytes>& callingTsap,
	const std::optional<Bytes>& calledTsap, std::optional<uint8_t> tpduSizeCode)
{
	Tpdu t;
	t.type = TpduType::ConnectRequest;
	t.dstRef = 0;
	t.srcRef = srcRef;
	t.callingTsap = callingTsap;
	t.calledTsap = calledTsap;
	t.tpduSizeCode = tpduSizeCode;
	return Encode(t);
}

Bytes EncodeConnectConfirm(uint16_t dstRef, uint16_t srcRef, const std::optional<Bytes>& callingTsap,
	const std::optional<Bytes>& calledTsap, std::optional<uint8_t> tpduSizeCode)
{
	Tpdu t;
	t.type = TpduType::ConnectConfirm;
	t.dstRef = dstRef;
	t.srcRef = srcRef;
	t.callingTsap = callingTsap;
	t.calledTsap = calledTsap;
	t.tpduSizeCode = tpduSizeCode;
	return Encode(t);
}

Bytes EncodeDisconnectRequest(uint16_t dstRef, uint16_t srcRef, uint8_t reason)
{
	Tpdu t;
	t.type = TpduType::DisconnectRequest;
	t.dstRef = dstRef;
	t.srcRef = srcRef;
	t.reason = reason;
	return Encode(t);
}

Bytes EncodeData(const uint8_t* data, size_t length, bool eot)
{
	Tpdu t;
	t.type = TpduType::Data;
	t.eot = eot;
	t.data = data;
	t.dataLength = length;
	return Encode(t);
}

std::vector<Bytes> Segment(const uint8_t* tsdu, size_t length, uint8_t tpduSizeCode)
{
	if(!IsValidTpduSizeCode(tpduSizeCode))
		throw std::invalid_argument("COTP: invalid TPDU size code");
	if(length > 0 && tsdu == nullptr)
		throw std::invalid_argument("COTP: null data");

	const size_t maxData = TpduSizeOctets(tpduSizeCode) - DataHeaderSize;
	std::vector<Bytes> out;
	out.reserve(length / maxData + 1);
	size_t pos = 0;
	do
	{
		const size_t n = std::min(maxData, length - pos);
		pos += n;
		out.push_back(EncodeData(tsdu + (pos - n), n, pos == length));
	}
	while(pos < length);
	return out;
}

uint8_t NegotiateTpduSize(std::optional<uint8_t> proposedByCr, uint8_t localMaxCode)
{
	if(!IsValidTpduSizeCode(localMaxCode))
		throw std::invalid_argument("COTP: invalid local TPDU size code");
	const uint8_t proposed = proposedByCr.value_or(DefaultTpduSize);
	if(!IsValidTpduSizeCode(proposed))
		throw std::invalid_argument("COTP: invalid proposed TPDU size code");
	return std::min(proposed, localMaxCode);
}

uint8_t ConfirmTpduSize(std::optional<uint8_t> proposedByCr, std::optional<uint8_t> selectedByCc)
{
	const uint8_t proposed = proposedByCr.value_or(DefaultTpduSize);
	const uint8_t selected = selectedByCc.value_or(DefaultTpduSize);
	if(!IsValidTpduSizeCode(proposed))
		throw std::invalid_argument("COTP: invalid proposed TPDU size code");
	if(!IsValidTpduSizeCode(selected))
		throw DecodeError("COTP: invalid TPDU size in CC");
	if(selected > proposed)
		throw DecodeError("COTP: CC selects a TPDU size larger than proposed");
	return selected;
}

TsduReassembler::TsduReassembler(size_t maxTsduOctets, uint8_t maxTpduSizeCode):
	maxTsduSize(maxTsduOctets),
	maxTpduOctets(TpduSizeOctets(maxTpduSizeCode))
{}

void TsduReassembler::Reset()
{
	tsdu.clear();
	complete = false;
}

bool TsduReassembler::Add(const Tpdu& dt)
{
	if(dt.type != TpduType::Data)
	{
		Reset();
		throw DecodeError("COTP: not a DT TPDU");
	}
	if(complete)
	{
		Reset();
		throw DecodeError("COTP: previous TSDU not taken");
	}
	if(maxTpduOctets != 0 && dt.dataLength + DataHeaderSize > maxTpduOctets)
	{
		Reset();
		throw DecodeError("COTP: DT exceeds negotiated TPDU size");
	}
	if(dt.dataLength > maxTsduSize || tsdu.size() > maxTsduSize - dt.dataLength)
	{
		Reset();
		throw DecodeError("COTP: TSDU too large");
	}
	if(dt.dataLength > 0)
		tsdu.insert(tsdu.end(), dt.data, dt.data + dt.dataLength);
	complete = dt.eot;
	return complete;
}

Bytes TsduReassembler::Take()
{
	if(!complete)
		return Bytes();
	complete = false;
	Bytes out;
	out.swap(tsdu);
	return out;
}

} // namespace cotp
} // namespace iccp
