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
 * AcseTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the ACSE (ISO 8650) codec
 */

#include "Acse.h"
#include "COTP.h"
#include "Presentation.h"
#include "Session.h"
#include "TPKT.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP ACSE - " name

namespace
{
using namespace iccp::acse;

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

Bytes DataOf(const External& x)
{
	return x.dataLength == 0 ? Bytes() : Bytes(x.data, x.data + x.dataLength);
}

//a small valid BER element standing in for an MMS Initiate PDU
const char* FakeInitiate = "a8 03 80 01 01";

//user-information in these: one EXTERNAL with direct-reference {2 1 1}, indirect-reference 3, single-ASN.1-type
const char* AarqMinimal =
	"60 1b a1 07 06 05 28 ca 22 02 03 "
	"be 10 28 0e 06 02 51 01 02 01 03 a0 05 a8 03 80 01 01";
const char* AarqWithTitles =
	"60 37 a1 07 06 05 28 ca 22 02 03 "
	"a2 07 06 05 29 87 67 01 01 a3 03 02 01 0c "
	"a6 07 06 05 29 87 67 01 02 a7 03 02 01 0d "
	"be 10 28 0e 06 02 51 01 02 01 03 a0 05 a8 03 80 01 01";
const char* AareAccepted =
	"61 27 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 00 "
	"be 10 28 0e 06 02 51 01 02 01 03 a0 05 a8 03 80 01 01";
const char* AareRejected =
	"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 01 a3 05 a1 03 02 01 02";
const char* AareProviderDiagnostic =
	"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 02 a3 05 a2 03 02 01 02";
const char* AarqAuthentication =
	"60 21 a1 07 06 05 28 ca 22 02 03 8a 02 07 80 8b 04 52 03 01 01 "
	"ac 07 80 05 70 61 73 73 31 9d 03 41 42 43";
const char* AarqForm1Titles =
	"60 29 a1 07 06 05 28 ca 22 02 03 "
	"a2 0f 30 0d 31 0b 30 09 06 03 55 04 03 13 02 41 42 "
	"a3 0d 31 0b 30 09 06 03 55 04 03 13 02 41 42";

Aarq TypicalAarq(const Bytes& initiate)
{
	Aarq a;
	a.calledApTitle = ApTitle{true, Oid{1, 1, 999, 1, 1}, {}
	};
	a.calledAeQualifier = AeQualifier{true, 12, {}
	};
	a.callingApTitle = ApTitle{true, Oid{1, 1, 999, 1, 2}, {}
	};
	a.callingAeQualifier = AeQualifier{true, 13, {}
	};
	a.userInformation.push_back(MakeExternal(3, initiate, Oid{2, 1, 1}));
	return a;
}

Aare TypicalAare(const Bytes& initiate)
{
	Aare a;
	a.userInformation.push_back(MakeExternal(3, initiate, Oid{2, 1, 1}));
	return a;
}

void CheckViewsInside(const UserInformation& ui, const Bytes& m)
{
	for(const auto& x : ui)
		if(x.dataLength > 0)
		{
			REQUIRE(x.data >= m.data());
			REQUIRE(x.data + x.dataLength <= m.data() + m.size());
		}
}

//an AARQ with `externals` small EXTERNALs
Bytes AarqWithExternals(size_t externals)
{
	const Bytes ext = Hex("28 05 02 01 03 81 00");
	Bytes body = Hex("a1 07 06 05 28 ca 22 02 03");
	body.push_back(0xBE);
	body.push_back(static_cast<uint8_t>(ext.size() * externals));
	for(size_t i = 0; i < externals; ++i)
		body.insert(body.end(), ext.begin(), ext.end());
	Bytes out = {0x60, static_cast<uint8_t>(body.size())};
	out.insert(out.end(), body.begin(), body.end());
	return out;
}
}

TEST_CASE(SUITE("The MMS application context name encodes as registered"))
{
	iccp::ber::Writer w;
	w.ObjectIdentifier(MmsApplicationContext);
	REQUIRE(w.Release() == Hex("06 05 28 ca 22 02 03"));
}

TEST_CASE(SUITE("Decode an AARQ"))
{
	const Bytes wire = Hex(AarqMinimal);
	REQUIRE(PeekApduType(wire) == ApduType::Aarq);
	const Aarq a = DecodeAarq(wire);
	REQUIRE(a.protocolVersion == ProtocolVersion::Version1);
	REQUIRE(a.applicationContextName == MmsApplicationContext);
	REQUIRE_FALSE(a.calledApTitle.has_value());
	REQUIRE_FALSE(a.callingApTitle.has_value());
	REQUIRE_FALSE(a.authentication.acseRequirements.has_value());
	REQUIRE_FALSE(a.implementationInformation.has_value());
	REQUIRE(a.userInformation.size() == 1);
	const External& x = a.userInformation[0];
	REQUIRE(*x.directReference == Oid({2, 1, 1}));
	REQUIRE(*x.indirectReference == 3);
	REQUIRE(x.encoding == ExternalEncoding::SingleAsn1Type);
	REQUIRE(DataOf(x) == Hex(FakeInitiate));
	CheckViewsInside(a.userInformation, wire);
}

TEST_CASE(SUITE("Decode an AARQ with AP titles and AE qualifiers"))
{
	const Bytes wire = Hex(AarqWithTitles);
	const Aarq a = DecodeAarq(wire);
	REQUIRE(a.calledApTitle->form2);
	REQUIRE(a.calledApTitle->oid == Oid({1, 1, 999, 1, 1}));
	REQUIRE(a.calledAeQualifier->form2);
	REQUIRE(a.calledAeQualifier->number == 12);
	REQUIRE(a.callingApTitle->oid == Oid({1, 1, 999, 1, 2}));
	REQUIRE(a.callingAeQualifier->number == 13);
	REQUIRE_FALSE(a.calledApInvocationId.has_value());
	REQUIRE(a.userInformation.size() == 1);
}

TEST_CASE(SUITE("Decode form 1 titles as opaque BER"))
{
	const Aarq a = DecodeAarq(Hex(AarqForm1Titles));
	REQUIRE_FALSE(a.calledApTitle->form2);
	REQUIRE(a.calledApTitle->form1 == Hex("30 0d 31 0b 30 09 06 03 55 04 03 13 02 41 42"));
	REQUIRE_FALSE(a.calledAeQualifier->form2);
	REQUIRE(a.calledAeQualifier->form1 == Hex("31 0b 30 09 06 03 55 04 03 13 02 41 42"));
}

TEST_CASE(SUITE("Decode invocation identifiers and implementation information"))
{
	//called AP-invocation-id 7, called AE-invocation-id 8, calling 9 and 10, implementation-information "ODC"
	const Bytes wire = Hex("60 22 a1 07 06 05 28 ca 22 02 03 a4 03 02 01 07 a5 03 02 01 08 a8 03 02 01 09 a9 03 02 01 0a 9d 03 4f 44 43");
	const Aarq a = DecodeAarq(wire);
	REQUIRE(*a.calledApInvocationId == 7);
	REQUIRE(*a.calledAeInvocationId == 8);
	REQUIRE(*a.callingApInvocationId == 9);
	REQUIRE(*a.callingAeInvocationId == 10);
	REQUIRE(*a.implementationInformation == "ODC");
	REQUIRE(a.userInformation.empty());
}

TEST_CASE(SUITE("Decode EXTERNAL variants"))
{
	//indirect-reference only
	Bytes wire = Hex("60 17 a1 07 06 05 28 ca 22 02 03 be 0c 28 0a 02 01 03 a0 05 a8 03 80 01 01");
	Aarq a = DecodeAarq(wire);
	REQUIRE_FALSE(a.userInformation[0].directReference.has_value());
	REQUIRE(*a.userInformation[0].indirectReference == 3);
	REQUIRE(DataOf(a.userInformation[0]) == Hex(FakeInitiate));

	//direct-reference only
	wire = Hex("60 18 a1 07 06 05 28 ca 22 02 03 be 0d 28 0b 06 02 51 01 a0 05 a8 03 80 01 01");
	a = DecodeAarq(wire);
	REQUIRE(*a.userInformation[0].directReference == Oid({2, 1, 1}));
	REQUIRE_FALSE(a.userInformation[0].indirectReference.has_value());

	//octet-aligned
	wire = Hex("60 15 a1 07 06 05 28 ca 22 02 03 be 0a 28 08 02 01 03 81 03 aa bb cc");
	a = DecodeAarq(wire);
	REQUIRE(a.userInformation[0].encoding == ExternalEncoding::OctetAligned);
	REQUIRE(DataOf(a.userInformation[0]) == Hex("aa bb cc"));

	//a data-value-descriptor (ObjectDescriptor) is skipped
	wire = Hex("60 20 a1 07 06 05 28 ca 22 02 03 be 15 28 13 06 02 51 01 02 01 03 07 03 41 42 43 a0 05 a8 03 80 01 01");
	a = DecodeAarq(wire);
	REQUIRE(*a.userInformation[0].indirectReference == 3);
	REQUIRE(DataOf(a.userInformation[0]) == Hex(FakeInitiate));

	//indefinite lengths throughout
	wire = Hex("60 80 a1 80 06 05 28 ca 22 02 03 00 00 be 80 28 80 02 01 03 a0 80 a8 03 80 01 01 00 00 00 00 00 00 00 00");
	a = DecodeAarq(wire);
	REQUIRE(a.applicationContextName == MmsApplicationContext);
	REQUIRE(DataOf(a.userInformation[0]) == Hex(FakeInitiate));
}

TEST_CASE(SUITE("Decode protocol version, skipping unknown fields"))
{
	Aarq a = DecodeAarq(Hex("60 0d 80 02 07 80 a1 07 06 05 28 ca 22 02 03"));
	REQUIRE(a.protocolVersion == ProtocolVersion::Version1);
	//no version 1 offered: reported, not enforced
	a = DecodeAarq(Hex("60 0d 80 02 07 00 a1 07 06 05 28 ca 22 02 03"));
	REQUIRE(a.protocolVersion == 0);

	//application-context-name-list [13] and an unknown [14] are skipped, in any position
	a = DecodeAarq(Hex("60 15 ad 07 06 05 28 ca 22 02 03 a1 07 06 05 28 ca 22 02 03 8e 01 00"));
	REQUIRE(a.applicationContextName == MmsApplicationContext);
	const Aare r = DecodeAare(Hex("61 1a a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 00 8e 01 00 ad 00"));
	REQUIRE(r.result == Result::Accepted);
}

TEST_CASE(SUITE("Decode an AARE"))
{
	const Bytes wire = Hex(AareAccepted);
	REQUIRE(PeekApduType(wire) == ApduType::Aare);
	const Aare a = DecodeAare(wire);
	REQUIRE(a.applicationContextName == MmsApplicationContext);
	REQUIRE(a.result == Result::Accepted);
	REQUIRE_FALSE(a.diagnostic.fromProvider);
	REQUIRE(a.diagnostic.value == UserDiagnostic::Null);
	REQUIRE(a.userInformation.size() == 1);
	REQUIRE(*a.userInformation[0].indirectReference == 3);
	REQUIRE(DataOf(a.userInformation[0]) == Hex(FakeInitiate));

	const Aare rej = DecodeAare(Hex(AareRejected));
	REQUIRE(rej.result == Result::RejectedPermanent);
	REQUIRE_FALSE(rej.diagnostic.fromProvider);
	REQUIRE(rej.diagnostic.value == UserDiagnostic::ApplicationContextNameNotSupported);
	REQUIRE(rej.userInformation.empty());

	const Aare prov = DecodeAare(Hex(AareProviderDiagnostic));
	REQUIRE(prov.result == Result::RejectedTransient);
	REQUIRE(prov.diagnostic.fromProvider);
	REQUIRE(prov.diagnostic.value == ProviderDiagnostic::NoCommonAcseVersion);
}

TEST_CASE(SUITE("Decode an AARE with responding titles"))
{
	const Bytes wire = Hex("61 2d a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 00 "
		"a4 07 06 05 29 87 67 01 01 a5 03 02 01 0c a6 03 02 01 05 a7 03 02 01 06");
	const Aare a = DecodeAare(wire);
	REQUIRE(a.respondingApTitle->oid == Oid({1, 1, 999, 1, 1}));
	REQUIRE(a.respondingAeQualifier->number == 12);
	REQUIRE(*a.respondingApInvocationId == 5);
	REQUIRE(*a.respondingAeInvocationId == 6);
}

TEST_CASE(SUITE("Decode release and abort APDUs"))
{
	Bytes wire = Hex("62 03 80 01 00");
	REQUIRE(PeekApduType(wire) == ApduType::Rlrq);
	Rlrq rq = DecodeRlrq(wire);
	REQUIRE(*rq.reason == ReleaseReason::Normal);

	wire = Hex("62 00");
	rq = DecodeRlrq(wire);
	REQUIRE_FALSE(rq.reason.has_value());
	REQUIRE(rq.userInformation.empty());

	wire = Hex("63 03 80 01 01");
	REQUIRE(PeekApduType(wire) == ApduType::Rlre);
	const Rlre re = DecodeRlre(wire);
	REQUIRE(*re.reason == ReleaseReason::UrgentOrNotFinished);

	wire = Hex("62 14 80 01 00 be 0f 28 0d 02 01 03 a0 08 a0 06 02 01 01 82 01 00");
	rq = DecodeRlrq(wire);
	REQUIRE(rq.userInformation.size() == 1);

	wire = Hex("64 03 80 01 00");
	REQUIRE(PeekApduType(wire) == ApduType::Abrt);
	Abrt ab = DecodeAbrt(wire);
	REQUIRE(ab.source == AbortSource::AcseServiceUser);
	REQUIRE_FALSE(ab.diagnostic.has_value());

	wire = Hex("64 06 80 01 01 81 01 02");
	ab = DecodeAbrt(wire);
	REQUIRE(ab.source == AbortSource::AcseServiceProvider);
	REQUIRE(*ab.diagnostic == AbortDiagnostic::ProtocolError);
}

TEST_CASE(SUITE("Authentication fields are carried but not interpreted"))
{
	const Bytes wire = Hex(AarqAuthentication);
	const Aarq a = DecodeAarq(wire);
	REQUIRE(*a.authentication.acseRequirements == AcseRequirement::Authentication);
	REQUIRE(*a.authentication.mechanismName == Oid({2, 2, 3, 1, 1}));
	REQUIRE(*a.authentication.authenticationValue == Hex("80 05 70 61 73 73 31"));
	REQUIRE(*a.implementationInformation == "ABC");

	//re-encoded unchanged
	REQUIRE(Encode(a) == wire);

	//responder side uses different tags: [8] requirements, [9] mechanism, [10] value
	const Bytes rw = Hex("61 28 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 00 "
		"88 02 07 80 89 04 52 03 01 01 aa 07 80 05 70 61 73 73 32");
	const Aare r = DecodeAare(rw);
	REQUIRE(*r.authentication.acseRequirements == AcseRequirement::Authentication);
	REQUIRE(*r.authentication.mechanismName == Oid({2, 2, 3, 1, 1}));
	REQUIRE(*r.authentication.authenticationValue == Hex("80 05 70 61 73 73 32"));
	REQUIRE(Encode(r) == rw);
}

TEST_CASE(SUITE("Decode rejects malformed AARQs"))
{
	const std::string minimal = AarqMinimal;
	for(const std::string bad : {
		std::string(""),
		std::string("61 00"),                                                                      //an AARE
		std::string("60 00"),                                                                      //no application-context-name
		std::string("60 03 80 01 00"),                                                             //only a version
		minimal + " 00",                                                                           //trailing octet
		std::string("60 12 a1 07 06 05 28 ca 22 02 03 a1 07 06 05 28 ca 22 02 03"),                //duplicate name
		std::string("60 05 a1 03 02 01 00"),                                                       //name is not an OID
		std::string("60 09 81 07 06 05 28 ca 22 02 03"),                                           //name wrapper is primitive
		std::string("60 08 a1 06 06 01 51 06 01 51"),                                              //two elements in the wrapper
		std::string("60 0e a1 07 06 05 28 ca 22 02 03 a2 03 02 01 05"),                            //AP-title that is an INTEGER
		std::string("60 0e a1 07 06 05 28 ca 22 02 03 a3 03 06 01 51"),                            //AE-qualifier that is an OID
		std::string("60 0e a1 07 06 05 28 ca 22 02 03 a4 03 06 01 51"),                            //invocation id that is an OID
		std::string("60 0e a1 07 06 05 28 ca 22 02 03 be 03 02 01 00"),                            //user information that is not an EXTERNAL
		std::string("60 14 a1 07 06 05 28 ca 22 02 03 be 09 28 07 02 01 03 82 02 00 aa"),          //arbitrary EXTERNAL
		std::string("60 10 a1 07 06 05 28 ca 22 02 03 be 05 28 03 02 01 03"),                      //EXTERNAL without data
		std::string("60 17 a1 07 06 05 28 ca 22 02 03 be 0c 28 0a 02 01 03 a0 05 60 01 00 05 00"), //two elements as single-ASN.1-type
		std::string("60 14 a1 07 06 05 28 ca 22 02 03 be 09 28 07 02 01 03 a1 02 aa bb"),          //constructed octet-aligned
		std::string("60 0b a1 07 06 05 28 ca 22 02 03 ac 00"),                                     //empty authentication-value wrapper
		std::string("60 0b a1 07 06 05 28 ca 22 02 03 ab 00"),                                     //constructed mechanism-name
		std::string("60 0b a1 07 06 05 28 ca 22 02 03 bd 00"),                                     //constructed implementation-information
		std::string("60 0c a1 07 06 05 28 ca 22 02 03 02 01 00"),                                  //universal element in the sequence
		std::string("60 11 a1 07 06 05 28 ca 22 02 03 80 02 07 80 80 02 07 80"),                   //duplicate version
		std::string("60 0a a1 07 06 05 28 ca 22 02 03"),                                           //length longer than the data
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeAarq(b), DecodeError);
	}

	//too many EXTERNALs
	const Bytes atLimit = AarqWithExternals(MaxExternals);
	REQUIRE(DecodeAarq(atLimit).userInformation.size() == MaxExternals);
	const Bytes overLimit = AarqWithExternals(MaxExternals + 1);
	REQUIRE_THROWS_AS(DecodeAarq(overLimit), DecodeError);

	REQUIRE_THROWS_AS(DecodeAarq(nullptr, 3), std::invalid_argument);
}

TEST_CASE(SUITE("Decode rejects malformed AAREs, releases and aborts"))
{
	for(const char* bad : {
		"",
		"60 00",                                                                               //an AARQ
		"61 00",                                                                               //nothing mandatory
		"61 10 a1 07 06 05 28 ca 22 02 03 a3 05 a1 03 02 01 00",                               //no result
		"61 0e a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00",                                     //no diagnostic
		"61 0c a2 03 02 01 00 a3 05 a1 03 02 01 00",                                           //no application-context-name
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 03 a3 05 a1 03 02 01 00",                //result 3
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 ff a3 05 a1 03 02 01 00",                //result -1
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 04 01 00 a3 05 a1 03 02 01 00",                //result not an INTEGER
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a3 03 02 01 00",                //unknown diagnostic choice
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 ff",                //negative diagnostic
		"61 15 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 04 01 00",                //diagnostic not an INTEGER
		"61 1a a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a2 03 02 01 00 a3 05 a1 03 02 01 00", //duplicate result
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeAare(b), DecodeError);
	}

	for(const char* bad : {
		"",
		"63 00",                   //an RLRE
		"62 00 00",                //trailing
		"62 03 80 01 ff",          //negative reason
		"62 02 80 01",             //truncated
		"62 06 80 01 00 80 01 00", //duplicate reason
		"62 03 02 01 00",          //universal element
		"62 03 a0 01 00",          //constructed reason
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeRlrq(b), DecodeError);
	}
	REQUIRE_THROWS_AS(DecodeRlre(Hex("62 00")), DecodeError);
	REQUIRE_THROWS_AS(DecodeRlre(Hex("63 03 80 01 ff")), DecodeError);

	for(const char* bad : {
		"",
		"62 00",                   //an RLRQ
		"64 00",                   //no abort-source
		"64 03 80 01 02",          //source 2
		"64 03 80 01 ff",          //negative source
		"64 06 80 01 00 81 01 ff", //negative diagnostic
		"64 06 80 01 00 80 01 00", //duplicate source
		"64 03 02 01 00",          //universal element
	})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(DecodeAbrt(b), DecodeError);
	}

	for(const char* bad : {"", "65 00", "40 00", "60", "02 01 00", "a0 00", "20 00"})
	{
		INFO(bad);
		const Bytes b = Hex(bad);
		REQUIRE_THROWS_AS(PeekApduType(b), DecodeError);
	}
}

TEST_CASE(SUITE("Encode matches known vectors"))
{
	const Bytes initiate = Hex(FakeInitiate);

	Aarq minimal;
	minimal.userInformation.push_back(MakeExternal(3, initiate, Oid{2, 1, 1}));
	REQUIRE(Encode(minimal) == Hex(AarqMinimal));
	REQUIRE(Encode(TypicalAarq(initiate)) == Hex(AarqWithTitles));

	REQUIRE(Encode(TypicalAare(initiate)) == Hex(AareAccepted));

	Aare rejected;
	rejected.result = Result::RejectedPermanent;
	rejected.diagnostic.value = UserDiagnostic::ApplicationContextNameNotSupported;
	REQUIRE(Encode(rejected) == Hex(AareRejected));

	Aare provider;
	provider.result = Result::RejectedTransient;
	provider.diagnostic.fromProvider = true;
	provider.diagnostic.value = ProviderDiagnostic::NoCommonAcseVersion;
	REQUIRE(Encode(provider) == Hex(AareProviderDiagnostic));

	Rlrq rq;
	rq.reason = ReleaseReason::Normal;
	REQUIRE(Encode(rq) == Hex("62 03 80 01 00"));
	REQUIRE(Encode(Rlrq{}) == Hex("62 00"));
	Rlre re;
	re.reason = ReleaseReason::Normal;
	REQUIRE(Encode(re) == Hex("63 03 80 01 00"));

	Abrt ab;
	REQUIRE(Encode(ab) == Hex("64 03 80 01 00"));
	ab.source = AbortSource::AcseServiceProvider;
	ab.diagnostic = AbortDiagnostic::ProtocolError;
	REQUIRE(Encode(ab) == Hex("64 06 80 01 01 81 01 02"));

	//indirect-reference only, octet aligned, form 1 titles
	Aarq indirect;
	indirect.userInformation.push_back(MakeExternal(3, initiate));
	REQUIRE(Encode(indirect) == Hex("60 17 a1 07 06 05 28 ca 22 02 03 be 0c 28 0a 02 01 03 a0 05 a8 03 80 01 01"));

	Aarq octets;
	const Bytes abc = Hex("aa bb cc");
	External x = MakeExternal(3, abc);
	x.encoding = ExternalEncoding::OctetAligned;
	octets.userInformation.push_back(x);
	REQUIRE(Encode(octets) == Hex("60 15 a1 07 06 05 28 ca 22 02 03 be 0a 28 08 02 01 03 81 03 aa bb cc"));

	Aarq titles;
	titles.calledApTitle = ApTitle{false, {}, Hex("30 0d 31 0b 30 09 06 03 55 04 03 13 02 41 42")};
	titles.calledAeQualifier = AeQualifier{false, 0, Hex("31 0b 30 09 06 03 55 04 03 13 02 41 42")};
	REQUIRE(Encode(titles) == Hex(AarqForm1Titles));

	//a non-default protocol version is written
	Aarq version;
	version.protocolVersion = 0x03;
	const Bytes versionWire = Encode(version);
	REQUIRE(versionWire == Hex("60 0d 80 02 06 c0 a1 07 06 05 28 ca 22 02 03"));
	REQUIRE(DecodeAarq(versionWire).protocolVersion == 0x03);
}

TEST_CASE(SUITE("Encode then decode round trips"))
{
	const Bytes initiate = Hex(FakeInitiate);

	SECTION("AARQ with every field")
	{
		Aarq a = TypicalAarq(initiate);
		a.protocolVersion = 0x01;
		a.calledApInvocationId = 1;
		a.calledAeInvocationId = 2;
		a.callingApInvocationId = 3;
		a.callingAeInvocationId = 4;
		a.authentication.acseRequirements = static_cast<uint8_t>(AcseRequirement::Authentication | AcseRequirement::ApplicationContextNegotiation);
		a.authentication.mechanismName = Oid{1, 3, 9999, 7};
		a.authentication.authenticationValue = Hex("a2 03 02 01 05");
		a.implementationInformation = "opendatacon ICCP";
		const Bytes second = Hex("05 00");
		a.userInformation.push_back(MakeExternal(5, second));

		const Bytes wire = Encode(a);
		const Aarq out = DecodeAarq(wire);
		REQUIRE(out.applicationContextName == a.applicationContextName);
		REQUIRE(out.calledApTitle->oid == a.calledApTitle->oid);
		REQUIRE(out.calledAeQualifier->number == 12);
		REQUIRE(out.callingApTitle->oid == a.callingApTitle->oid);
		REQUIRE(out.callingAeQualifier->number == 13);
		REQUIRE(out.calledApInvocationId == a.calledApInvocationId);
		REQUIRE(out.calledAeInvocationId == a.calledAeInvocationId);
		REQUIRE(out.callingApInvocationId == a.callingApInvocationId);
		REQUIRE(out.callingAeInvocationId == a.callingAeInvocationId);
		REQUIRE(out.authentication.acseRequirements == a.authentication.acseRequirements);
		REQUIRE(out.authentication.mechanismName == a.authentication.mechanismName);
		REQUIRE(out.authentication.authenticationValue == a.authentication.authenticationValue);
		REQUIRE(out.implementationInformation == a.implementationInformation);
		REQUIRE(out.userInformation.size() == 2);
		REQUIRE(DataOf(out.userInformation[0]) == initiate);
		REQUIRE(out.userInformation[1].directReference == a.userInformation[1].directReference);
		REQUIRE(*out.userInformation[1].indirectReference == 5);
		REQUIRE(DataOf(out.userInformation[1]) == second);
		REQUIRE(Encode(out) == wire);
	}
	SECTION("AARE with every field")
	{
		Aare a = TypicalAare(initiate);
		a.result = Result::RejectedTransient;
		a.diagnostic = SourceDiagnostic{true, ProviderDiagnostic::NoReasonGiven};
		a.respondingApTitle = ApTitle{true, Oid{1, 1, 999, 1, 1}, {}
		};
		a.respondingAeQualifier = AeQualifier{true, -7, {}
		};
		a.respondingApInvocationId = 11;
		a.respondingAeInvocationId = 12;
		a.authentication.acseRequirements = static_cast<uint8_t>(AcseRequirement::Authentication);
		a.authentication.mechanismName = Oid{2, 2, 3, 1, 1};
		a.authentication.authenticationValue = Hex("80 02 41 42");
		a.implementationInformation = "x";

		const Bytes wire = Encode(a);
		const Aare out = DecodeAare(wire);
		REQUIRE(out.result == a.result);
		REQUIRE(out.diagnostic.fromProvider);
		REQUIRE(out.diagnostic.value == ProviderDiagnostic::NoReasonGiven);
		REQUIRE(out.respondingApTitle->oid == a.respondingApTitle->oid);
		REQUIRE(out.respondingAeQualifier->number == -7);
		REQUIRE(out.respondingApInvocationId == a.respondingApInvocationId);
		REQUIRE(out.respondingAeInvocationId == a.respondingAeInvocationId);
		REQUIRE(out.authentication.authenticationValue == a.authentication.authenticationValue);
		REQUIRE(out.implementationInformation == a.implementationInformation);
		REQUIRE(DataOf(out.userInformation[0]) == initiate);
		REQUIRE(Encode(out) == wire);
	}
	SECTION("release and abort with user information")
	{
		Rlrq rq;
		rq.reason = ReleaseReason::UserDefined;
		rq.userInformation.push_back(MakeExternal(1, initiate));
		const Bytes w1 = Encode(rq);
		const Rlrq o1 = DecodeRlrq(w1);
		REQUIRE(o1.reason == rq.reason);
		REQUIRE(DataOf(o1.userInformation.at(0)) == initiate);

		Rlre re;
		re.userInformation.push_back(MakeExternal(1, initiate));
		const Bytes w2 = Encode(re);
		const Rlre o2 = DecodeRlre(w2);
		REQUIRE_FALSE(o2.reason.has_value());
		REQUIRE(o2.userInformation.size() == 1);

		Abrt ab;
		ab.source = AbortSource::AcseServiceProvider;
		ab.diagnostic = AbortDiagnostic::AuthenticationFailure;
		ab.userInformation.push_back(MakeExternal(1, initiate));
		const Bytes w3 = Encode(ab);
		const Abrt o3 = DecodeAbrt(w3);
		REQUIRE(o3.source == ab.source);
		REQUIRE(o3.diagnostic == ab.diagnostic);
		REQUIRE(o3.userInformation.size() == 1);
	}
	SECTION("long user information uses long form lengths")
	{
		for(size_t n : {size_t(100), size_t(120), size_t(121), size_t(250), size_t(1000), size_t(70000)})
		{
			iccp::ber::Writer w;
			w.Begin(iccp::ber::Tag::Context(8, true));
			w.OctetString(Pattern(n));
			w.End();
			const Bytes pdu = w.Release();
			Aarq a;
			a.userInformation.push_back(MakeExternal(3, pdu, Oid{2, 1, 1}));
			const Bytes wire = Encode(a);
			const Aarq out = DecodeAarq(wire);
			REQUIRE(DataOf(out.userInformation.at(0)) == pdu);
			REQUIRE(Encode(out) == wire);
		}
	}
}

TEST_CASE(SUITE("Encode rejects what cannot be encoded"))
{
	const Bytes initiate = Hex(FakeInitiate);

	Aare a;
	a.result = 3;
	REQUIRE_THROWS_AS(Encode(a), std::invalid_argument);
	a.result = -1;
	REQUIRE_THROWS_AS(Encode(a), std::invalid_argument);
	a.result = 0;
	a.diagnostic.value = -1;
	REQUIRE_THROWS_AS(Encode(a), std::invalid_argument);

	Abrt ab;
	ab.source = 2;
	REQUIRE_THROWS_AS(Encode(ab), std::invalid_argument);
	ab.source = 0;
	ab.diagnostic = -1;
	REQUIRE_THROWS_AS(Encode(ab), std::invalid_argument);

	Rlrq rq;
	rq.reason = -1;
	REQUIRE_THROWS_AS(Encode(rq), std::invalid_argument);
	Rlre re;
	re.reason = -1;
	REQUIRE_THROWS_AS(Encode(re), std::invalid_argument);

	Aarq q;
	const External one = MakeExternal(3, initiate);
	q.userInformation.assign(MaxExternals + 1, one);
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);

	//single-ASN.1-type data must be exactly one BER element
	for(const char* notOne : {"ff", "05 00 05 00", "05"})
	{
		const Bytes bad = Hex(notOne);
		q = Aarq();
		q.userInformation.push_back(MakeExternal(3, bad));
		REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
	}
	q = Aarq();
	q.userInformation.push_back(MakeExternal(3, nullptr, 4));
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
	q = Aarq();
	q.userInformation.push_back(MakeExternal(3, nullptr, 0));
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);

	//form 1 titles must be one BER element of the right type
	q = Aarq();
	q.calledApTitle = ApTitle{false, {}, Hex("02 01 00")};
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
	q.calledApTitle = ApTitle{false, {}, Hex("30 05 00")};
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
	q.calledApTitle = ApTitle{false, {}, Bytes()};
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
	q = Aarq();
	q.calledAeQualifier = AeQualifier{false, 0, Hex("02 01 00")};
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);

	//authentication-value must be one BER element
	q = Aarq();
	q.authentication.authenticationValue = Hex("80 05 aa");
	REQUIRE_THROWS_AS(Encode(q), std::invalid_argument);
}

TEST_CASE(SUITE("Truncation never succeeds and never crashes"))
{
	iccp::ber::Writer big;
	big.Begin(iccp::ber::Tag::Context(8, true));
	big.OctetString(Pattern(300));
	big.End();
	const Bytes bigPdu = big.Release();
	Aarq bigAarq;
	bigAarq.userInformation.push_back(MakeExternal(3, bigPdu, Oid{2, 1, 1}));

	struct Case
	{
		Bytes wire;
		int which;
	};
	const std::vector<Case> cases = {
		{Hex(AarqMinimal), 0},
		{Hex(AarqWithTitles), 0},
		{Hex(AarqAuthentication), 0},
		{Hex(AarqForm1Titles), 0},
		{Encode(bigAarq), 0},
		{Hex(AareAccepted), 1},
		{Hex(AareRejected), 1},
		{Hex(AareProviderDiagnostic), 1},
		{Hex("62 03 80 01 00"), 2},
		{Hex("63 03 80 01 01"), 3},
		{Hex("64 06 80 01 01 81 01 02"), 4},
	};
	for(const auto& c : cases)
	{
		auto decode = [&c](const uint8_t* d, size_t n)
				  {
					  switch(c.which)
					  {
						  case 0: DecodeAarq(d, n); break;
						  case 1: DecodeAare(d, n); break;
						  case 2: DecodeRlrq(d, n); break;
						  case 3: DecodeRlre(d, n); break;
						  default: DecodeAbrt(d, n); break;
					  }
				  };
		REQUIRE_NOTHROW(decode(c.wire.data(), c.wire.size()));
		for(size_t n = 0; n < c.wire.size(); ++n)
			REQUIRE_THROWS_AS(decode(c.wire.data(), n), DecodeError);
	}
}

TEST_CASE(SUITE("Mutated APDUs only ever raise DecodeError"))
{
	const std::vector<Bytes> originals = {
		Hex(AarqMinimal),
		Hex(AarqWithTitles),
		Hex(AarqAuthentication),
		Hex(AarqForm1Titles),
		Hex(AareAccepted),
		Hex(AareRejected),
		Hex(AareProviderDiagnostic),
		Hex("62 14 80 01 00 be 0f 28 0d 02 01 03 a0 08 a0 06 02 01 01 82 01 00"),
		Hex("63 03 80 01 01"),
		Hex("64 06 80 01 01 81 01 02"),
	};

	uint32_t state = 2718;
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
			PeekApduType(m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeAarq(m).userInformation, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			const Aare a = DecodeAare(m);
			CheckViewsInside(a.userInformation, m);
			REQUIRE(a.userInformation.size() <= MaxExternals);
			REQUIRE(a.result >= 0);
			REQUIRE(a.result <= 2);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeRlrq(m).userInformation, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeRlre(m).userInformation, m);
		}
		catch(const DecodeError&)
		{}
		try
		{
			CheckViewsInside(DecodeAbrt(m).userInformation, m);
		}
		catch(const DecodeError&)
		{}
	}
}

TEST_CASE(SUITE("An explicit protocol-version is preserved, as some peers send it"))
{
	//some peers write protocol-version {version1} although it is the default: "80 02 07 80"
	const Bytes wire = Hex("60 0d 80 02 07 80 a1 07 06 05 28 ca 22 02 03");
	const Aarq a = DecodeAarq(wire);
	REQUIRE(a.encodeProtocolVersion);
	REQUIRE(Encode(a) == wire);

	//absent on the wire, absent when re-encoded
	const Bytes minimal = Hex(AarqMinimal);
	const Aarq b = DecodeAarq(minimal);
	REQUIRE_FALSE(b.encodeProtocolVersion);
	REQUIRE(Encode(b) == minimal);

	Aarq c;
	REQUIRE(Encode(c) == Hex("60 09 a1 07 06 05 28 ca 22 02 03"));
	c.encodeProtocolVersion = true;
	REQUIRE(Encode(c) == wire);

	const Bytes rw = Hex("61 19 80 02 07 80 a1 07 06 05 28 ca 22 02 03 a2 03 02 01 00 a3 05 a1 03 02 01 00");
	const Aare r = DecodeAare(rw);
	REQUIRE(r.encodeProtocolVersion);
	REQUIRE(Encode(r) == rw);
	Aare d = r;
	d.encodeProtocolVersion = false;
	REQUIRE(Encode(d).size() == rw.size() - 4);
}

TEST_CASE(SUITE("Anything the AARQ decoder accepts can be encoded again"))
{
	const std::vector<Bytes> originals = {
		Hex(AarqMinimal), Hex(AarqWithTitles), Hex(AarqAuthentication), Hex(AarqForm1Titles),
	};
	uint32_t state = 99;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};
	for(int iter = 0; iter < 20000; ++iter)
	{
		Bytes m = originals[next() % originals.size()];
		m[next() % m.size()] = static_cast<uint8_t>(next());
		try
		{
			const Aarq a = DecodeAarq(m);
			const Bytes again = Encode(a);
			REQUIRE_NOTHROW(DecodeAarq(again));
		}
		catch(const DecodeError&)
		{}
	}
}

TEST_CASE(SUITE("ACSE travels through Presentation, Session and COTP: CN -> CP(AARQ) -> CPA -> AARE -> release"))
{
	using namespace iccp;
	namespace pres = iccp::presentation;
	const uint8_t tpduSize = cotp::TpduSize128; //forces segmentation

	constexpr int64_t AcseContext = 1;
	constexpr int64_t MmsContext = 3;

	//MMS Initiate PDUs big enough to need long form lengths
	ber::Writer iw;
	iw.Begin(ber::Tag::Context(8, true));
	iw.OctetString(Pattern(200, 1));
	iw.End();
	const Bytes initiateReq = iw.Release();
	ber::Writer rw;
	rw.Begin(ber::Tag::Context(9, true));
	rw.OctetString(Pattern(60, 2));
	rw.End();
	const Bytes initiateResp = rw.Release();

	Bytes stream;
	auto sendTsdu = [&](const Bytes& tsdu)
			    {
				    for(const Bytes& dt : cotp::Segment(tsdu, tpduSize))
				    {
					    const Bytes f = tpkt::Frame(dt);
					    stream.insert(stream.end(), f.begin(), f.end());
				    }
			    };
	auto sendSpdu = [&](session::SpduType type, const Bytes& userData)
			    {
				    session::Spdu s;
				    s.type = type;
				    s.protocolOptions = static_cast<uint8_t>(0);
				    s.version = session::Version::Version2;
				    s.requirements = session::Requirement::Duplex;
				    s.hasUserData = true;
				    s.userData = userData.data();
				    s.userDataLength = userData.size();
				    sendTsdu(session::Encode(s));
			    };

	//client: CN carrying a CP whose user data is the AARQ (ACSE context)
	{
		const Bytes aarq = Encode(TypicalAarq(initiateReq));
		pres::ConnectRequest cp;
		cp.callingSelector = Bytes{0, 0, 0, 1};
		cp.calledSelector = Bytes{0, 0, 0, 1};
		cp.contexts.push_back(pres::ContextDefinition{AcseContext, pres::AcseAbstractSyntax, {pres::BerTransferSyntax}
			});
		cp.contexts.push_back(pres::ContextDefinition{MmsContext, pres::MmsAbstractSyntax, {pres::BerTransferSyntax}
			});
		cp.userData.push_back(pres::MakePdv(AcseContext, aarq));
		sendSpdu(session::SpduType::Connect, pres::Encode(cp));
	}
	//server: AC carrying a CPA whose user data is the AARE
	{
		const Bytes aare = Encode(TypicalAare(initiateResp));
		pres::ConnectAccept cpa;
		cpa.respondingSelector = Bytes{0, 0, 0, 1};
		pres::ContextResult accepted;
		accepted.transferSyntax = pres::BerTransferSyntax;
		cpa.results = {accepted, accepted};
		cpa.userData.push_back(pres::MakePdv(AcseContext, aare));
		sendSpdu(session::SpduType::Accept, pres::Encode(cpa));
	}
	//release: RLRQ in a data-phase PPDU on the ACSE context, RLRE back
	{
		Rlrq rlrq;
		rlrq.reason = ReleaseReason::Normal;
		sendTsdu(session::EncodeData(pres::EncodeData(AcseContext, Encode(rlrq))));
		Rlre rlre;
		rlre.reason = ReleaseReason::Normal;
		sendTsdu(session::EncodeData(pres::EncodeData(AcseContext, Encode(rlre))));
	}

	for(size_t chunk : {size_t(1), size_t(7), size_t(128), size_t(100000)})
	{
		tpkt::Reader reader;
		cotp::TsduReassembler reassembler(1 << 20, tpduSize);
		Bytes payload;

		bool sawAarq = false, sawAare = false, sawRlrq = false, sawRlre = false;
		int64_t mmsContextFromCp = 0;
		Bytes initiateSeenByServer, initiateSeenByClient;

		for(size_t pos = 0; pos < stream.size(); pos += chunk)
		{
			reader.Feed(stream.data() + pos, std::min(chunk, stream.size() - pos));
			while(reader.Next(payload))
			{
				const cotp::Tpdu t = cotp::Decode(payload);
				REQUIRE(t.type == cotp::TpduType::Data);
				if(!reassembler.Add(t))
					continue;
				const Bytes tsdu = reassembler.Take();
				const session::Spdu s = session::Decode(tsdu);

				if(s.type == session::SpduType::Connect)
				{
					const pres::ConnectRequest cp = pres::DecodeConnectRequest(s.userData, s.userDataLength);
					for(const auto& ctx : cp.contexts)
						if(ctx.abstractSyntax == pres::MmsAbstractSyntax)
							mmsContextFromCp = ctx.id;
					REQUIRE(cp.userData.size() == 1);
					REQUIRE(cp.userData[0].contextId == AcseContext);
					REQUIRE(PeekApduType(cp.userData[0].data, cp.userData[0].dataLength) == ApduType::Aarq);
					const Aarq aarq = DecodeAarq(cp.userData[0].data, cp.userData[0].dataLength);
					REQUIRE(aarq.applicationContextName == MmsApplicationContext);
					REQUIRE(aarq.calledApTitle->oid == Oid({1, 1, 999, 1, 1}));
					REQUIRE(aarq.callingAeQualifier->number == 13);
					REQUIRE(aarq.userInformation.size() == 1);
					//the EXTERNAL's indirect reference names the MMS presentation context
					REQUIRE(*aarq.userInformation[0].indirectReference == mmsContextFromCp);
					initiateSeenByServer = DataOf(aarq.userInformation[0]);
					sawAarq = true;
				}
				else if(s.type == session::SpduType::Accept)
				{
					const pres::ConnectAccept cpa = pres::DecodeConnectAccept(s.userData, s.userDataLength);
					REQUIRE(cpa.results.size() == 2);
					REQUIRE(cpa.userData.size() == 1);
					const Aare aare = DecodeAare(cpa.userData[0].data, cpa.userData[0].dataLength);
					REQUIRE(aare.result == Result::Accepted);
					REQUIRE(*aare.userInformation.at(0).indirectReference == MmsContext);
					initiateSeenByClient = DataOf(aare.userInformation[0]);
					sawAare = true;
				}
				else
				{
					REQUIRE(s.type == session::SpduType::Data);
					const pres::Pdv p = pres::DecodeData(s.userData, s.userDataLength);
					REQUIRE(p.contextId == AcseContext);
					switch(PeekApduType(p.data, p.dataLength))
					{
						case ApduType::Rlrq:
							REQUIRE(*DecodeRlrq(p.data, p.dataLength).reason == ReleaseReason::Normal);
							sawRlrq = true;
							break;
						case ApduType::Rlre:
							REQUIRE(*DecodeRlre(p.data, p.dataLength).reason == ReleaseReason::Normal);
							sawRlre = true;
							break;
						default:
							FAIL("unexpected APDU");
					}
				}
			}
		}
		REQUIRE(sawAarq);
		REQUIRE(sawAare);
		REQUIRE(sawRlrq);
		REQUIRE(sawRlre);
		REQUIRE(mmsContextFromCp == MmsContext);
		REQUIRE(initiateSeenByServer == initiateReq);
		REQUIRE(initiateSeenByClient == initiateResp);
	}
}
