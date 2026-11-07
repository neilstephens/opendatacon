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
 * SessionTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the Session (ISO 8327) codec
 */

#include "COTP.h"
#include "Session.h"
#include <catch.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP Session - " name

namespace
{
using namespace iccp::session;

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

Bytes UserData(const Spdu& s)
{
	return s.userDataLength == 0 ? Bytes() : Bytes(s.userData, s.userData + s.userDataLength);
}

void SetUserData(Spdu& s, const Bytes& data)
{
	s.hasUserData = true;
	s.userData = data.data();
	s.userDataLength = data.size();
}

//CN as sent by a typical MMS client: protocol options 0, version 2, duplex, selectors 0001, 5 octets of user data
const char* TypicalCN = "0d 1b 05 06 13 01 00 16 01 02 14 02 00 02 33 02 00 01 34 02 00 01 c1 05 01 02 03 04 05";
const char* TypicalAC = "0e 17 05 06 13 01 00 16 01 02 14 02 00 02 34 02 00 01 c1 05 01 02 03 04 05";

Spdu FullConnect(const Bytes& userData)
{
	Spdu s;
	s.type = SpduType::Connect;
	s.callingSelector = Pattern(16);
	s.calledSelector = Pattern(3, 5);
	s.protocolOptions = static_cast<uint8_t>(0x01);
	s.tsduMaxSize = TsduMaxSize{0x1234, 0xFFFE};
	s.version = Version::Version2;
	s.tokenSetting = static_cast<uint8_t>(0xAA);
	s.requirements = static_cast<uint16_t>(Requirement::Duplex | Requirement::NegotiatedRelease);
	SetUserData(s, userData);
	return s;
}
}

TEST_CASE(SUITE("Decode a connect"))
{
	const Bytes wire = Hex(TypicalCN);
	const Spdu s = Decode(wire);
	REQUIRE(s.type == SpduType::Connect);
	REQUIRE(*s.callingSelector == Hex("00 01"));
	REQUIRE(*s.calledSelector == Hex("00 01"));
	REQUIRE(*s.protocolOptions == 0);
	REQUIRE(*s.version == Version::Version2);
	REQUIRE(*s.requirements == Requirement::Duplex);
	REQUIRE_FALSE(s.tsduMaxSize.has_value());
	REQUIRE_FALSE(s.tokenSetting.has_value());
	REQUIRE(s.hasUserData);
	REQUIRE(UserData(s) == Hex("01 02 03 04 05"));
}

TEST_CASE(SUITE("Decode an accept"))
{
	const Bytes wire = Hex(TypicalAC);
	const Spdu s = Decode(wire);
	REQUIRE(s.type == SpduType::Accept);
	REQUIRE_FALSE(s.callingSelector.has_value());
	REQUIRE(*s.calledSelector == Hex("00 01"));
	REQUIRE(*s.version == Version::Version2);
	REQUIRE(UserData(s) == Hex("01 02 03 04 05"));
}

TEST_CASE(SUITE("Decode a refuse"))
{
	const Bytes wire = Hex("0c 06 11 01 01 32 01 80");
	const Spdu s = Decode(wire);
	REQUIRE(s.type == SpduType::Refuse);
	REQUIRE(*s.transportDisconnect == TransportDisconnect::ConnectionReleased);
	REQUIRE(*s.reasonCode == RefuseReason::SelectorUnknown);
	REQUIRE_FALSE(s.hasUserData);

	//reason text after the code
	const Bytes withText = Hex("0c 05 32 03 82 41 42");
	REQUIRE(*Decode(withText).reasonCode == RefuseReason::ProviderCongestion);
}

TEST_CASE(SUITE("Decode a refuse that carries its user data inside the reason code"))
{
	//a real-world stack sends the refusal's presentation PPDU after the reason code in PI 50 and no PI 193:
	//PI 20 (requirements), PI 22 (version), PI 50 = reason 02 followed by the data
	const Bytes wire = Hex("0c 0f 14 02 00 02 16 01 02 32 06 02 aa bb cc dd ee");
	const Spdu s = Decode(wire);
	REQUIRE(s.type == SpduType::Refuse);
	REQUIRE(*s.reasonCode == RefuseReason::RejectedByCalledUserWithData);
	REQUIRE(*s.requirements == 0x0002);
	REQUIRE(*s.version == Version::Version2);
	REQUIRE(s.hasUserData);
	REQUIRE(UserData(s) == Hex("aa bb cc dd ee"));

	//a user data parameter, when there is one, is the user data; the octets after the reason code stay what they are
	const Bytes bothWire = Hex("0c 09 32 03 02 aa bb c1 02 cc dd");
	const Spdu both = Decode(bothWire);
	REQUIRE(UserData(both) == Hex("cc dd"));
	REQUIRE(both.reasonTextLength == 2);

	//a reason code alone has no user data
	REQUIRE_FALSE(Decode(Hex("0c 03 32 01 02")).hasUserData);

	//only a refusal does this
	const Spdu abort = Decode(Hex("19 05 32 03 02 aa bb"));
	REQUIRE_FALSE(abort.hasUserData);
}

TEST_CASE(SUITE("Decode finish, disconnect, abort and abort accept"))
{
	Bytes w = Hex("09 03 11 01 01");
	Spdu s = Decode(w);
	REQUIRE(s.type == SpduType::Finish);
	REQUIRE(*s.transportDisconnect == 1);

	w = Hex("09 00");
	REQUIRE(Decode(w).type == SpduType::Finish);

	w = Hex("0a 07 c1 05 01 02 03 04 05");
	s = Decode(w);
	REQUIRE(s.type == SpduType::Disconnect);
	REQUIRE(UserData(s) == Hex("01 02 03 04 05"));

	w = Hex("19 03 11 01 02");
	s = Decode(w);
	REQUIRE(s.type == SpduType::Abort);
	REQUIRE(*s.transportDisconnect == TransportDisconnect::UserAbort);

	w = Hex("1a 00");
	s = Decode(w);
	REQUIRE(s.type == SpduType::AbortAccept);
	REQUIRE_FALSE(s.hasUserData);
}

TEST_CASE(SUITE("Decode a data transfer"))
{
	const Bytes w = Hex("01 00 01 00 de ad be ef");
	const Spdu s = Decode(w);
	REQUIRE(s.type == SpduType::Data);
	REQUIRE(s.hasUserData);
	REQUIRE(UserData(s) == Hex("de ad be ef"));

	const Bytes empty = Hex("01 00 01 00");
	const Spdu e = Decode(empty);
	REQUIRE(e.hasUserData);
	REQUIRE(e.userDataLength == 0);

	//user data that itself looks like an SPDU is not interpreted
	const Bytes tricky = Hex("01 00 01 00 01 00 01 00 0d");
	REQUIRE(UserData(Decode(tricky)) == Hex("01 00 01 00 0d"));
}

TEST_CASE(SUITE("Decode handles long form lengths"))
{
	const Bytes ud = Pattern(300, 4);

	//CN with 300 octets of user data: SPDU LI = 4 + 300, PI 193 length = 300
	Bytes w = Hex("0d ff 01 30 c1 ff 01 2c");
	w.insert(w.end(), ud.begin(), ud.end());
	Spdu s = Decode(w);
	REQUIRE(UserData(s) == ud);

	//the long form is tolerated for small values too
	w = Hex("0d ff 00 07 c1 ff 00 03 aa bb cc");
	s = Decode(w);
	REQUIRE(UserData(s) == Hex("aa bb cc"));

	//254 is the largest short form value, 255 needs the long form
	for(size_t n : {size_t(254), size_t(255), size_t(256)})
	{
		Spdu t;
		t.type = SpduType::Disconnect;
		const Bytes d = Pattern(n);
		SetUserData(t, d);
		const Bytes wire = Encode(t);
		REQUIRE(UserData(Decode(wire)) == d);
	}
}

TEST_CASE(SUITE("Decode accepts trailing octets as user data"))
{
	const Bytes w = Hex("0e 00 aa bb");
	const Spdu s = Decode(w);
	REQUIRE(s.hasUserData);
	REQUIRE(UserData(s) == Hex("aa bb"));

	const Bytes w2 = Hex("0e 08 05 06 13 01 00 16 01 02 aa bb");
	const Spdu s2 = Decode(w2);
	REQUIRE(*s2.version == 2);
	REQUIRE(UserData(s2) == Hex("aa bb"));
}

TEST_CASE(SUITE("Decode skips unknown parameters and finds items in either place"))
{
	//connection identifier group (PI 9) and an unknown PI 0x99
	const Spdu s = Decode(Hex("0d 0a 01 05 09 03 41 42 43 99 01 00"));
	REQUIRE(s.type == SpduType::Connect);
	REQUIRE_FALSE(s.version.has_value());

	//version at the top level, as in a refuse, and within a group
	REQUIRE(*Decode(Hex("0c 03 16 01 02")).version == 2);
	REQUIRE(*Decode(Hex("0c 05 05 03 16 01 02")).version == 2);

	//a group inside a group is skipped, not recursed into
	const Spdu nested = Decode(Hex("0d 0a 05 08 05 06 16 01 02 16 01 03"));
	REQUIRE_FALSE(nested.version.has_value());
}

TEST_CASE(SUITE("Decode rejects malformed SPDUs"))
{
	for(const char* bad : {
		"",                              //empty
		"0d",                            //one octet
		"01 00",                         //GT alone
		"01 00 01",                      //DT header truncated
		"01 00 01 01 aa",                //DT with a parameter length
		"01 00 02 00 aa",                //second SPDU is not DT
		"01 01 00 01 00",                //GT with a parameter
		"05 00",                         //expedited, unsupported
		"08 00",                         //not finished, unsupported
		"21 00",                         //typed data, unsupported
		"ff 00",                         //unknown type
		"0d 01",                         //LI says 1, nothing there
		"0d ff 00",                      //extended LI truncated
		"0d ff 00 05 00 00",             //extended LI longer than data
		"0d 02 14 02",                   //parameter value missing
		"0d 03 14 02 00",                //parameter overruns the SPDU
		"0d 01 14",                      //parameter header truncated
		"0d 03 14 01 00",                //requirements of wrong length
		"0d 04 16 02 00 02",             //version of wrong length
		"0d 04 15 02 00 01",             //TSDU size of wrong length
		"0d 03 13 02 00",                //protocol options overruns
		"0d 06 05 04 13 03 00 00",       //parameter overruns its group
		"0d 06 16 01 02 16 01 02",       //duplicate version
		"0d 08 33 02 00 01 33 02 00 02", //duplicate calling selector
		"0d 08 34 02 00 01 34 02 00 02", //duplicate called selector
		"0d 06 c1 01 aa c1 01 bb",       //duplicate user data
		"0d 06 c1 01 aa c2 01 bb",       //user data and extended user data
		"0d 03 c1 01 aa bb",             //parameter user data plus trailing octets
		"0c 02 32 00",                   //empty reason code
		"0c 03 11 02 00 00",             //transport disconnect of wrong length
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(Decode(b), DecodeError);
	}

	//selector of 17 octets
	Bytes sel = Hex("0d 13 33 11");
	sel.insert(sel.end(), 17, 0x41);
	REQUIRE_THROWS_AS(Decode(sel), DecodeError);

	REQUIRE_THROWS_AS(Decode(nullptr, 3), std::invalid_argument);
}

TEST_CASE(SUITE("Encode matches known vectors"))
{
	const Bytes ud = Hex("01 02 03 04 05");

	Spdu cn;
	cn.type = SpduType::Connect;
	cn.protocolOptions = static_cast<uint8_t>(0);
	cn.version = Version::Version2;
	cn.requirements = Requirement::Duplex;
	cn.callingSelector = Bytes{0, 1};
	cn.calledSelector = Bytes{0, 1};
	SetUserData(cn, ud);
	REQUIRE(Encode(cn) == Hex(TypicalCN));

	Spdu ac;
	ac.type = SpduType::Accept;
	ac.protocolOptions = static_cast<uint8_t>(0);
	ac.version = Version::Version2;
	ac.requirements = Requirement::Duplex;
	ac.calledSelector = Bytes{0, 1};
	SetUserData(ac, ud);
	REQUIRE(Encode(ac) == Hex(TypicalAC));

	Spdu rf;
	rf.type = SpduType::Refuse;
	rf.transportDisconnect = TransportDisconnect::ConnectionReleased;
	rf.reasonCode = RefuseReason::SelectorUnknown;
	REQUIRE(Encode(rf) == Hex("0c 06 11 01 01 32 01 80"));

	Spdu fn;
	fn.type = SpduType::Finish;
	REQUIRE(Encode(fn) == Hex("09 00"));
	fn.transportDisconnect = static_cast<uint8_t>(1);
	REQUIRE(Encode(fn) == Hex("09 03 11 01 01"));

	Spdu dn;
	dn.type = SpduType::Disconnect;
	SetUserData(dn, ud);
	REQUIRE(Encode(dn) == Hex("0a 07 c1 05 01 02 03 04 05"));

	Spdu ab;
	ab.type = SpduType::Abort;
	ab.transportDisconnect = TransportDisconnect::UserAbort;
	REQUIRE(Encode(ab) == Hex("19 03 11 01 02"));

	Spdu aa;
	aa.type = SpduType::AbortAccept;
	REQUIRE(Encode(aa) == Hex("1a 00"));

	REQUIRE(EncodeData(Hex("de ad")) == Hex("01 00 01 00 de ad"));
	REQUIRE(EncodeData(nullptr, 0) == Hex("01 00 01 00"));

	Spdu data;
	data.type = SpduType::Data;
	SetUserData(data, ud);
	REQUIRE(Encode(data) == Hex("01 00 01 00 01 02 03 04 05"));
}

TEST_CASE(SUITE("Encode then decode round trips every parameter"))
{
	const Bytes ud = Pattern(40);
	const Spdu in = FullConnect(ud);
	for(SpduType type : {SpduType::Connect, SpduType::Accept, SpduType::Refuse})
	{
		Spdu s = in;
		s.type = type;
		s.transportDisconnect = static_cast<uint8_t>(TransportDisconnect::ProtocolError | TransportDisconnect::NoReason);
		s.reasonCode = static_cast<uint8_t>(0x85);

		const Bytes wire = Encode(s);
		const Spdu out = Decode(wire);
		REQUIRE(out.type == type);
		REQUIRE(out.callingSelector == s.callingSelector);
		REQUIRE(out.calledSelector == s.calledSelector);
		REQUIRE(out.protocolOptions == s.protocolOptions);
		REQUIRE(out.tsduMaxSize == s.tsduMaxSize);
		REQUIRE(out.version == s.version);
		REQUIRE(out.tokenSetting == s.tokenSetting);
		REQUIRE(out.requirements == s.requirements);
		REQUIRE(out.transportDisconnect == s.transportDisconnect);
		REQUIRE(out.reasonCode == s.reasonCode);
		REQUIRE(out.hasUserData);
		REQUIRE(UserData(out) == ud);
	}
}

TEST_CASE(SUITE("A long accept carries its user data in PI 193 with the 3 octet length, a long connect in PI 194"))
{
	//the layout of a real-world stack's accept: 0e, ff + 2 octet LI, parameters, c1 ff + 2 octet length, data [V]
	const Bytes d = Pattern(1465);
	Spdu ac;
	ac.type = SpduType::Accept;
	ac.protocolOptions = static_cast<uint8_t>(0);
	ac.version = Version::Version2;
	ac.requirements = static_cast<uint16_t>(Requirement::Duplex);
	SetUserData(ac, d);
	const Bytes wire = Encode(ac);
	const Bytes head = Hex("0e ff 05 c9 05 06 13 01 00 16 01 02 14 02 00 02 c1 ff 05 b9");
	REQUIRE(wire.size() == head.size() + d.size());
	REQUIRE(Bytes(wire.begin(), wire.begin() + head.size()) == head);
	REQUIRE(Bytes(wire.begin() + head.size(), wire.end()) == d);
	REQUIRE(UserData(Decode(wire)) == d);

	//and the decoder takes the same user data in PI 194
	Bytes alt = wire;
	alt[16] = 0xC2;
	REQUIRE(UserData(Decode(alt)) == d);

	Spdu cn = ac;
	cn.type = SpduType::Connect;
	const Bytes cnWire = Encode(cn);
	REQUIRE(cnWire[0] == 0x0d);
	REQUIRE(cnWire[16] == 0xC2);
	REQUIRE(cnWire[17] == 0xFF);
	REQUIRE(UserData(Decode(cnWire)) == d);
}

TEST_CASE(SUITE("User data size handling"))
{
	for(size_t n : {size_t(0), size_t(1), size_t(254), size_t(255), size_t(512)})
	{
		const Bytes d = Pattern(n);
		for(SpduType type : {SpduType::Connect, SpduType::Accept, SpduType::Refuse, SpduType::Finish, SpduType::Disconnect, SpduType::Abort})
		{
			Spdu s;
			s.type = type;
			SetUserData(s, d);
			const Bytes wire = Encode(s);
			const Spdu out = Decode(wire);
			REQUIRE(out.hasUserData);
			REQUIRE(UserData(out) == d);
		}
	}

	//beyond 512 octets only CN and AC may carry it: the CN in extended user data (PI 194), the AC as a real-world stack does it, in PI 193 [V]
	for(size_t n : {size_t(513), size_t(2000), MaxExtendedUserData})
	{
		const Bytes d = Pattern(n);
		for(SpduType type : {SpduType::Connect, SpduType::Accept})
		{
			Spdu s;
			s.type = type;
			SetUserData(s, d);
			const Bytes wire = Encode(s);
			REQUIRE(wire[4] == (type == SpduType::Connect ? 0xC2 : 0xC1));
			const Spdu out = Decode(wire);
			REQUIRE(UserData(out) == d);
		}
		Spdu f;
		f.type = SpduType::Finish;
		SetUserData(f, d);
		REQUIRE_THROWS_AS(Encode(f), std::invalid_argument);
	}
	const Bytes tooBig = Pattern(MaxExtendedUserData + 1);
	Spdu s;
	s.type = SpduType::Connect;
	SetUserData(s, tooBig);
	REQUIRE_THROWS_AS(Encode(s), std::invalid_argument);
}

TEST_CASE(SUITE("Encode rejects what cannot be encoded"))
{
	Spdu s;
	s.type = SpduType::Connect;
	s.callingSelector = Pattern(16);
	REQUIRE_NOTHROW(Encode(s));
	s.callingSelector = Pattern(17);
	REQUIRE_THROWS_AS(Encode(s), std::invalid_argument);
	s.callingSelector.reset();
	s.calledSelector = Pattern(17);
	REQUIRE_THROWS_AS(Encode(s), std::invalid_argument);
	s.calledSelector.reset();

	s.hasUserData = true;
	s.userData = nullptr;
	s.userDataLength = 4;
	REQUIRE_THROWS_AS(Encode(s), std::invalid_argument);

	REQUIRE_THROWS_AS(EncodeData(nullptr, 4), std::invalid_argument);
}

TEST_CASE(SUITE("Truncation never succeeds and never crashes"))
{
	const Bytes ud = Pattern(300);
	std::vector<Bytes> messages = {
		Encode(FullConnect(Pattern(20))),
		Encode(FullConnect(ud)),
		Hex(TypicalCN),
		Hex(TypicalAC),
		Hex("0c 06 11 01 01 32 01 80"),
		Hex("09 03 11 01 01"),
		Hex("0a 07 c1 05 01 02 03 04 05"),
		Hex("19 03 11 01 02"),
		Hex("1a 00"),
	};
	for(const Bytes& full : messages)
	{
		REQUIRE_NOTHROW(Decode(full));
		for(size_t n = 0; n < full.size(); ++n)
			REQUIRE_THROWS_AS(Decode(full.data(), n), DecodeError);
	}

	const Bytes dt = Hex("01 00 01 00 aa bb");
	for(size_t n = 0; n < 4; ++n)
		REQUIRE_THROWS_AS(Decode(dt.data(), n), DecodeError);
}

TEST_CASE(SUITE("Mutated SPDUs only ever raise DecodeError"))
{
	const std::vector<Bytes> originals = {
		Encode(FullConnect(Pattern(20))),
		Encode(FullConnect(Pattern(300))),
		Hex(TypicalAC),
		Hex("0c 06 11 01 01 32 01 80"),
		Hex("09 03 11 01 01"),
		Hex("0a 07 c1 05 01 02 03 04 05"),
		Hex("19 03 11 01 02"),
		Hex("01 00 01 00 de ad be ef"),
	};

	uint32_t state = 777;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};

	for(int iter = 0; iter < 30000; ++iter)
	{
		Bytes m = originals[next() % originals.size()];
		const unsigned changes = 1 + next() % 3;
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		if(next() % 4 == 0)
			m.resize(next() % (m.size() + 1));
		try
		{
			const Spdu s = Decode(m);
			//any user data view must lie wholly inside the input
			if(s.hasUserData && s.userDataLength > 0)
			{
				REQUIRE(s.userData >= m.data());
				REQUIRE(s.userData + s.userDataLength <= m.data() + m.size());
			}
			if(s.callingSelector)
				REQUIRE(s.callingSelector->size() <= MaxSelectorLength);
			if(s.calledSelector)
				REQUIRE(s.calledSelector->size() <= MaxSelectorLength);
		}
		catch(const DecodeError&)
		{}
	}
}

TEST_CASE(SUITE("Session SPDUs travel through COTP segmentation"))
{
	const uint8_t size = iccp::cotp::TpduSize128;
	const Bytes ud = Pattern(450, 9);

	Spdu cn = FullConnect(ud);
	const Bytes wire = Encode(cn);
	const Bytes data = EncodeData(ud);

	iccp::cotp::TsduReassembler reassembler(1 << 20, size);
	std::vector<Bytes> tsdus;
	for(const Bytes* tsdu : {&wire, &data})
		for(const Bytes& dt : iccp::cotp::Segment(*tsdu, size))
			if(reassembler.Add(iccp::cotp::Decode(dt)))
				tsdus.push_back(reassembler.Take());

	REQUIRE(tsdus.size() == 2);
	const Spdu a = Decode(tsdus[0]);
	REQUIRE(a.type == SpduType::Connect);
	REQUIRE(UserData(a) == ud);
	REQUIRE(a.calledSelector == cn.calledSelector);
	const Spdu b = Decode(tsdus[1]);
	REQUIRE(b.type == SpduType::Data);
	REQUIRE(UserData(b) == ud);
}
