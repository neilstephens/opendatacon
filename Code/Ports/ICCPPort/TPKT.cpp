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
 * TPKT.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TPKT framing (RFC 1006 section 6) with stream reassembly.
 */

#include "TPKT.h"
#include <stdexcept>

namespace iccp
{
namespace tpkt
{

Bytes Frame(const uint8_t* tpdu, size_t length)
{
	if(length > MaxPayload)
		throw std::length_error("TPKT: TPDU too large for one TPKT");
	if(length > 0 && tpdu == nullptr)
		throw std::invalid_argument("TPKT: null TPDU");

	const size_t total = HeaderSize + length;
	Bytes out;
	out.reserve(total);
	out.push_back(Version);
	out.push_back(0);
	out.push_back(static_cast<uint8_t>(total >> 8));
	out.push_back(static_cast<uint8_t>(total & 0xFF));
	if(length > 0)
		out.insert(out.end(), tpdu, tpdu + length);
	return out;
}

Reader::Reader(size_t maxBufferedOctets):
	maxBuffered(maxBufferedOctets)
{}

void Reader::Reset()
{
	buffer.clear();
	position = 0;
	failed = false;
}

void Reader::Feed(const uint8_t* data, size_t length)
{
	if(failed)
		throw DecodeError("TPKT: stream is in a failed state");
	if(length == 0)
		return;
	if(data == nullptr)
		throw std::invalid_argument("TPKT: null data");

	//reclaim consumed space before appending
	if(position > 0)
	{
		buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(position));
		position = 0;
	}
	if(length > maxBuffered || buffer.size() > maxBuffered - length)
	{
		failed = true;
		throw DecodeError("TPKT: receive buffer limit exceeded");
	}
	buffer.insert(buffer.end(), data, data + length);
}

bool Reader::Next(Bytes& payload)
{
	if(failed)
		throw DecodeError("TPKT: stream is in a failed state");

	const size_t available = Buffered();
	if(available == 0)
		return false;

	const uint8_t* p = buffer.data() + position;

	//fail as early as possible on a bad version, rather than waiting for a whole header
	if(p[0] != Version)
	{
		failed = true;
		throw DecodeError("TPKT: bad version");
	}
	if(available < HeaderSize)
		return false;

	//the reserved octet (p[1]) is ignored on receipt
	const size_t total = (static_cast<size_t>(p[2]) << 8) | p[3];
	if(total < MinLength)
	{
		failed = true;
		throw DecodeError("TPKT: length too small");
	}
	if(available < total)
		return false;

	payload.assign(p + HeaderSize, p + total);
	position += total;
	if(position == buffer.size())
	{
		buffer.clear();
		position = 0;
	}
	return true;
}

} // namespace tpkt
} // namespace iccp
