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
 * PresentationTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the Presentation (ISO 8823) codec
 */

#include "COTP.h"
#include "Presentation.h"
#include "Session.h"
#include "TPKT.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP Presentation - " name

namespace
{
using namespace iccp::presentation;

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

Bytes PduData(const Pdv& p)
{
	return p.dataLength == 0 ? Bytes() : Bytes(p.data, p.data + p.dataLength);
}

//a small but valid BER element standing in for an ACSE or MMS PDU
const char* FakeAarq = "60 03 80 01 00";
const char* FakeMms = "a8 03 80 01 01";

//The CP a typical MMS client sends: ACSE (context 1) and MMS (context 3), BER for both
const char* TypicalCP =
	"31 46 a0 03 80 01 01 a2 3f 81 04 00 00 00 01 82 04 00 00 00 01 "
	"a4 23 30 0f 02 01 01 06 04 52 01 00 01 30 04 06 02 51 01 "
	"30 10 02 01 03 06 05 28 ca 22 02 01 30 04 06 02 51 01 "
	"61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00";
const char* TypicalCPA =
	"31 2f a0 03 80 01 01 a2 28 83 04 00 00 00 01 "
	"a5 12 30 07 80 01 00 81 02 51 01 30 07 80 01 00 81 02 51 01 "
	"61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00";

ConnectRequest TypicalRequest(const Bytes& aarq)
{
	ConnectRequest cp;
	cp.callingSelector = Bytes{0, 0, 0, 1};
	cp.calledSelector = Bytes{0, 0, 0, 1};
	cp.contexts.push_back(ContextDefinition{1, AcseAbstractSyntax, {BerTransferSyntax}
		});
	cp.contexts.push_back(ContextDefinition{3, MmsAbstractSyntax, {BerTransferSyntax}
		});
	cp.userData.push_back(MakePdv(1, aarq));
	return cp;
}

ConnectAccept TypicalAccept(const Bytes& aare)
{
	ConnectAccept cpa;
	cpa.respondingSelector = Bytes{0, 0, 0, 1};
	ContextResult accepted;
	accepted.transferSyntax = BerTransferSyntax;
	cpa.results = {accepted, accepted};
	cpa.userData.push_back(MakePdv(1, aare));
	return cpa;
}

void CheckViewsInside(const UserData& ud, const Bytes& m)
{
	for(const auto& p : ud)
		if(p.dataLength > 0)
		{
			REQUIRE(p.data >= m.data());
			REQUIRE(p.data + p.dataLength <= m.data() + m.size());
		}
}

//a CP carrying `count` presentation contexts, built directly so it can exceed the encoder's limits
Bytes CpWithContexts(size_t count, size_t transferSyntaxesEach = 1)
{
	using namespace iccp::ber;
	Writer w;
	w.Begin(Tag::Universal(UniversalTag::Set, true));
	w.Begin(Tag::Context(0, true));
	w.Integer(1, Tag::Context(0));
	w.End();
	w.Begin(Tag::Context(2, true));
	w.Begin(Tag::Context(4, true));
	for(size_t i = 0; i < count; ++i)
	{
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		w.Integer(static_cast<int64_t>(2 * i + 1));
		w.ObjectIdentifier(AcseAbstractSyntax);
		w.Begin(Tag::Universal(UniversalTag::Sequence, true));
		for(size_t t = 0; t < transferSyntaxesEach; ++t)
			w.ObjectIdentifier(BerTransferSyntax);
		w.End();
		w.End();
	}
	w.End();
	w.End();
	w.End();
	return w.Release();
}
}

TEST_CASE(SUITE("Object identifiers encode as the registered values"))
{
	using namespace iccp::ber;
	auto enc = [](const Oid& o)
		     {
			     Writer w;
			     w.ObjectIdentifier(o);
			     return w.Release();
		     };
	REQUIRE(enc(AcseAbstractSyntax) == Hex("06 04 52 01 00 01"));
	REQUIRE(enc(MmsAbstractSyntax) == Hex("06 05 28 ca 22 02 01"));
	REQUIRE(enc(BerTransferSyntax) == Hex("06 02 51 01"));
}

TEST_CASE(SUITE("Decode a connect presentation"))
{
	const Bytes wire = Hex(TypicalCP);
	const ConnectRequest cp = DecodeConnectRequest(wire);
	REQUIRE(*cp.callingSelector == Hex("00 00 00 01"));
	REQUIRE(*cp.calledSelector == Hex("00 00 00 01"));
	REQUIRE(cp.contexts.size() == 2);
	REQUIRE(cp.contexts[0].id == 1);
	REQUIRE(cp.contexts[0].abstractSyntax == AcseAbstractSyntax);
	REQUIRE(cp.contexts[0].transferSyntaxes == std::vector<Oid>{BerTransferSyntax});
	REQUIRE(cp.contexts[1].id == 3);
	REQUIRE(cp.contexts[1].abstractSyntax == MmsAbstractSyntax);
	REQUIRE_FALSE(cp.defaultContextName.has_value());
	REQUIRE_FALSE(cp.presentationRequirements.has_value());
	REQUIRE_FALSE(cp.userSessionRequirements.has_value());
	REQUIRE(cp.userData.size() == 1);
	REQUIRE(cp.userData[0].contextId == 1);
	REQUIRE(cp.userData[0].kind == PdvKind::SingleAsn1Type);
	REQUIRE_FALSE(cp.userData[0].transferSyntax.has_value());
	REQUIRE(PduData(cp.userData[0]) == Hex(FakeAarq));
	CheckViewsInside(cp.userData, wire);
}

TEST_CASE(SUITE("Decode a connect presentation accept"))
{
	const Bytes wire = Hex(TypicalCPA);
	const ConnectAccept cpa = DecodeConnectAccept(wire);
	REQUIRE(*cpa.respondingSelector == Hex("00 00 00 01"));
	REQUIRE(cpa.results.size() == 2);
	for(const auto& r : cpa.results)
	{
		REQUIRE(r.result == Result::Acceptance);
		REQUIRE(*r.transferSyntax == BerTransferSyntax);
		REQUIRE_FALSE(r.providerReason.has_value());
	}
	REQUIRE(cpa.userData.size() == 1);
	REQUIRE(PduData(cpa.userData[0]) == Hex(FakeAarq));
}

TEST_CASE(SUITE("Decode a connect presentation refuse"))
{
	Bytes wire = Hex("30 06 87 01 00 8a 01 01");
	ConnectRefuse r = DecodeConnectRefuse(wire);
	REQUIRE(*r.defaultContextResult == Result::Acceptance);
	REQUIRE(*r.providerReason == RefuseReason::TemporaryCongestion);
	REQUIRE(r.userData.empty());

	wire = Hex("30 13 83 04 00 00 00 01 a5 08 30 06 80 01 02 82 01 01 8a 01 03");
	r = DecodeConnectRefuse(wire);
	REQUIRE(*r.respondingSelector == Hex("00 00 00 01"));
	REQUIRE(r.results.size() == 1);
	REQUIRE(r.results[0].result == Result::ProviderRejection);
	REQUIRE(*r.results[0].providerReason == ContextReason::AbstractSyntaxNotSupported);
	REQUIRE_FALSE(r.results[0].transferSyntax.has_value());
	REQUIRE(*r.providerReason == RefuseReason::CalledPresentationAddressUnknown);

	//a CPR can carry the ACSE AARE rejecting the association
	wire = Hex("30 0e 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00");
	r = DecodeConnectRefuse(wire);
	REQUIRE(r.userData.size() == 1);
	REQUIRE(PduData(r.userData[0]) == Hex(FakeAarq));

	//empty parameters are fine
	REQUIRE_NOTHROW(DecodeConnectRefuse(Hex("30 00")));
}

TEST_CASE(SUITE("Decode aborts"))
{
	Bytes wire = Hex("a0 0e 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00");
	Abort a = DecodeAbort(wire);
	REQUIRE_FALSE(a.providerAbort);
	REQUIRE(a.userData.size() == 1);
	REQUIRE(PduData(a.userData[0]) == Hex(FakeAarq));

	//ARU with a context identifier list, which is validated but unused
	wire = Hex("a0 0a a0 08 30 06 02 01 01 06 01 51");
	a = DecodeAbort(wire);
	REQUIRE_FALSE(a.providerAbort);
	REQUIRE(a.userData.empty());

	wire = Hex("a0 00");
	REQUIRE_FALSE(DecodeAbort(wire).providerAbort);

	wire = Hex("30 06 80 01 02 81 01 00");
	a = DecodeAbort(wire);
	REQUIRE(a.providerAbort);
	REQUIRE(*a.providerReason == AbortReason::UnexpectedPpdu);
	REQUIRE(*a.eventIdentifier == 0);

	wire = Hex("30 00");
	a = DecodeAbort(wire);
	REQUIRE(a.providerAbort);
	REQUIRE_FALSE(a.providerReason.has_value());
}

TEST_CASE(SUITE("Decode user data"))
{
	Bytes wire = Hex("61 0c 30 0a 02 01 03 a0 05 a8 03 80 01 01");
	Pdv p = DecodeData(wire);
	REQUIRE(p.contextId == 3);
	REQUIRE(p.kind == PdvKind::SingleAsn1Type);
	REQUIRE(PduData(p) == Hex(FakeMms));

	//with a transfer syntax name
	wire = Hex("61 10 30 0e 06 02 51 01 02 01 03 a0 05 60 03 80 01 00");
	p = DecodeData(wire);
	REQUIRE(*p.transferSyntax == BerTransferSyntax);

	//octet-aligned
	wire = Hex("61 08 30 06 02 01 03 81 01 aa");
	p = DecodeData(wire);
	REQUIRE(p.kind == PdvKind::OctetAligned);
	REQUIRE(PduData(p) == Hex("aa"));

	//indefinite length encodings
	wire = Hex("61 80 30 80 02 01 03 a0 80 a8 03 80 01 01 00 00 00 00 00 00");
	p = DecodeData(wire);
	REQUIRE(p.contextId == 3);
	REQUIRE(PduData(p) == Hex(FakeMms));
}

TEST_CASE(SUITE("Decode accepts protocol version 1 and skips unknown parameters"))
{
	//protocol-version with version-1 set
	REQUIRE_NOTHROW(DecodeConnectRequest(Hex("31 0b a0 03 80 01 01 a2 04 80 02 07 80")));
	//unknown context-specific parameter 0x8c (number 12)
	REQUIRE_NOTHROW(DecodeConnectRequest(Hex("31 0b a0 03 80 01 01 a2 04 8c 02 aa bb")));
	//minimal CP
	const ConnectRequest cp = DecodeConnectRequest(Hex("31 07 a0 03 80 01 01 a2 00"));
	REQUIRE(cp.contexts.empty());
	REQUIRE(cp.userData.empty());
	//elements of the SET in another order
	REQUIRE_NOTHROW(DecodeConnectRequest(Hex("31 07 a2 00 a0 03 80 01 01")));
}

TEST_CASE(SUITE("Decode requirements and default context name"))
{
	const ConnectRequest cp = DecodeConnectRequest(Hex("31 13 a0 03 80 01 01 a2 0c 81 02 00 01 88 02 07 80 89 02 06 40"));
	REQUIRE(*cp.callingSelector == Hex("00 01"));
	REQUIRE(*cp.presentationRequirements == PresentationRequirement::ContextManagement);
	REQUIRE(*cp.userSessionRequirements == 0x0002);

	const ConnectRequest d = DecodeConnectRequest(Hex("31 13 a0 03 80 01 01 a2 0c a6 0a 80 04 52 01 00 01 81 02 51 01"));
	REQUIRE(d.defaultContextName.has_value());
	REQUIRE(d.defaultContextName->abstractSyntax == AcseAbstractSyntax);
	REQUIRE(d.defaultContextName->transferSyntax == BerTransferSyntax);
}

TEST_CASE(SUITE("Decode rejects malformed connect presentations"))
{
	std::string trailing = std::string(TypicalCP) + " 00";
	for(const std::string bad : {
		std::string(""),
		std::string("30 00"),                                                                   //not a SET
		trailing,                                                                               //trailing octet
		std::string("31 00"),                                                                   //no mode selector or parameters
		std::string("31 05 a0 03 80 01 01"),                                                    //no normal-mode-parameters
		std::string("31 05 a2 03 81 01 00"),                                                    //no mode selector
		std::string("31 07 a0 03 80 01 00 a2 00"),                                              //X.410 mode value
		std::string("31 07 a0 03 80 01 02 a2 00"),                                              //unknown mode value
		std::string("31 09 a0 03 80 01 01 a1 00 a2 00"),                                        //X.410 parameters
		std::string("31 09 a0 03 80 01 01 a2 00 a2 00"),                                        //duplicate normal-mode-parameters
		std::string("31 0c a0 03 80 01 01 a0 03 80 01 01 a2 00"),                               //duplicate mode selector
		std::string("31 07 80 03 80 01 01 a2 00"),                                              //mode selector primitive
		std::string("31 07 a0 03 81 01 01 a2 00"),                                              //mode value with the wrong tag
		std::string("31 07 a0 03 80 01 01 82 00"),                                              //normal-mode-parameters primitive
		std::string("31 0f a0 03 80 01 01 a2 08 81 02 00 01 81 02 00 02"),                      //duplicate calling selector
		std::string("31 0b a0 03 80 01 01 a2 04 83 02 00 01"),                                  //responding selector in a CP
		std::string("31 0b a0 03 80 01 01 a2 04 85 02 00 01"),                                  //result list in a CP
		std::string("31 0b a0 03 80 01 01 a2 04 8a 02 00 01"),                                  //provider reason in a CP
		std::string("31 0b a0 03 80 01 01 a2 04 80 02 07 00"),                                  //protocol version 1 not offered
		std::string("31 0b a0 03 80 01 01 a2 04 80 01 00 00"),                                  //malformed
		std::string("31 16 a0 03 80 01 01 a2 0f a4 0d 30 0b 02 01 00 06 01 51 30 03 06 01 51"), //context id 0
		std::string("31 16 a0 03 80 01 01 a2 0f a4 0d 30 0b 02 01 ff 06 01 51 30 03 06 01 51"), //context id -1
		std::string("31 0c a0 03 80 01 01 a2 05 60 03 04 01 aa"),                               //simply-encoded-data
		std::string("31 09 a0 03 80 01 01 a2 02 61 00"),                                        //empty fully-encoded-data
		std::string("31 12 a0 03 80 01 01 a2 0b 61 09 30 07 02 01 01 82 02 00 aa"),             //arbitrary bit string PDV
		std::string("31 14 a0 03 80 01 01 a2 0d 61 0b 30 09 02 01 01 a0 04 60 03 aa bb"),       //truncated element
		std::string("31 15 a0 03 80 01 01 a2 0e 61 0c 30 0a 02 01 01 a0 05 60 01 00 05 00"),    //two elements
		std::string("31 0e a0 03 80 01 01 a2 07 61 05 30 03 02 01 01"),                         //PDV with no data values
		std::string("31 12 a0 03 80 01 01 a2 0b 61 09 30 07 02 01 01 a1 02 aa bb"),             //constructed octet-aligned data
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeConnectRequest(b), DecodeError);
	}

	//selector of 17 octets
	Bytes sel = Hex("31 1f a0 03 80 01 01 a2 18 81 11");
	sel.insert(sel.end(), 17, 0x41);
	sel[1] = static_cast<uint8_t>(sel.size() - 2);
	sel[8] = static_cast<uint8_t>(sel.size() - 9);
	REQUIRE_THROWS_AS(DecodeConnectRequest(sel), DecodeError);

	REQUIRE_THROWS_AS(DecodeConnectRequest(nullptr, 3), std::invalid_argument);
}

TEST_CASE(SUITE("Decode rejects malformed accepts, refuses, aborts and data"))
{
	for(const char* bad : {
		"",
		"30 00",                                           //CPA must be a SET
		"31 07 a0 03 80 01 01 a2 00 00",                   //trailing
		"31 0b a0 03 80 01 01 a2 04 81 02 00 01",          //calling selector in a CPA
		"31 0b a0 03 80 01 01 a2 04 a4 02 00 01",          //context list in a CPA
		"31 0e a0 03 80 01 01 a2 07 a5 05 30 03 80 01 03", //result 3
		"31 0e a0 03 80 01 01 a2 07 a5 05 30 03 80 01 ff", //result -1
		"31 0d a0 03 80 01 01 a2 06 a5 04 30 02 81 00",    //result item with no result
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeConnectAccept(b), DecodeError);
	}

	for(const char* bad : {
		"",
		"31 00",                         //X.410 mode CPR is a SET
		"30 00 00",                      //trailing
		"30 03 81 01 00",                //calling selector in a CPR
		"30 03 88 01 00",                //presentation requirements in a CPR
		"30 03 87 01 03",                //default context result 3
		"30 06 8a 01 01 8a 01 02",       //duplicate provider reason
		"30 08 83 02 00 01 83 02 00 01", //duplicate responding selector
		"30 03 8a 01",                   //truncated
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeConnectRefuse(b), DecodeError);
	}

	for(const char* bad : {
		"",
		"31 00", //X.410 mode ARU is a SET
		"a1 00", //not an abort
		"61 00",
		"a0 00 00",                                                                                  //trailing
		"a0 04 a0 02 02 01",                                                                         //malformed identifier list
		"a0 0a a0 08 30 06 02 01 00 06 01 51",                                                       //identifier 0
		"a0 02 a0 00 a0 00",                                                                         //length mismatch: trailing octets
		"a0 04 a0 00 a0 00",                                                                         //duplicate identifier list
		"a0 1c 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00", //duplicate user data
		"30 04 80 01 01 80",                                                                         //truncated
		"30 06 80 01 01 80 01 02",                                                                   //duplicate reason
		"30 03 02 01 00",                                                                            //unexpected universal element
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeAbort(b), DecodeError);
	}

	for(const char* bad : {
		"",
		"61 00",          //no PDV-list
		"60 03 04 01 aa", //simply-encoded-data
		"31 00",
		"61 0c 30 0a 02 01 03 a0 05 a8 03 80 01 01 00",                                  //trailing
		"61 0c 30 0a 02 01 00 a0 05 a8 03 80 01 01",                                     //context id 0
		"61 0a 30 08 02 01 03 a0 03 a8 03 80",                                           //truncated inner element
		"61 18 30 0a 02 01 03 a0 05 a8 03 80 01 01 30 0a 02 01 03 a0 05 a8 03 80 01 01", //two PDVs
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeData(b), DecodeError);
	}
}

TEST_CASE(SUITE("List size caps"))
{
	REQUIRE(DecodeConnectRequest(CpWithContexts(MaxContexts)).contexts.size() == MaxContexts);
	REQUIRE_THROWS_AS(DecodeConnectRequest(CpWithContexts(MaxContexts + 1)), DecodeError);
	REQUIRE(DecodeConnectRequest(CpWithContexts(1, MaxTransferSyntaxes)).contexts[0].transferSyntaxes.size() == MaxTransferSyntaxes);
	REQUIRE_THROWS_AS(DecodeConnectRequest(CpWithContexts(1, MaxTransferSyntaxes + 1)), DecodeError);

	//PDV-lists in user data, carried in an ARU
	for(size_t count : {MaxPdvs, MaxPdvs + 1})
	{
		const Bytes pdvList = Hex("30 06 02 01 01 81 01 aa");
		iccp::ber::Writer w;
		w.Begin(iccp::ber::Tag::Context(0, true));
		w.Begin(iccp::ber::Tag::Application(1, true));
		for(size_t i = 0; i < count; ++i)
			w.Raw(pdvList);
		w.End();
		w.End();
		const Bytes aru = w.Release();
		if(count <= MaxPdvs)
			REQUIRE(DecodeAbort(aru).userData.size() == count);
		else
			REQUIRE_THROWS_AS(DecodeAbort(aru), DecodeError);
	}
}

TEST_CASE(SUITE("Encode matches known vectors"))
{
	const Bytes aarq = Hex(FakeAarq);
	REQUIRE(Encode(TypicalRequest(aarq)) == Hex(TypicalCP));
	REQUIRE(Encode(TypicalAccept(aarq)) == Hex(TypicalCPA));

	ConnectRequest small;
	small.callingSelector = Bytes{0, 1};
	small.presentationRequirements = PresentationRequirement::ContextManagement;
	small.userSessionRequirements = static_cast<uint16_t>(0x0002);
	REQUIRE(Encode(small) == Hex("31 13 a0 03 80 01 01 a2 0c 81 02 00 01 88 02 06 80 89 02 06 40"));

	ConnectRequest empty;
	REQUIRE(Encode(empty) == Hex("31 07 a0 03 80 01 01 a2 00"));

	ConnectRefuse r;
	r.defaultContextResult = Result::Acceptance;
	r.providerReason = RefuseReason::TemporaryCongestion;
	REQUIRE(Encode(r) == Hex("30 06 87 01 00 8a 01 01"));

	ConnectRefuse r2;
	r2.respondingSelector = Bytes{0, 0, 0, 1};
	ContextResult rej;
	rej.result = Result::ProviderRejection;
	rej.providerReason = ContextReason::AbstractSyntaxNotSupported;
	r2.results.push_back(rej);
	r2.providerReason = RefuseReason::CalledPresentationAddressUnknown;
	REQUIRE(Encode(r2) == Hex("30 13 83 04 00 00 00 01 a5 08 30 06 80 01 02 82 01 01 8a 01 03"));

	Abort aru;
	aru.userData.push_back(MakePdv(1, aarq));
	REQUIRE(Encode(aru) == Hex("a0 0e 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00"));

	Abort arp;
	arp.providerAbort = true;
	arp.providerReason = AbortReason::UnexpectedPpdu;
	arp.eventIdentifier = 0;
	REQUIRE(Encode(arp) == Hex("30 06 80 01 02 81 01 00"));

	const Bytes mms = Hex(FakeMms);
	REQUIRE(EncodeData(3, mms) == Hex("61 0c 30 0a 02 01 03 a0 05 a8 03 80 01 01"));

	Pdv octets;
	octets.contextId = 3;
	octets.kind = PdvKind::OctetAligned;
	const Bytes aa = Hex("aa");
	octets.data = aa.data();
	octets.dataLength = aa.size();
	REQUIRE(EncodeData(octets) == Hex("61 08 30 06 02 01 03 81 01 aa"));
}

TEST_CASE(SUITE("Encode then decode round trips"))
{
	const Bytes aarq = Hex(FakeAarq);

	SECTION("connect request with every parameter")
	{
		ConnectRequest cp = TypicalRequest(aarq);
		cp.callingSelector = Pattern(16);
		cp.contexts.push_back(ContextDefinition{5, AcseAbstractSyntax, {BerTransferSyntax, Oid{1, 3, 6, 1}
								    }
			});
		cp.defaultContextName = DefaultContextName{AcseAbstractSyntax, BerTransferSyntax};
		cp.presentationRequirements = static_cast<uint8_t>(PresentationRequirement::ContextManagement | PresentationRequirement::Restoration);
		cp.userSessionRequirements = static_cast<uint16_t>(0x1802);
		const Bytes mmsData = Hex(FakeMms);
		Pdv second = MakePdv(3, mmsData);
		second.transferSyntax = BerTransferSyntax;
		cp.userData.push_back(second);

		const Bytes wire = Encode(cp);
		const ConnectRequest out = DecodeConnectRequest(wire);
		REQUIRE(out.callingSelector == cp.callingSelector);
		REQUIRE(out.calledSelector == cp.calledSelector);
		REQUIRE(out.contexts.size() == 3);
		for(size_t i = 0; i < 3; ++i)
		{
			REQUIRE(out.contexts[i].id == cp.contexts[i].id);
			REQUIRE(out.contexts[i].abstractSyntax == cp.contexts[i].abstractSyntax);
			REQUIRE(out.contexts[i].transferSyntaxes == cp.contexts[i].transferSyntaxes);
		}
		REQUIRE(out.defaultContextName->abstractSyntax == AcseAbstractSyntax);
		REQUIRE(out.presentationRequirements == cp.presentationRequirements);
		REQUIRE(out.userSessionRequirements == cp.userSessionRequirements);
		REQUIRE(out.userData.size() == 2);
		REQUIRE(PduData(out.userData[0]) == aarq);
		REQUIRE(out.userData[1].transferSyntax == second.transferSyntax);
		REQUIRE(PduData(out.userData[1]) == Hex(FakeMms));
	}
	SECTION("connect accept and refuse")
	{
		ConnectAccept cpa = TypicalAccept(aarq);
		ContextResult rej;
		rej.result = Result::ProviderRejection;
		rej.providerReason = ContextReason::ProposedTransferSyntaxesNotSupported;
		cpa.results.push_back(rej);
		cpa.presentationRequirements = static_cast<uint8_t>(0);
		cpa.userSessionRequirements = static_cast<uint16_t>(0x0002);
		const Bytes wire = Encode(cpa);
		const ConnectAccept out = DecodeConnectAccept(wire);
		REQUIRE(out.respondingSelector == cpa.respondingSelector);
		REQUIRE(out.results.size() == 3);
		REQUIRE(out.results[2].result == Result::ProviderRejection);
		REQUIRE(out.results[2].providerReason == rej.providerReason);
		REQUIRE(out.presentationRequirements == cpa.presentationRequirements);
		REQUIRE(out.userSessionRequirements == cpa.userSessionRequirements);
		REQUIRE(PduData(out.userData[0]) == aarq);

		ConnectRefuse cpr;
		cpr.respondingSelector = Bytes{9};
		cpr.results = {rej};
		cpr.defaultContextResult = Result::UserRejection;
		cpr.providerReason = RefuseReason::UserDataNotReadable;
		cpr.userData.push_back(MakePdv(1, aarq));
		const Bytes w2 = Encode(cpr);
		const ConnectRefuse o2 = DecodeConnectRefuse(w2);
		REQUIRE(o2.respondingSelector == cpr.respondingSelector);
		REQUIRE(o2.results.size() == 1);
		REQUIRE(o2.defaultContextResult == cpr.defaultContextResult);
		REQUIRE(o2.providerReason == cpr.providerReason);
		REQUIRE(PduData(o2.userData[0]) == aarq);
	}
	SECTION("aborts")
	{
		Abort aru;
		aru.userData.push_back(MakePdv(1, aarq));
		const Bytes w1 = Encode(aru);
		const Abort a = DecodeAbort(w1);
		REQUIRE_FALSE(a.providerAbort);
		REQUIRE(PduData(a.userData[0]) == aarq);

		const Bytes w2 = Encode(Abort{});
		REQUIRE(w2 == Hex("a0 00"));
		REQUIRE_FALSE(DecodeAbort(w2).providerAbort);

		Abort arp;
		arp.providerAbort = true;
		arp.providerReason = AbortReason::InvalidPpduParameterValue;
		const Bytes w3 = Encode(arp);
		const Abort b = DecodeAbort(w3);
		REQUIRE(b.providerAbort);
		REQUIRE(b.providerReason == arp.providerReason);
		REQUIRE_FALSE(b.eventIdentifier.has_value());
	}
	SECTION("data sizes around the length form boundaries")
	{
		for(size_t n : {size_t(0), size_t(1), size_t(100), size_t(120), size_t(121), size_t(250), size_t(300), size_t(5000), size_t(70000)})
		{
			iccp::ber::Writer w;
			w.OctetString(Pattern(n));
			const Bytes pdu = w.Release();
			const Bytes wire = EncodeData(3, pdu);
			const Pdv p = DecodeData(wire);
			REQUIRE(p.contextId == 3);
			REQUIRE(PduData(p) == pdu);
		}
	}
	SECTION("octet-aligned data")
	{
		Pdv p;
		p.contextId = 7;
		p.kind = PdvKind::OctetAligned;
		const Bytes d = Pattern(300);
		p.data = d.data();
		p.dataLength = d.size();
		const Bytes wire = EncodeData(p);
		const Pdv o = DecodeData(wire);
		REQUIRE(o.kind == PdvKind::OctetAligned);
		REQUIRE(PduData(o) == d);
	}
}

TEST_CASE(SUITE("Encode rejects what cannot be encoded"))
{
	const Bytes aarq = Hex(FakeAarq);

	ConnectRequest cp = TypicalRequest(aarq);
	cp.callingSelector = Pattern(17);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	cp = TypicalRequest(aarq);
	cp.contexts[0].id = 0;
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	cp = TypicalRequest(aarq);
	const ContextDefinition oneContext = cp.contexts[0];
	cp.contexts.assign(MaxContexts + 1, oneContext);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	cp = TypicalRequest(aarq);
	cp.contexts[0].transferSyntaxes.assign(MaxTransferSyntaxes + 1, BerTransferSyntax);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	cp = TypicalRequest(aarq);
	const Pdv onePdv = cp.userData[0];
	cp.userData.assign(MaxPdvs + 1, onePdv);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	//single-ASN.1-type data must be exactly one BER element
	cp = TypicalRequest(aarq);
	const Bytes notBer = Hex("ff");
	cp.userData[0] = MakePdv(1, notBer);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);
	const Bytes two = Hex("05 00 05 00");
	cp.userData[0] = MakePdv(1, two);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);
	cp.userData[0] = MakePdv(1, nullptr, 0);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);
	cp.userData[0] = MakePdv(1, nullptr, 4);
	REQUIRE_THROWS_AS(Encode(cp), std::invalid_argument);

	ConnectAccept cpa = TypicalAccept(aarq);
	cpa.results[0].result = 3;
	REQUIRE_THROWS_AS(Encode(cpa), std::invalid_argument);

	ConnectRefuse cpr;
	cpr.defaultContextResult = 3;
	REQUIRE_THROWS_AS(Encode(cpr), std::invalid_argument);

	Abort arp;
	arp.providerAbort = true;
	arp.userData.push_back(MakePdv(1, aarq));
	REQUIRE_THROWS_AS(Encode(arp), std::invalid_argument);

	Abort aru;
	aru.providerReason = 1;
	REQUIRE_THROWS_AS(Encode(aru), std::invalid_argument);

	REQUIRE_THROWS_AS(EncodeData(0, aarq), std::invalid_argument);
	REQUIRE_THROWS_AS(EncodeData(-1, aarq), std::invalid_argument);
}

TEST_CASE(SUITE("Truncation never succeeds and never crashes"))
{
	iccp::ber::Writer big;
	big.OctetString(Pattern(300));
	const Bytes bigPdu = big.Release();

	struct Case
	{
		Bytes wire;
		int which;
	};
	const std::vector<Case> cases = {
		{Hex(TypicalCP), 0},
		{Encode(TypicalRequest(bigPdu)), 0}, //long form lengths
		{Hex(TypicalCPA), 1},
		{Hex("30 13 83 04 00 00 00 01 a5 08 30 06 80 01 02 82 01 01 8a 01 03"), 2},
		{Hex("a0 0e 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00"), 3},
		{Hex("30 06 80 01 02 81 01 00"), 3},
		{Hex("61 0c 30 0a 02 01 03 a0 05 a8 03 80 01 01"), 4},
		{EncodeData(3, bigPdu), 4},
	};
	for(const auto& c : cases)
	{
		auto decode = [&c](const uint8_t* d, size_t n)
				  {
					  switch(c.which)
					  {
						  case 0: DecodeConnectRequest(d, n); break;
						  case 1: DecodeConnectAccept(d, n); break;
						  case 2: DecodeConnectRefuse(d, n); break;
						  case 3: DecodeAbort(d, n); break;
						  default: DecodeData(d, n); break;
					  }
				  };
		REQUIRE_NOTHROW(decode(c.wire.data(), c.wire.size()));
		for(size_t n = 0; n < c.wire.size(); ++n)
			REQUIRE_THROWS_AS(decode(c.wire.data(), n), DecodeError);
	}
}

TEST_CASE(SUITE("Mutated PPDUs only ever raise DecodeError"))
{
	const Bytes aarq = Hex(FakeAarq);
	const std::vector<Bytes> originals = {
		Hex(TypicalCP),
		Encode(TypicalRequest(aarq)),
		Hex(TypicalCPA),
		Hex("30 13 83 04 00 00 00 01 a5 08 30 06 80 01 02 82 01 01 8a 01 03"),
		Hex("a0 0e 61 0c 30 0a 02 01 01 a0 05 60 03 80 01 00"),
		Hex("a0 0a a0 08 30 06 02 01 01 06 01 51"),
		Hex("30 06 80 01 02 81 01 00"),
		Hex("61 0c 30 0a 02 01 03 a0 05 a8 03 80 01 01"),
		Hex("31 13 a0 03 80 01 01 a2 0c 81 02 00 01 88 02 07 80 89 02 06 40"),
		Hex("31 13 a0 03 80 01 01 a2 0c a6 0a 80 04 52 01 00 01 81 02 51 01"),
	};

	uint32_t state = 31337;
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
			CheckViewsInside(DecodeConnectRequest(m).userData, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			const ConnectAccept a = DecodeConnectAccept(m);
			CheckViewsInside(a.userData, m);
			REQUIRE(a.results.size() <= MaxContexts);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeConnectRefuse(m).userData, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeAbort(m).userData, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			const Pdv p = DecodeData(m);
			if(p.dataLength > 0)
			{
				REQUIRE(p.data >= m.data());
				REQUIRE(p.data + p.dataLength <= m.data() + m.size());
			}
		}
		catch(const DecodeError&)
		{}
	}
}

TEST_CASE(SUITE("TPKT, COTP, Session and Presentation work together over a chunked stream"))
{
	using namespace iccp;
	const uint8_t tpduSize = cotp::TpduSize128; //forces segmentation of the larger PPDUs

	const Bytes aarq = Hex(FakeAarq);
	const Bytes aare = Hex("60 03 80 01 01"); //a distinct stand-in for an AARE
	const Bytes mmsPdu = []
				   {
					   ber::Writer w;
					   w.Begin(ber::Tag::Context(0, true)); //a confirmed-RequestPDU shaped element
					   w.OctetString(Pattern(400, 3));
					   w.End();
					   return w.Release();
				   }();

	//build what each side sends, as session user data -> SPDU -> TSDU -> DTs -> TPKTs
	Bytes stream;
	auto sendTpdu = [&stream](const Bytes& tpdu)
			    {
				    const Bytes f = tpkt::Frame(tpdu);
				    stream.insert(stream.end(), f.begin(), f.end());
			    };
	auto sendTsdu = [&](const Bytes& tsdu)
			    {
				    for(const Bytes& dt : cotp::Segment(tsdu, tpduSize))
					    sendTpdu(dt);
			    };

	sendTpdu(cotp::EncodeConnectRequest(1, Bytes{0, 1}, Bytes{0, 1}, tpduSize));
	sendTpdu(cotp::EncodeConnectConfirm(1, 2, std::nullopt, Bytes{0, 1}, tpduSize));

	{
		session::Spdu cn;
		cn.type = session::SpduType::Connect;
		cn.callingSelector = Bytes{0, 1};
		cn.calledSelector = Bytes{0, 1};
		cn.protocolOptions = static_cast<uint8_t>(0);
		cn.version = session::Version::Version2;
		cn.requirements = session::Requirement::Duplex;
		const Bytes cp = Encode(TypicalRequest(aarq));
		cn.hasUserData = true;
		cn.userData = cp.data();
		cn.userDataLength = cp.size();
		sendTsdu(session::Encode(cn));
	}
	{
		session::Spdu ac;
		ac.type = session::SpduType::Accept;
		ac.protocolOptions = static_cast<uint8_t>(0);
		ac.version = session::Version::Version2;
		ac.requirements = session::Requirement::Duplex;
		const Bytes cpa = Encode(TypicalAccept(aare));
		ac.hasUserData = true;
		ac.userData = cpa.data();
		ac.userDataLength = cpa.size();
		sendTsdu(session::Encode(ac));
	}
	sendTsdu(session::EncodeData(EncodeData(3, mmsPdu)));
	sendTsdu(session::EncodeData(EncodeData(3, Hex(FakeMms))));

	for(size_t chunk : {size_t(1), size_t(3), size_t(77), size_t(128), size_t(100000)})
	{
		tpkt::Reader reader;
		cotp::TsduReassembler reassembler(1 << 20, tpduSize);
		Bytes payload;

		bool sawCr = false, sawCc = false, sawConnect = false, sawAccept = false;
		std::vector<Bytes> dataPdus;
		Bytes cpUserData, cpaUserData;
		size_t contexts = 0, results = 0;

		for(size_t pos = 0; pos < stream.size(); pos += chunk)
		{
			reader.Feed(stream.data() + pos, std::min(chunk, stream.size() - pos));
			while(reader.Next(payload))
			{
				const cotp::Tpdu t = cotp::Decode(payload);
				if(t.type == cotp::TpduType::ConnectRequest)
					sawCr = true;
				else if(t.type == cotp::TpduType::ConnectConfirm)
					sawCc = true;
				else if(t.type == cotp::TpduType::Data)
				{
					if(!reassembler.Add(t))
						continue;
					const Bytes tsdu = reassembler.Take();
					const session::Spdu s = session::Decode(tsdu);
					switch(s.type)
					{
						case session::SpduType::Connect:
						{
							const ConnectRequest cp = DecodeConnectRequest(s.userData, s.userDataLength);
							sawConnect = true;
							contexts = cp.contexts.size();
							REQUIRE(cp.userData.size() == 1);
							cpUserData = PduData(cp.userData[0]);
							break;
						}
						case session::SpduType::Accept:
						{
							const ConnectAccept cpa = DecodeConnectAccept(s.userData, s.userDataLength);
							sawAccept = true;
							results = cpa.results.size();
							cpaUserData = PduData(cpa.userData.at(0));
							break;
						}
						case session::SpduType::Data:
						{
							const Pdv p = DecodeData(s.userData, s.userDataLength);
							REQUIRE(p.contextId == 3);
							dataPdus.push_back(PduData(p));
							break;
						}
						default:
							FAIL("unexpected SPDU");
					}
				}
			}
		}
		REQUIRE(sawCr);
		REQUIRE(sawCc);
		REQUIRE(sawConnect);
		REQUIRE(sawAccept);
		REQUIRE(contexts == 2);
		REQUIRE(results == 2);
		REQUIRE(cpUserData == aarq);
		REQUIRE(cpaUserData == aare);
		REQUIRE(dataPdus.size() == 2);
		REQUIRE(dataPdus[0] == mmsPdu);
		REQUIRE(dataPdus[1] == Hex(FakeMms));
	}
}

namespace
{
struct SampleFrame
{
	const char* frame;  //the whole TPKT
	const char* mmsPdu; //the MMS PDU inside the presentation PDV
};

//Complete single-TPDU data-phase frames (synthetic): MMS Identify request/response
const SampleFrame CapturedFrames[] = {
	{
		"03 00 00 1c 02 f0 80 01 00 01 00 61 0f 30 0d 02 01 03 a0 08 a0 06 02 02 12 34 82 00",
		"a0 06 02 02 12 34 82 00"
	},
	{
		"03 00 00 36 02 f0 80 01 00 01 00 61 29 30 27 02 01 03 a0 22 a1 20 02 02 12 34 a2 1a 80 08 56 45 4e 44 4f 52 5f 58 81 07 4d 4f 44 45 4c 5f 59 82 05 52 45 56 5f 5a",
		"a1 20 02 02 12 34 a2 1a 80 08 56 45 4e 44 4f 52 5f 58 81 07 4d 4f 44 45 4c 5f 59 82 05 52 45 56 5f 5a"
	},
	{
		"03 00 00 1c 02 f0 80 01 00 01 00 61 0f 30 0d 02 01 03 a0 08 a0 06 02 02 23 45 82 00",
		"a0 06 02 02 23 45 82 00"
	},
	{
		"03 00 00 36 02 f0 80 01 00 01 00 61 29 30 27 02 01 03 a0 22 a1 20 02 02 23 45 a2 1a 80 08 56 45 4e 44 4f 52 5f 58 81 07 4d 4f 44 45 4c 5f 59 82 05 52 45 56 5f 5a",
		"a1 20 02 02 23 45 a2 1a 80 08 56 45 4e 44 4f 52 5f 58 81 07 4d 4f 44 45 4c 5f 59 82 05 52 45 56 5f 5a"
	},
};
}

TEST_CASE(SUITE("Data-phase frames decode and re-encode byte for byte"))
{
	using namespace iccp;
	for(const auto& c : CapturedFrames)
	{
		INFO(c.frame);
		const Bytes wire = Hex(c.frame);

		//TPKT -> COTP DT (EOT) -> TSDU
		tpkt::Reader reader;
		reader.Feed(wire);
		Bytes payload;
		REQUIRE(reader.Next(payload));
		REQUIRE(reader.Buffered() == 0);
		const cotp::Tpdu dt = cotp::Decode(payload);
		REQUIRE(dt.type == cotp::TpduType::Data);
		REQUIRE(dt.eot);
		cotp::TsduReassembler reassembler(cotp::TsduReassembler::DefaultMaxTsdu, cotp::TpduSize1024);
		REQUIRE(reassembler.Add(dt));
		const Bytes tsdu = reassembler.Take();

		//Session: GT + DT
		const session::Spdu spdu = session::Decode(tsdu);
		REQUIRE(spdu.type == session::SpduType::Data);

		//Presentation: user-data PPDU, one PDV-list on context 3
		const Pdv pdv = DecodeData(spdu.userData, spdu.userDataLength);
		REQUIRE(pdv.contextId == 3);
		REQUIRE(pdv.kind == PdvKind::SingleAsn1Type);
		REQUIRE_FALSE(pdv.transferSyntax.has_value());
		REQUIRE(PduData(pdv) == Hex(c.mmsPdu));

		//and back out again
		const Bytes ppdu = EncodeData(pdv);
		REQUIRE(ppdu == Bytes(spdu.userData, spdu.userData + spdu.userDataLength));
		REQUIRE(session::EncodeData(ppdu) == tsdu);
		const auto tpdus = cotp::Segment(tsdu, cotp::TpduSize1024);
		REQUIRE(tpdus.size() == 1);
		REQUIRE(tpkt::Frame(tpdus[0]) == wire);
	}
}

//Optional regression against externally supplied data: set ICCP_CAPTURE_TSDU_DIR to a directory of
//*.bin files, each holding one reassembled TSDU of a data phase (context id 3).
TEST_CASE(SUITE("TSDUs from a directory (optional)"))
{
	std::string dir;
	#ifdef _MSC_VER
	char* env = nullptr;
	size_t envLen = 0;
	if(_dupenv_s(&env, &envLen, "ICCP_CAPTURE_TSDU_DIR") == 0 && env != nullptr)
	{
		dir = env;
		free(env);
	}
	#else
	if(const char* env = std::getenv("ICCP_CAPTURE_TSDU_DIR"))
		dir = env;
	#endif
	if(dir.empty())
		return; //nothing to check

	using namespace iccp;
	size_t files = 0, fragmented = 0;
	size_t largest = 0;
	for(const auto& entry : std::filesystem::directory_iterator(dir))
	{
		if(entry.path().extension() != ".bin")
			continue;
		std::ifstream in(entry.path(), std::ios::binary);
		const Bytes tsdu((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		INFO(entry.path().string());
		++files;
		largest = std::max(largest, tsdu.size());
		REQUIRE(tsdu.size() < cotp::TsduReassembler::DefaultMaxTsdu);

		const session::Spdu spdu = session::Decode(tsdu);
		REQUIRE(spdu.type == session::SpduType::Data);
		const Pdv pdv = DecodeData(spdu.userData, spdu.userDataLength);
		REQUIRE(pdv.contextId == 3);
		REQUIRE(pdv.kind == PdvKind::SingleAsn1Type);

		//byte exact re-encoding of the presentation and session layers
		const Bytes ppdu = EncodeData(pdv);
		REQUIRE(ppdu == Bytes(spdu.userData, spdu.userData + spdu.userDataLength));
		REQUIRE(session::EncodeData(ppdu) == tsdu);

		//segmentation at the negotiated 1024 octet TPDU size
		const auto tpdus = cotp::Segment(tsdu, cotp::TpduSize1024);
		REQUIRE(tpdus.size() == (tsdu.size() + 1020) / 1021);
		if(tpdus.size() > 1)
			++fragmented;
		cotp::TsduReassembler reassembler(cotp::TsduReassembler::DefaultMaxTsdu, cotp::TpduSize1024);
		for(size_t i = 0; i < tpdus.size(); ++i)
		{
			REQUIRE(tpkt::Frame(tpdus[i]).size() == (i + 1 < tpdus.size() ? 1028 : tsdu.size() - 1021 * i + 7));
			REQUIRE(reassembler.Add(cotp::Decode(tpdus[i])) == (i + 1 == tpdus.size()));
		}
		REQUIRE(reassembler.Take() == tsdu);
	}
	INFO("directory " << dir);
	REQUIRE(files > 0);
	WARN("checked " << files << " TSDUs, " << fragmented << " multi-fragment, largest " << largest << " octets");
}
