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
 * TPKTTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for TPKT framing and stream reassembly
 */

#include "TPKT.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP TPKT - " name

namespace
{
using namespace iccp::tpkt;

Bytes Hex(const std::string& s)
{
	Bytes out;
	int hi = -1;
	for(char c : s)
	{
		int v;
		if(c >= '0' && c <= '9') v = c - '0';
		else if(c >= 'a' && c <= 'f') v = c - 'a' + 10;
		else if(c >= 'A' && c <= 'F') v = c - 'A' + 10;
		else continue;
		if(hi < 0) hi = v;
		else
		{
			out.push_back(static_cast<uint8_t>(hi * 16 + v));
			hi = -1;
		}
	}
	return out;
}

//a valid TPKT whose payload is `n` octets of a counting pattern
Bytes MakeTpkt(size_t n, uint8_t seed = 0)
{
	Bytes payload(n);
	for(size_t i = 0; i < n; ++i)
		payload[i] = static_cast<uint8_t>(seed + i);
	return Frame(payload);
}
}

TEST_CASE(SUITE("Frame writes the header"))
{
	const Bytes payload = Hex("02 f0 80 aa bb");
	REQUIRE(Frame(payload) == Hex("03 00 00 09 02 f0 80 aa bb"));

	//length is big endian and counts the header
	const Bytes big = Frame(Bytes(0x1234 - 4, 0x55));
	REQUIRE(big.size() == 0x1234);
	REQUIRE(big[2] == 0x12);
	REQUIRE(big[3] == 0x34);
}

TEST_CASE(SUITE("Frame size limits"))
{
	REQUIRE(Frame(Bytes(MaxPayload, 1)).size() == MaxLength);
	REQUIRE_THROWS_AS(Frame(Bytes(MaxPayload + 1, 1)), std::length_error);
	REQUIRE_THROWS_AS(Frame(nullptr, 5), std::invalid_argument);
}

TEST_CASE(SUITE("Reader extracts a complete TPKT"))
{
	Reader r;
	const Bytes wire = Hex("03 00 00 0c 02 f0 80 01 02 03 04 05");
	r.Feed(wire);
	Bytes payload;
	REQUIRE(r.Next(payload));
	REQUIRE(payload == Hex("02 f0 80 01 02 03 04 05"));
	REQUIRE(r.Buffered() == 0);
	REQUIRE_FALSE(r.Next(payload));
}

TEST_CASE(SUITE("Reader ignores the reserved octet"))
{
	Reader r;
	r.Feed(Hex("03 ff 00 07 02 f0 80"));
	Bytes payload;
	REQUIRE(r.Next(payload));
	REQUIRE(payload == Hex("02 f0 80"));
}

TEST_CASE(SUITE("Reader returns false on nothing / partial input"))
{
	Reader r;
	Bytes payload;
	REQUIRE_FALSE(r.Next(payload));
	r.Feed(nullptr, 0);
	REQUIRE_FALSE(r.Next(payload));
	r.Feed(Hex("03"));
	REQUIRE_FALSE(r.Next(payload));
	r.Feed(Hex("00 00"));
	REQUIRE_FALSE(r.Next(payload));
	r.Feed(Hex("0c 02 f0 80 01"));
	REQUIRE_FALSE(r.Next(payload));
	REQUIRE(r.Buffered() == 8);
	r.Feed(Hex("02 03 04 05"));
	REQUIRE(r.Next(payload));
	REQUIRE(payload.size() == 8);
}

TEST_CASE(SUITE("Reader reassembles a stream split at every possible point"))
{
	const Bytes a = MakeTpkt(10, 1);
	const Bytes b = MakeTpkt(300, 50);
	Bytes stream = a;
	stream.insert(stream.end(), b.begin(), b.end());

	for(size_t cut = 0; cut <= stream.size(); ++cut)
	{
		Reader r;
		std::vector<Bytes> got;
		Bytes payload;
		r.Feed(stream.data(), cut);
		while(r.Next(payload))
			got.push_back(payload);
		r.Feed(stream.data() + cut, stream.size() - cut);
		while(r.Next(payload))
			got.push_back(payload);

		REQUIRE(got.size() == 2);
		REQUIRE(got[0] == Bytes(a.begin() + HeaderSize, a.end()));
		REQUIRE(got[1] == Bytes(b.begin() + HeaderSize, b.end()));
		REQUIRE(r.Buffered() == 0);
	}
}

TEST_CASE(SUITE("Reader reassembles a byte at a time"))
{
	Bytes stream;
	std::vector<Bytes> expected;
	for(size_t n : {3u, 3u, 100u, 7u, 1000u, 3u})
	{
		const Bytes t = MakeTpkt(n, static_cast<uint8_t>(n));
		stream.insert(stream.end(), t.begin(), t.end());
		expected.emplace_back(t.begin() + HeaderSize, t.end());
	}

	Reader r;
	std::vector<Bytes> got;
	Bytes payload;
	for(uint8_t byte : stream)
	{
		r.Feed(&byte, 1);
		while(r.Next(payload))
			got.push_back(payload);
	}
	REQUIRE(got == expected);
}

TEST_CASE(SUITE("Reader handles many TPKTs in a single feed"))
{
	Bytes stream;
	for(int i = 0; i < 50; ++i)
	{
		const Bytes t = MakeTpkt(3 + static_cast<size_t>(i), static_cast<uint8_t>(i));
		stream.insert(stream.end(), t.begin(), t.end());
	}
	Reader r;
	r.Feed(stream);
	Bytes payload;
	int count = 0;
	while(r.Next(payload))
	{
		REQUIRE(payload.size() == 3 + static_cast<size_t>(count));
		++count;
	}
	REQUIRE(count == 50);
}

TEST_CASE(SUITE("Reader handles the maximum size TPKT"))
{
	Reader r;
	r.Feed(Frame(Bytes(MaxPayload, 0x42)));
	Bytes payload;
	REQUIRE(r.Next(payload));
	REQUIRE(payload.size() == MaxPayload);
}

TEST_CASE(SUITE("Reader rejects bad headers and stays failed"))
{
	Bytes payload;
	SECTION("bad version is detected from the first octet")
	{
		Reader r;
		r.Feed(Hex("04"));
		REQUIRE_THROWS_AS(r.Next(payload), DecodeError);
		REQUIRE(r.Failed());
	}
	SECTION("length below the minimum")
	{
		for(const char* hdr : {"03 00 00 00", "03 00 00 04", "03 00 00 06"})
		{
			Reader r;
			r.Feed(Hex(hdr));
			REQUIRE_THROWS_AS(r.Next(payload), DecodeError);
			REQUIRE(r.Failed());
		}
	}
	SECTION("a failed reader keeps throwing until Reset")
	{
		Reader r;
		r.Feed(Hex("03 00 00 02"));
		REQUIRE_THROWS_AS(r.Next(payload), DecodeError);
		REQUIRE_THROWS_AS(r.Next(payload), DecodeError);
		REQUIRE_THROWS_AS(r.Feed(Hex("03 00 00 07 02 f0 80")), DecodeError);
		r.Reset();
		REQUIRE_FALSE(r.Failed());
		r.Feed(Hex("03 00 00 07 02 f0 80"));
		REQUIRE(r.Next(payload));
	}
	SECTION("a good TPKT before a bad one is still delivered")
	{
		Reader r;
		r.Feed(Hex("03 00 00 07 02 f0 80 09 00 00 07"));
		REQUIRE(r.Next(payload));
		REQUIRE_THROWS_AS(r.Next(payload), DecodeError);
	}
}

TEST_CASE(SUITE("Reader enforces its buffer limit"))
{
	Reader r(100);
	//the TPKT is incomplete, so nothing can be drained
	Bytes hdr = Hex("03 00 ff ff");
	hdr.resize(60, 0);
	r.Feed(hdr);
	REQUIRE_THROWS_AS(r.Feed(Bytes(41, 0)), DecodeError);
	REQUIRE(r.Failed());

	Reader exact(100);
	Bytes h2 = Hex("03 00 ff ff");
	h2.resize(100, 0);
	exact.Feed(h2);
	REQUIRE(exact.Buffered() == 100);

	Reader huge(100);
	REQUIRE_THROWS_AS(huge.Feed(Bytes(101, 3)), DecodeError);
}

TEST_CASE(SUITE("Reader draining keeps the buffer bounded"))
{
	Reader r(2000);
	Bytes payload;
	for(int i = 0; i < 1000; ++i)
	{
		r.Feed(MakeTpkt(500, static_cast<uint8_t>(i)));
		REQUIRE(r.Next(payload));
		REQUIRE(payload.size() == 500);
	}
}

TEST_CASE(SUITE("Random chunking and corruption never crash the reader"))
{
	uint32_t state = 987654;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};

	Bytes clean;
	for(int i = 0; i < 20; ++i)
	{
		const Bytes t = MakeTpkt(3 + next() % 400, static_cast<uint8_t>(i));
		clean.insert(clean.end(), t.begin(), t.end());
	}

	for(int iter = 0; iter < 2000; ++iter)
	{
		Bytes m = clean;
		const unsigned changes = next() % 4; //0 changes exercises pure chunking
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		if(next() % 4 == 0)
			m.resize(next() % (m.size() + 1));

		Reader r(8192);
		Bytes payload;
		size_t total = 0;
		try
		{
			size_t pos = 0;
			while(pos < m.size())
			{
				const size_t n = std::min<size_t>(1 + next() % 700, m.size() - pos);
				r.Feed(m.data() + pos, n);
				pos += n;
				while(r.Next(payload))
				{
					REQUIRE(payload.size() >= MinLength - HeaderSize);
					total += payload.size() + HeaderSize;
				}
			}
		}
		catch(const DecodeError&)
		{}
		REQUIRE(total <= m.size());
		if(changes == 0 && m.size() == clean.size())
			REQUIRE(total == clean.size());
	}
}
