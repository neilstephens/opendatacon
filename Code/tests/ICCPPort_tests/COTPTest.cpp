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
 * COTPTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the COTP (ISO 8073 class 0) codec
 */

#include "COTP.h"
#include "TPKT.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP COTP - " name

namespace
{
using namespace iccp::cotp;

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

Bytes Pattern(size_t n, uint8_t seed = 0)
{
	Bytes b(n);
	for(size_t i = 0; i < n; ++i)
		b[i] = static_cast<uint8_t>(seed + i * 7);
	return b;
}

//CR as sent by a typical MMS client: calling TSAP 0001, called TSAP 0001, 1024 octet TPDUs
const char* TypicalCR = "11 e0 00 00 00 01 00 c1 02 00 01 c2 02 00 01 c0 01 0a";
}

TEST_CASE(SUITE("Decode a connect request"))
{
	const Bytes wire = Hex(TypicalCR);
	const Tpdu t = Decode(wire);
	REQUIRE(t.type == TpduType::ConnectRequest);
	REQUIRE(t.dstRef == 0);
	REQUIRE(t.srcRef == 1);
	REQUIRE(t.callingTsap.has_value());
	REQUIRE(*t.callingTsap == Hex("00 01"));
	REQUIRE(t.calledTsap.has_value());
	REQUIRE(*t.calledTsap == Hex("00 01"));
	REQUIRE(t.tpduSizeCode.has_value());
	REQUIRE(*t.tpduSizeCode == TpduSize1024);
}

TEST_CASE(SUITE("Decode a connect confirm"))
{
	const Bytes wire = Hex("0d d0 00 01 12 34 00 c2 02 00 01 c0 01 0d");
	const Tpdu t = Decode(wire);
	REQUIRE(t.type == TpduType::ConnectConfirm);
	REQUIRE(t.dstRef == 1);
	REQUIRE(t.srcRef == 0x1234);
	REQUIRE_FALSE(t.callingTsap.has_value());
	REQUIRE(*t.calledTsap == Hex("00 01"));
	REQUIRE(*t.tpduSizeCode == TpduSize8192);
}

TEST_CASE(SUITE("Decode a CR with no variable part"))
{
	const Tpdu t = Decode(Hex("06 e0 00 00 00 05 00"));
	REQUIRE(t.type == TpduType::ConnectRequest);
	REQUIRE(t.srcRef == 5);
	REQUIRE_FALSE(t.callingTsap.has_value());
	REQUIRE_FALSE(t.calledTsap.has_value());
	REQUIRE_FALSE(t.tpduSizeCode.has_value());
}

TEST_CASE(SUITE("Decode skips unknown parameters and ignores the CDT nibble"))
{
	//checksum (c3), alternative class (c7), vendor parameter (ea) then the size
	const Tpdu t = Decode(Hex("15 e5 00 00 00 01 00 c3 02 aa bb c7 01 00 ea 03 01 02 03 c0 01 0b"));
	REQUIRE(t.type == TpduType::ConnectRequest);
	REQUIRE(*t.tpduSizeCode == TpduSize2048);
}

TEST_CASE(SUITE("Decode a TSAP of maximum length, reject longer"))
{
	Bytes cr = Hex("06 e0 00 00 00 01 00");
	cr.push_back(0xC2);
	cr.push_back(32);
	cr.insert(cr.end(), 32, 0x41);
	cr[0] = static_cast<uint8_t>(cr.size() - 1);
	REQUIRE(Decode(cr).calledTsap->size() == 32);

	Bytes too_long = Hex("06 e0 00 00 00 01 00");
	too_long.push_back(0xC2);
	too_long.push_back(33);
	too_long.insert(too_long.end(), 33, 0x41);
	too_long[0] = static_cast<uint8_t>(too_long.size() - 1);
	REQUIRE_THROWS_AS(Decode(too_long), DecodeError);
}

TEST_CASE(SUITE("Decode a disconnect request"))
{
	Tpdu t = Decode(Hex("06 80 00 01 00 02 80"));
	REQUIRE(t.type == TpduType::DisconnectRequest);
	REQUIRE(t.dstRef == 1);
	REQUIRE(t.srcRef == 2);
	REQUIRE(t.reason == DisconnectReason::NormalDisconnect);
	REQUIRE(t.dataLength == 0);

	//with a checksum parameter and user data
	const Bytes wire = Hex("09 80 00 01 00 02 85 c5 01 00 de ad");
	t = Decode(wire);
	REQUIRE(t.reason == DisconnectReason::ProtocolError);
	REQUIRE(Bytes(t.data, t.data + t.dataLength) == Hex("de ad"));
}

TEST_CASE(SUITE("Decode a data TPDU"))
{
	const Bytes wire = Hex("02 f0 80 01 02 03");
	Tpdu t = Decode(wire);
	REQUIRE(t.type == TpduType::Data);
	REQUIRE(t.eot);
	REQUIRE(Bytes(t.data, t.data + t.dataLength) == Hex("01 02 03"));

	const Bytes more = Hex("02 f0 00 aa");
	t = Decode(more);
	REQUIRE_FALSE(t.eot);
	REQUIRE(t.dataLength == 1);

	//empty DT
	t = Decode(Hex("02 f0 80"));
	REQUIRE(t.eot);
	REQUIRE(t.dataLength == 0);
}

TEST_CASE(SUITE("Decode rejects malformed TPDUs"))
{
	for(const char* bad : {
		"",                                       //empty
		"06",                                     //one octet
		"ff e0 00 00 00 01 00",                   //reserved LI
		"06 e0 00 00 00 01",                      //truncated
		"05 e0 00 00 00 01",                      //LI too small for CR
		"05 d0 00 00 00 01",                      //LI too small for CC
		"05 80 00 01 00 02",                      //LI too small for DR
		"06 e0 00 00 00 01 20",                   //class 2
		"06 e0 00 00 00 01 40",                   //class 4
		"07 e0 00 00 00 01 00 aa",                //parameter header truncated
		"08 e0 00 00 00 01 00 c1 02",             //parameter value missing
		"09 e0 00 00 00 01 00 c1 03 00",          //parameter overruns
		"08 e0 00 00 00 01 00 c0 00 00",          //trailing octet after LI
		"09 e0 00 00 00 01 00 c0 01 06",          //size code 6 too small
		"09 e0 00 00 00 01 00 c0 01 0e",          //size code 14 too big
		"09 e0 00 00 00 01 00 c0 01 00",          //size code 0
		"0a e0 00 00 00 01 00 c0 02 0a 0a",       //size parameter of wrong length
		"0c e0 00 00 00 01 00 c0 01 0a c0 01 0a", //duplicate size
		"0c e0 00 00 00 01 00 c1 01 01 c1 01 02", //duplicate calling TSAP
		"0c d0 00 00 00 01 00 c2 01 01 c2 01 02", //duplicate called TSAP
		"06 e0 00 00 00 01 00 aa",                //user data in CR
		"06 d0 00 00 00 01 00 aa",                //user data in CC
		"01 f0",                                  //DT with LI 1
		"03 f0 80 00",                            //DT with LI 3
		"02 f0",                                  //DT truncated
		"02 70 00",                               //ER, unsupported
		"02 c0 00",                               //DC, unsupported
		"02 10 00",                               //ED, unsupported
		"06 00 00 00 00 01 00",                   //unknown type
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(Decode(b), DecodeError);
	}
	REQUIRE_THROWS_AS(Decode(nullptr, 3), std::invalid_argument);
}

TEST_CASE(SUITE("Encode matches known vectors"))
{
	REQUIRE(EncodeConnectRequest(1, Bytes{0, 1}, Bytes{0, 1}, TpduSize1024) == Hex(TypicalCR));
	REQUIRE(EncodeConnectConfirm(1, 0x1234, std::nullopt, Bytes{0, 1}, TpduSize8192)
		== Hex("0d d0 00 01 12 34 00 c2 02 00 01 c0 01 0d"));
	REQUIRE(EncodeConnectRequest(5, std::nullopt, std::nullopt, std::nullopt) == Hex("06 e0 00 00 00 05 00"));
	REQUIRE(EncodeDisconnectRequest(1, 2, DisconnectReason::NormalDisconnect) == Hex("06 80 00 01 00 02 80"));
	const Bytes d = Hex("01 02 03");
	REQUIRE(EncodeData(d.data(), d.size()) == Hex("02 f0 80 01 02 03"));
	REQUIRE(EncodeData(d.data(), d.size(), false) == Hex("02 f0 00 01 02 03"));
	REQUIRE(EncodeData(nullptr, 0) == Hex("02 f0 80"));
}

TEST_CASE(SUITE("Encode then decode round trips"))
{
	SECTION("connect parameters")
	{
		for(const std::optional<Bytes>& calling : {std::optional<Bytes>(), std::optional<Bytes>(Bytes{}), std::optional<Bytes>(Pattern(32))})
			for(const std::optional<Bytes>& called : {std::optional<Bytes>(), std::optional<Bytes>(Pattern(2, 9))})
				for(std::optional<uint8_t> size : {std::optional<uint8_t>(), std::optional<uint8_t>(TpduSize128), std::optional<uint8_t>(TpduSize8192)})
				{
					const Bytes cr = EncodeConnectRequest(0xABCD, calling, called, size);
					const Tpdu t = Decode(cr);
					REQUIRE(t.type == TpduType::ConnectRequest);
					REQUIRE(t.srcRef == 0xABCD);
					REQUIRE(t.callingTsap == calling);
					REQUIRE(t.calledTsap == called);
					REQUIRE(t.tpduSizeCode == size);

					const Bytes cc = EncodeConnectConfirm(0xABCD, 7, calling, called, size);
					const Tpdu c = Decode(cc);
					REQUIRE(c.type == TpduType::ConnectConfirm);
					REQUIRE(c.dstRef == 0xABCD);
					REQUIRE(c.srcRef == 7);
					REQUIRE(c.calledTsap == called);
				}
	}
	SECTION("disconnect with data")
	{
		const Bytes extra = Pattern(64);
		Tpdu t;
		t.type = TpduType::DisconnectRequest;
		t.dstRef = 0xFFFF;
		t.srcRef = 0x0100;
		t.reason = DisconnectReason::ConnectionRefused;
		t.data = extra.data();
		t.dataLength = extra.size();
		const Bytes wire = Encode(t);
		const Tpdu d = Decode(wire);
		REQUIRE(d.dstRef == 0xFFFF);
		REQUIRE(d.srcRef == 0x0100);
		REQUIRE(d.reason == DisconnectReason::ConnectionRefused);
		REQUIRE(Bytes(d.data, d.data + d.dataLength) == extra);
	}
}

TEST_CASE(SUITE("Encode rejects what cannot be encoded"))
{
	REQUIRE_THROWS_AS(EncodeConnectRequest(1, Pattern(33), std::nullopt, std::nullopt), std::invalid_argument);
	REQUIRE_THROWS_AS(EncodeConnectRequest(1, std::nullopt, Pattern(33), std::nullopt), std::invalid_argument);
	REQUIRE_THROWS_AS(EncodeConnectRequest(1, std::nullopt, std::nullopt, 6), std::invalid_argument);
	REQUIRE_THROWS_AS(EncodeConnectRequest(1, std::nullopt, std::nullopt, 14), std::invalid_argument);

	const Bytes data = Pattern(65);
	Tpdu t;
	t.type = TpduType::DisconnectRequest;
	t.data = data.data();
	t.dataLength = data.size();
	REQUIRE_THROWS_AS(Encode(t), std::invalid_argument);

	t.type = TpduType::ConnectRequest;
	t.dataLength = 1;
	REQUIRE_THROWS_AS(Encode(t), std::invalid_argument);

	t.type = TpduType::Data;
	t.data = nullptr;
	REQUIRE_THROWS_AS(Encode(t), std::invalid_argument);
}

TEST_CASE(SUITE("Truncation of connect and disconnect TPDUs never succeeds and never crashes"))
{
	const Bytes cr = EncodeConnectRequest(1, Pattern(10), Pattern(12, 3), TpduSize2048);
	const Bytes cc = EncodeConnectConfirm(1, 2, Pattern(4), Pattern(5), TpduSize512);
	const Bytes dr = EncodeDisconnectRequest(3, 4, DisconnectReason::ProtocolError);
	for(const Bytes* full : {&cr, &cc, &dr})
	{
		REQUIRE_NOTHROW(Decode(*full));
		for(size_t n = 0; n < full->size(); ++n)
			REQUIRE_THROWS_AS(Decode(full->data(), n), DecodeError);
	}

	const Bytes dt = Hex("02 f0 80 01 02 03");
	for(size_t n = 0; n < 3; ++n)
		REQUIRE_THROWS_AS(Decode(dt.data(), n), DecodeError);
}

TEST_CASE(SUITE("Mutated TPDUs only ever raise DecodeError"))
{
	const std::vector<Bytes> originals = {
		EncodeConnectRequest(1, Pattern(10), Pattern(12, 3), TpduSize2048),
		EncodeConnectConfirm(1, 2, Pattern(4), Pattern(5), TpduSize512),
		EncodeDisconnectRequest(3, 4, DisconnectReason::ProtocolError),
		Hex("02 f0 80 01 02 03 04 05")
	};

	uint32_t state = 424242;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};

	for(int iter = 0; iter < 20000; ++iter)
	{
		Bytes m = originals[next() % originals.size()];
		const unsigned changes = 1 + next() % 3;
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		if(next() % 4 == 0)
			m.resize(next() % (m.size() + 1));
		try
		{
			const Tpdu t = Decode(m);
			//anything accepted must be self consistent and re-encodable
			REQUIRE(t.dataLength <= m.size());
			if(t.type == TpduType::DisconnectRequest && t.dataLength > 64)
				continue; //valid to decode, not to re-encode
			REQUIRE_NOTHROW(Encode(t));
		}
		catch(const DecodeError&)
		{}
	}
}

TEST_CASE(SUITE("Segmentation respects the TPDU size and sets EOT on the last"))
{
	SECTION("sizes around the boundary")
	{
		for(uint8_t code = TpduSize128; code <= TpduSize8192; ++code)
		{
			const size_t maxData = TpduSizeOctets(code) - DataHeaderSize;
			for(size_t len : {size_t(1), maxData - 1, maxData, maxData + 1, 2 * maxData, 2 * maxData + 1, 5 * maxData + 17})
			{
				const Bytes tsdu = Pattern(len);
				const auto tpdus = Segment(tsdu, code);
				REQUIRE(tpdus.size() == (len + maxData - 1) / maxData);

				Bytes joined;
				for(size_t i = 0; i < tpdus.size(); ++i)
				{
					REQUIRE(tpdus[i].size() <= TpduSizeOctets(code));
					const Tpdu t = Decode(tpdus[i]);
					REQUIRE(t.type == TpduType::Data);
					REQUIRE(t.eot == (i + 1 == tpdus.size()));
					if(!t.eot)
						REQUIRE(tpdus[i].size() == TpduSizeOctets(code));
					joined.insert(joined.end(), t.data, t.data + t.dataLength);
				}
				REQUIRE(joined == tsdu);
			}
		}
	}
	SECTION("empty TSDU gives one empty DT with EOT")
	{
		const auto tpdus = Segment(nullptr, 0, TpduSize128);
		REQUIRE(tpdus.size() == 1);
		REQUIRE(tpdus[0] == Hex("02 f0 80"));
	}
	SECTION("known vector")
	{
		const Bytes tsdu = Pattern(130);
		const auto tpdus = Segment(tsdu, TpduSize128);
		REQUIRE(tpdus.size() == 2);
		REQUIRE(tpdus[0].size() == 128);
		REQUIRE(Bytes(tpdus[0].begin(), tpdus[0].begin() + 3) == Hex("02 f0 00"));
		REQUIRE(tpdus[1].size() == 3 + 5);
		REQUIRE(Bytes(tpdus[1].begin(), tpdus[1].begin() + 3) == Hex("02 f0 80"));
	}
	SECTION("bad arguments")
	{
		REQUIRE_THROWS_AS(Segment(Pattern(10), 6), std::invalid_argument);
		REQUIRE_THROWS_AS(Segment(Pattern(10), 14), std::invalid_argument);
		REQUIRE_THROWS_AS(Segment(nullptr, 5, TpduSize128), std::invalid_argument);
	}
}

TEST_CASE(SUITE("TPDU size negotiation"))
{
	SECTION("responder picks the smaller of proposed and local maximum")
	{
		REQUIRE(NegotiateTpduSize(TpduSize1024, TpduSize8192) == TpduSize1024);
		REQUIRE(NegotiateTpduSize(TpduSize8192, TpduSize1024) == TpduSize1024);
		REQUIRE(NegotiateTpduSize(TpduSize512, TpduSize512) == TpduSize512);
	}
	SECTION("absent parameter means 128")
	{
		REQUIRE(NegotiateTpduSize(std::nullopt, TpduSize8192) == TpduSize128);
	}
	SECTION("invalid inputs")
	{
		REQUIRE_THROWS_AS(NegotiateTpduSize(TpduSize1024, 3), std::invalid_argument);
		REQUIRE_THROWS_AS(NegotiateTpduSize(20, TpduSize1024), std::invalid_argument);
	}
	SECTION("initiator accepts equal or smaller, rejects larger")
	{
		REQUIRE(ConfirmTpduSize(TpduSize1024, TpduSize1024) == TpduSize1024);
		REQUIRE(ConfirmTpduSize(TpduSize1024, TpduSize256) == TpduSize256);
		REQUIRE(ConfirmTpduSize(TpduSize1024, std::nullopt) == TpduSize128);
		REQUIRE(ConfirmTpduSize(std::nullopt, std::nullopt) == TpduSize128);
		REQUIRE_THROWS_AS(ConfirmTpduSize(TpduSize1024, TpduSize2048), DecodeError);
		REQUIRE_THROWS_AS(ConfirmTpduSize(std::nullopt, TpduSize256), DecodeError);
		REQUIRE_THROWS_AS(ConfirmTpduSize(TpduSize1024, 99), DecodeError);
	}
	SECTION("full exchange")
	{
		const Bytes cr = EncodeConnectRequest(1, std::nullopt, std::nullopt, TpduSize4096);
		const uint8_t chosen = NegotiateTpduSize(Decode(cr).tpduSizeCode, TpduSize2048);
		const Bytes cc = EncodeConnectConfirm(1, 9, std::nullopt, std::nullopt, chosen);
		REQUIRE(ConfirmTpduSize(TpduSize4096, Decode(cc).tpduSizeCode) == TpduSize2048);
	}
}

TEST_CASE(SUITE("TSDU reassembly"))
{
	TsduReassembler r;
	const Bytes a = Pattern(100);
	const Bytes b = Pattern(50, 3);

	Tpdu t;
	t.type = TpduType::Data;
	t.eot = false;
	t.data = a.data();
	t.dataLength = a.size();
	REQUIRE_FALSE(r.Add(t));
	REQUIRE(r.Pending() == 100);
	REQUIRE(r.Take().empty()); //not complete yet
	REQUIRE(r.Pending() == 100);

	t.eot = true;
	t.data = b.data();
	t.dataLength = b.size();
	REQUIRE(r.Add(t));
	Bytes expected = a;
	expected.insert(expected.end(), b.begin(), b.end());
	REQUIRE(r.Take() == expected);
	REQUIRE(r.Pending() == 0);

	//reusable for the next TSDU
	REQUIRE(r.Add(t));
	REQUIRE(r.Take() == b);
}

TEST_CASE(SUITE("TSDU reassembly limits"))
{
	const Bytes chunk = Pattern(100);
	Tpdu t;
	t.type = TpduType::Data;
	t.eot = false;
	t.data = chunk.data();
	t.dataLength = chunk.size();

	SECTION("TSDU size cap")
	{
		TsduReassembler r(250);
		REQUIRE_FALSE(r.Add(t));
		REQUIRE_FALSE(r.Add(t));
		REQUIRE_THROWS_AS(r.Add(t), DecodeError);
		REQUIRE(r.Pending() == 0); //discarded
	}
	SECTION("cap is exact")
	{
		TsduReassembler r(200);
		REQUIRE_FALSE(r.Add(t));
		t.eot = true;
		REQUIRE(r.Add(t));
		REQUIRE(r.Take().size() == 200);
	}
	SECTION("DT larger than the negotiated TPDU size")
	{
		TsduReassembler r(1000, TpduSize128);
		t.dataLength = 125;
		REQUIRE_NOTHROW(r.Add(t));
		t.dataLength = 126;
		REQUIRE_THROWS_AS(r.Add(t), DecodeError);
	}
	SECTION("only DTs")
	{
		TsduReassembler r;
		Tpdu cr;
		cr.type = TpduType::ConnectRequest;
		REQUIRE_THROWS_AS(r.Add(cr), DecodeError);
	}
	SECTION("a completed TSDU must be taken first")
	{
		TsduReassembler r;
		t.eot = true;
		REQUIRE(r.Add(t));
		REQUIRE_THROWS_AS(r.Add(t), DecodeError);
		REQUIRE(r.Take().empty());
	}
}

TEST_CASE(SUITE("TPKT, COTP and segmentation work together over a chunked stream"))
{
	const uint8_t size = TpduSize256;
	const Bytes tsdu1 = Pattern(1000, 1);
	const Bytes tsdu2 = Pattern(3, 2);
	const Bytes tsdu3 = Pattern(253, 3);

	Bytes stream;
	stream = iccp::tpkt::Frame(EncodeConnectRequest(1, Bytes{0, 1}, Bytes{0, 1}, size));
	for(const Bytes* tsdu : {&tsdu1, &tsdu2, &tsdu3})
		for(const Bytes& dt : Segment(*tsdu, size))
		{
			const Bytes f = iccp::tpkt::Frame(dt);
			stream.insert(stream.end(), f.begin(), f.end());
		}
	{
		const Bytes f = iccp::tpkt::Frame(EncodeDisconnectRequest(1, 2, DisconnectReason::NormalDisconnect));
		stream.insert(stream.end(), f.begin(), f.end());
	}

	for(size_t chunk : {size_t(1), size_t(2), size_t(5), size_t(100), size_t(256), size_t(100000)})
	{
		iccp::tpkt::Reader reader;
		TsduReassembler reassembler(1 << 20, size);
		std::vector<Bytes> tsdus;
		bool sawCr = false, sawDr = false;
		Bytes payload;

		for(size_t pos = 0; pos < stream.size(); pos += chunk)
		{
			reader.Feed(stream.data() + pos, std::min(chunk, stream.size() - pos));
			while(reader.Next(payload))
			{
				const Tpdu t = Decode(payload);
				switch(t.type)
				{
					case TpduType::ConnectRequest:
						sawCr = true;
						break;
					case TpduType::DisconnectRequest:
						sawDr = true;
						break;
					case TpduType::Data:
						if(reassembler.Add(t))
							tsdus.push_back(reassembler.Take());
						break;
					default:
						FAIL("unexpected TPDU");
				}
			}
		}
		REQUIRE(sawCr);
		REQUIRE(sawDr);
		REQUIRE(tsdus.size() == 3);
		REQUIRE(tsdus[0] == tsdu1);
		REQUIRE(tsdus[1] == tsdu2);
		REQUIRE(tsdus[2] == tsdu3);
	}
}
