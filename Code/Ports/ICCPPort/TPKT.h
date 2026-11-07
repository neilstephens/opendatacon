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
 * TPKT.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TPKT framing (RFC 1006 section 6) with stream reassembly.
 *
 *  A TPKT is a 4 octet header - version (3), reserved (0), 16 bit big endian
 *  total length (header included) - followed by exactly one TPDU.
 *
 *  This is a pure byte-buffer codec, with no I/O. Input is untrusted: a bad
 *  header throws DecodeError and leaves the Reader in a failed state (the
 *  stream cannot be resynchronised, so the connection must be dropped).
 */

#ifndef ICCP_TPKT_H_
#define ICCP_TPKT_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>

namespace iccp
{
namespace tpkt
{

using ber::Bytes;
using ber::DecodeError;

constexpr uint8_t Version = 3;
constexpr size_t HeaderSize = 4;
//smallest legal TPKT: header + a class 0 DT TPDU header (RFC 1006: 7..65535)
constexpr size_t MinLength = 7;
constexpr size_t MaxLength = 0xFFFF;
constexpr size_t MaxPayload = MaxLength - HeaderSize;

//Wrap one TPDU in a TPKT. Throws std::length_error if the payload does not fit.
Bytes Frame(const uint8_t* tpdu, size_t length);
inline Bytes Frame(const Bytes& tpdu)
{
	return Frame(tpdu.data(), tpdu.size());
}

//Reassembles TPKTs from an arbitrarily chunked byte stream.
//Feed() whatever the socket delivers, then call Next() until it returns false.
class Reader
{
public:
	//Feed() refuses to buffer more than this many unconsumed octets
	static constexpr size_t DefaultMaxBuffered = 1024 * 1024;

	explicit Reader(size_t maxBuffered = DefaultMaxBuffered);

	//Append stream data. Throws DecodeError if the stream already failed or the
	//buffer limit would be exceeded (the caller is not draining with Next()).
	void Feed(const uint8_t* data, size_t length);
	void Feed(const Bytes& data)
	{
		Feed(data.data(), data.size());
	}

	//Extract the next complete TPKT payload (the TPDU) into `payload`.
	//Returns false if more data is needed. Throws DecodeError on a malformed header.
	bool Next(Bytes& payload);

	//octets received but not yet returned from Next()
	size_t Buffered() const
	{
		return buffer.size() - position;
	}
	bool Failed() const
	{
		return failed;
	}
	//discard everything, including the failed state (e.g. on reconnect)
	void Reset();

private:
	Bytes buffer;
	size_t position = 0;
	size_t maxBuffered;
	bool failed = false;
};

} // namespace tpkt
} // namespace iccp

#endif /* ICCP_TPKT_H_ */
