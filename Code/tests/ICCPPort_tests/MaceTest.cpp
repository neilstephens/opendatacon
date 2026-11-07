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
 * MaceTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE authentication: the encoding of the authentication value in the ACSE APDUs (all builds), the signature scheme and the
 *  verification rules (with OpenSSL), the configuration, and an optional local check against a real peer's capture (only when
 *  the environment variable ICCP_MACE_CAPTURE_DIR names a directory holding the ACSE APDUs; prints counts only).
 *  Synthetic values only.
 */

#include "Acse.h"
#include "ICCPPortConf.h"
#include "Mace.h"
#include "MaceConf.h"
#include "MaceTestPki.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#define SUITE(name) "ICCP MACE - " name

using namespace iccp;
using ber::Bytes;

namespace
{
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

Bytes Pattern(size_t n, uint8_t seed)
{
	Bytes b(n);
	for(size_t i = 0; i < n; ++i)
		b[i] = static_cast<uint8_t>(seed + i * 13);
	return b;
}

const std::string Time1 = "20260102030405Z";

//The nesting of a BER encoding as lines: class letter, number, "{" for constructed, and (optionally) the content length, indented by depth.
void SkeletonInto(const uint8_t* data, size_t length, int depth, bool lengths, bool skipUserInfo, std::vector<std::string>& out)
{
	static const char classes[] = {'U', 'A', 'C', 'P'};
	ber::Cursor c(data, length);
	while(!c.Empty())
	{
		const ber::Element e = c.Next();
		if(skipUserInfo && depth == 1 && e.tag.cls == ber::TagClass::ContextSpecific && e.tag.number == 30)
			continue;
		std::string line(static_cast<size_t>(depth) * 2, ' ');
		line += classes[static_cast<int>(e.tag.cls)] + std::to_string(e.tag.number) + (e.tag.constructed ? "{" : "");
		if(lengths)
			line += " " + std::to_string(e.contentLength);
		out.push_back(line);
		if(e.tag.constructed)
			SkeletonInto(e.content, e.contentLength, depth + 1, lengths, skipUserInfo, out);
	}
}

std::vector<std::string> Skeleton(const Bytes& b, bool lengths, bool skipUserInfo = true)
{
	std::vector<std::string> out;
	SkeletonInto(b.data(), b.size(), 0, lengths, skipUserInfo, out);
	return out;
}

acse::Aarq SampleAarq(const Bytes& cert, const Bytes& signature, const std::string& time = Time1)
{
	acse::Aarq a;
	a.encodeProtocolVersion = true;
	a.calledApTitle = acse::ApTitle{true, {1, 3, 9999, 1, 1, 1, 1, 1, 1}, {}
	};
	a.calledAeQualifier = acse::AeQualifier{true, 610, {}
	};
	a.callingApTitle = acse::ApTitle{true, {1, 3, 9999, 1, 1, 1, 1, 1, 2}, {}
	};
	a.callingAeQualifier = acse::AeQualifier{true, 153, {}
	};
	a.authentication = acse::MakeMaceAuthentication({1, 3, 9999, 7, 1, 1, 1, 1}, acse::EncodeMaceAuthValue(cert, time, signature));
	return a;
}

acse::Aare SampleAare(const Bytes& cert, const Bytes& signature, const std::string& time = Time1)
{
	acse::Aare a;
	a.encodeProtocolVersion = true;
	a.respondingApTitle = acse::ApTitle{true, {1, 3, 9999, 1, 1, 1, 1, 1, 1}, {}
	};
	a.respondingAeQualifier = acse::AeQualifier{true, 610, {}
	};
	a.authentication = acse::MakeMaceAuthentication({1, 3, 9999, 7, 1, 1, 1, 1}, acse::EncodeMaceAuthValue(cert, time, signature));
	return a;
}
}

// ---------------------------------------------------------------------------------------------
// the encoding of the authentication value (all builds)
// ---------------------------------------------------------------------------------------------

TEST_CASE(SUITE("GeneralizedTime format and parse"))
{
	struct V
	{
		int64_t t;
		const char* text;
	};
	for(const V& v : {V{0, "19700101000000Z"}, V{1700000000, "20231114221320Z"}, V{1709164800, "20240229000000Z"}, V{1709251199, "20240229235959Z"},
	                  V{951782400, "20000229000000Z"}, V{4102444800, "21000101000000Z"}, V{253402300799, "99991231235959Z"}, V{-1, "19691231235959Z"}
	    })
	{
		REQUIRE(acse::FormatGeneralizedTime(v.t) == v.text);
		int64_t back = 12345;
		REQUIRE(acse::ParseGeneralizedTime(v.text, back));
		REQUIRE(back == v.t);
	}
	//a round trip over many instants, including month and year ends
	for(int64_t t = -400000000; t < 4000000000; t += 86399 * 31 + 17)
	{
		int64_t back = 0;
		REQUIRE(acse::ParseGeneralizedTime(acse::FormatGeneralizedTime(t), back));
		REQUIRE(back == t);
	}
	for(const char* bad : {"", "2026010203040Z", "202601020304055Z", "20260102030405z", "20260102030405", "20260102030405Y", "2026-1-2T03:04:05Z", "20261302030405Z", "20260002030405Z",
	                       "20260132030405Z", "20260230030405Z", "20250229030405Z", "20260102240405Z", "20260102036005Z", "20260102030460Z", "2026010203040 Z", "+0260102030405Z"})
	{
		int64_t t;
		INFO(bad);
		REQUIRE_FALSE(acse::ParseGeneralizedTime(bad, t));
	}
	REQUIRE_THROWS_AS(acse::FormatGeneralizedTime(253402300800), std::invalid_argument);
	REQUIRE_THROWS_AS(acse::FormatGeneralizedTime(-62167219201), std::invalid_argument);
}

TEST_CASE(SUITE("The authentication value has the layout of a real peer's"))
{
	//[2] { [0] { [0] { [0] certificate, [1] time, [2] signature } } }, built by hand
	const Bytes cert = {0xAA, 0xAA, 0xAA, 0xAA}, sig = {0xBB, 0xBB, 0xBB};
	const Bytes expected = Hex("a2 20  a0 1e  a0 1c  80 04 aaaaaaaa  81 0f 32303236303130323033303430 35 5a  82 03 bbbbbb");
	REQUIRE(acse::EncodeMaceAuthValue(cert, Time1, sig) == expected);

	const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(expected);
	REQUIRE(Bytes(v.certificate, v.certificate + v.certificateLength) == cert);
	REQUIRE(Bytes(v.signature, v.signature + v.signatureLength) == sig);
	REQUIRE(v.SigningTimeText() == Time1);
}

TEST_CASE(SUITE("Authentication value: long forms, round trip and malformed input"))
{
	//a certificate sized element (1217 octets: long form length) and an RSA 2048 signature
	for(size_t certSize : {size_t(1), size_t(127), size_t(128), size_t(255), size_t(994), size_t(1217), size_t(70000)})
	{
		const Bytes cert = Pattern(certSize, 1), sig = Pattern(256, 7);
		const Bytes value = acse::EncodeMaceAuthValue(cert, Time1, sig);
		const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(value);
		REQUIRE(v.certificateLength == certSize);
		REQUIRE(Bytes(v.certificate, v.certificate + v.certificateLength) == cert);
		REQUIRE(v.signatureLength == 256);
		REQUIRE(v.SigningTimeText() == Time1);
	}

	const Bytes cert = Pattern(1217, 1), sig = Pattern(256, 7);
	const Bytes value = acse::EncodeMaceAuthValue(cert, Time1, sig);

	//every truncation fails, never reads past the end
	for(size_t n = 0; n < value.size(); n += (n < 40 || n + 40 > value.size()) ? 1 : 37)
	{
		const Bytes cut(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(n));
		REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(cut), ber::DecodeError);
	}
	//a trailing octet
	Bytes longer = value;
	longer.push_back(0);
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(longer), ber::DecodeError);
	//the encoder refuses what it cannot encode
	REQUIRE_THROWS_AS(acse::EncodeMaceAuthValue(Bytes(), Time1, sig), std::invalid_argument);
	REQUIRE_THROWS_AS(acse::EncodeMaceAuthValue(cert, Time1, Bytes()), std::invalid_argument);
	REQUIRE_THROWS_AS(acse::EncodeMaceAuthValue(cert, "2026-01-02T03:04:05Z", sig), std::invalid_argument);

	//structure errors
	const Bytes small = acse::EncodeMaceAuthValue(Bytes{1, 2}, Time1, Bytes{3, 4});
	REQUIRE_NOTHROW(acse::DecodeMaceAuthValue(small));
	auto retag = [&](size_t pos, uint8_t tag)
			 {
				 Bytes b = small;
				 b[pos] = tag;
				 return b;
			 };
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(0, 0xA3)), ber::DecodeError);  //not the [2] alternative
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(0, 0x82)), ber::DecodeError);  //primitive
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(2, 0xA1)), ber::DecodeError);  //not the single-ASN1-type [0]
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(4, 0xA1)), ber::DecodeError);  //not the inner [0]
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(6, 0x83)), ber::DecodeError);  //no certificate member ([0] gone, a [3] unknown member instead)
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(retag(10, 0x80)), ber::DecodeError); //two certificates and no signing time
	//a duplicate member
	{
		acse::Bytes dup = Hex("a2 0c a0 0a a0 08 80 01 aa 80 01 bb 81 00");
		REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(dup), ber::DecodeError);
	}
	//two elements at the second level
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(Hex("a2 0a a0 04 a0 02 80 00 a0 02 a0 00")), ber::DecodeError);
	REQUIRE_THROWS_AS(acse::DecodeMaceAuthValue(Bytes()), ber::DecodeError);

	//unknown further members are skipped; the three known ones are still found
	const Bytes extra = Hex("a2 24  a0 22  a0 20  80 02 0102  83 03 ffffff  81 0f 32303236303130323033303430 35 5a  84 00  82 02 0304");
	const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(extra);
	REQUIRE(v.certificateLength == 2);
	REQUIRE(v.signatureLength == 2);
	REQUIRE(v.SigningTimeText() == Time1);
}

TEST_CASE(SUITE("Mutated authentication values never crash"))
{
	const Bytes value = acse::EncodeMaceAuthValue(Pattern(300, 1), Time1, Pattern(256, 7));
	std::mt19937 rng(12345);
	size_t accepted = 0;
	for(int i = 0; i < 4000; ++i)
	{
		Bytes b = value;
		const int flips = 1 + static_cast<int>(rng() % 3);
		for(int f = 0; f < flips; ++f)
			b[rng() % b.size()] ^= static_cast<uint8_t>(1u << (rng() % 8));
		if(i % 5 == 0)
			b.resize(rng() % (b.size() + 1));
		try
		{
			const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(b);
			REQUIRE(v.certificate >= b.data());
			REQUIRE(v.certificate + v.certificateLength <= b.data() + b.size());
			REQUIRE(v.signature + v.signatureLength <= b.data() + b.size());
			++accepted;
		}
		catch(const ber::DecodeError&)
		{}
	}
	REQUIRE(accepted > 0); //(a flip inside the certificate is still a valid value)
}

TEST_CASE(SUITE("An AARQ and an AARE with authentication round trip byte for byte"))
{
	const Bytes cert = Pattern(994, 3), sig = Pattern(256, 9);
	const Bytes userInfo = Hex("a8 02 80 00"); //(the External only views its data)
	acse::Aarq aarq = SampleAarq(cert, sig);
	aarq.userInformation.push_back(acse::MakeExternal(3, userInfo));
	const Bytes bytes = acse::Encode(aarq);

	const acse::Aarq out = acse::DecodeAarq(bytes);
	REQUIRE(out.authentication.acseRequirements == acse::AcseRequirement::Authentication);
	REQUIRE(out.authentication.mechanismName == aarq.authentication.mechanismName);
	REQUIRE(out.authentication.authenticationValue == aarq.authentication.authenticationValue);
	REQUIRE(acse::Encode(out) == bytes);
	const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(*out.authentication.authenticationValue);
	REQUIRE(v.certificateLength == 994);
	REQUIRE(v.signatureLength == 256);

	//the tag octets of the three fields, in order, between the AE-qualifier [7] and the user information [30]
	const Bytes calling = Hex("a7 04 02 02 00 99");
	const Bytes tail(std::search(bytes.begin(), bytes.end(), calling.begin(), calling.end()) + static_cast<std::ptrdiff_t>(calling.size()), bytes.end());
	REQUIRE(tail[0] == 0x8A);
	REQUIRE(tail[1] == 0x02);
	REQUIRE(tail[2] == 0x07);
	REQUIRE(tail[3] == 0x80);
	REQUIRE(tail[4] == 0x8B);
	REQUIRE(tail[5] == 0x08);
	REQUIRE(tail[14] == 0xAC);
	REQUIRE(tail[15] == 0x82);
	REQUIRE(tail[18] == 0xA2); //the [2] external alternative inside the [12] wrapper

	acse::Aare aare = SampleAare(cert, sig);
	const Bytes aareInfo = Hex("a9 02 80 00");
	aare.userInformation.push_back(acse::MakeExternal(3, aareInfo));
	const Bytes aareBytes = acse::Encode(aare);
	const acse::Aare aout = acse::DecodeAare(aareBytes);
	REQUIRE(aout.authentication.acseRequirements == acse::AcseRequirement::Authentication);
	REQUIRE(aout.authentication.mechanismName == aare.authentication.mechanismName);
	REQUIRE(aout.authentication.authenticationValue == aare.authentication.authenticationValue);
	REQUIRE(acse::Encode(aout) == aareBytes);
	const Bytes responding = Hex("a5 04 02 02 02 62"); //responding AE-qualifier
	const Bytes aareTail(std::search(aareBytes.begin(), aareBytes.end(), responding.begin(), responding.end()) + static_cast<std::ptrdiff_t>(responding.size()), aareBytes.end());
	REQUIRE(aareTail[0] == 0x88);  //responder-acse-requirements
	REQUIRE(aareTail[4] == 0x89);  //mechanism-name
	REQUIRE(aareTail[14] == 0xAA); //responding-authentication-value
}

TEST_CASE(SUITE("The encoder's skeleton equals a real peer's (tags, nesting, lengths of the authentication; no values)"))
{
	const Bytes cert = Pattern(994, 3), sig = Pattern(256, 9);
	const Bytes userInfo = Hex("a8 02 80 00"), aareInfo = Hex("a9 02 80 00");
	acse::Aarq aarq = SampleAarq(cert, sig);
	aarq.userInformation.push_back(acse::MakeExternal(3, userInfo));
	const std::vector<std::string> expectedAarq = {
		"A0{ 1289", "  C0 2", "  C1{ 7", "    U6 5", "  C2{ 11", "    U6 9", "  C3{ 4", "    U2 2", "  C6{ 11", "    U6 9", "  C7{ 4", "    U2 2",
		"  C10 2", "  C11 8", "  C12{ 1287", "    C2{ 1283", "      C0{ 1279", "        C0{ 1275", "          C0 994", "          C1 15", "          C2 256"};
	// ([30] user information excluded; the lengths of A0 differ by what is excluded, so the first line is only compared by its tag)
	std::vector<std::string> got = Skeleton(acse::Encode(aarq), true);
	REQUIRE(got.size() == expectedAarq.size());
	got[0] = "A0{ 1289";
	REQUIRE(got == expectedAarq);

	acse::Aare aare = SampleAare(Pattern(1000, 3), sig);
	aare.diagnostic = acse::SourceDiagnostic{false, 0};
	aare.userInformation.push_back(acse::MakeExternal(3, aareInfo));
	const std::vector<std::string> expectedAare = {
		"A1{ 0", "  C0 2", "  C1{ 7", "    U6 5", "  C2{ 3", "    U2 1", "  C3{ 5", "    C1{ 3", "      U2 1", "  C4{ 11", "    U6 9", "  C5{ 4", "    U2 2",
		"  C8 2", "  C9 8", "  C10{ 1293", "    C2{ 1289", "      C0{ 1285", "        C0{ 1281", "          C0 1000", "          C1 15", "          C2 256"};
	got = Skeleton(acse::Encode(aare), true);
	REQUIRE(got.size() == expectedAare.size());
	got[0] = "A1{ 0";
	REQUIRE(got == expectedAare);
}

TEST_CASE(SUITE("Without OpenSSL the encoding works and a MACE context cannot be made"))
{
	if(mace::MaceSupported())
		return;
	mace::MaceConf conf;
	conf.enabled = true;
	std::string error;
	REQUIRE(mace::Context::Create(conf, error) == nullptr);
	REQUIRE(error.find("no MACE support") != std::string::npos);
	REQUIRE(mace::SignedBlock(Time1).size() == 30);
}

TEST_CASE(SUITE("The signed block is the SHA-1 DigestInfo header followed by the time text"))
{
	const Bytes block = mace::SignedBlock(Time1);
	REQUIRE(block.size() == 30);
	REQUIRE(Bytes(block.begin(), block.begin() + 15) == Hex("30 21 30 09 06 05 2b 0e 03 02 1a 05 00 04 14"));
	REQUIRE(Bytes(block.begin() + 15, block.end()) == Bytes(Time1.begin(), Time1.end()));
	REQUIRE(mace::IsoTime(1700000000) == "2023-11-14T22:13:20Z");
}

// ---------------------------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------------------------

namespace
{
Json::Value ParseJson(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string errs;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b, in, &v, &errs));
	return v;
}

std::vector<std::string> Parse(const std::string& text, mace::MaceConf& conf)
{
	std::vector<std::string> problems;
	mace::ParseMaceConf(ParseJson(text), conf, problems);
	return problems;
}

bool Mentions(const std::vector<std::string>& problems, const std::string& part)
{
	for(const auto& p : problems)
		if(p.find(part) != std::string::npos)
			return true;
	return false;
}
}

TEST_CASE(SUITE("Configuration defaults and shape"))
{
	mace::MaceConf conf;
	REQUIRE(Parse("{}", conf).empty());
	REQUIRE_FALSE(conf.enabled);

	auto problems = Parse(R"({"MACE":{"Enabled":false}})", conf);
	REQUIRE(problems.empty());
	REQUIRE(conf.timeWindowSeconds == 600);
	REQUIRE(conf.minKeyBits == 2048);
	REQUIRE(conf.maxKeyBits == 8192);
	REQUIRE(conf.requireAuthentication);
	REQUIRE(conf.requireResponderAuthentication);
	REQUIRE(conf.verifyPeer);
	REQUIRE(conf.mechanismText == "1.0.62351.4.3.1");
	REQUIRE(conf.mechanismName == acse::Oid({1, 0, 62351, 4, 3, 1}));

	problems = Parse(R"({"MACE":{"Enabled":false,"TimeWindowSeconds":30,"MinKeyBits":3072,"MaxKeyBits":4096,"RequireAuthentication":false,
		"RequireResponderAuthentication":false,"VerifyPeer":false,"PeerCertAllowList":["CN:a","SAN:b","SHA256:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"]}})", conf);
	REQUIRE(problems.empty());
	REQUIRE(conf.timeWindowSeconds == 30);
	REQUIRE(conf.minKeyBits == 3072);
	REQUIRE(conf.maxKeyBits == 4096);
	REQUIRE_FALSE(conf.requireAuthentication);
	REQUIRE_FALSE(conf.requireResponderAuthentication);
	REQUIRE_FALSE(conf.verifyPeer);
	REQUIRE(conf.peerCertAllowList.size() == 3);

	REQUIRE(Mentions(Parse(R"({"MACE":5})", conf), "must be a JSON object"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"Bogus":1}})", conf), "unknown key \"Bogus\""));
	REQUIRE(Mentions(Parse(R"({"MACE":{"KeyPassword":"x"}})", conf), "KeyPasswordEnv"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"Enabled":"yes"}})", conf), "\"Enabled\" must be true or false"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"TimeWindowSeconds":0}})", conf), "\"TimeWindowSeconds\" must be between"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"MaxKeyBits":16384}})", conf), "\"MaxKeyBits\" must be between 1024 and 8192"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"PeerCertAllowList":["bogus"]}})", conf), "PeerCertAllowList"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"PeerCertAllowList":["SHA256:12"]}})", conf), "64 hexadecimal"));
}

TEST_CASE(SUITE("An enabled configuration is checked"))
{
	mace::MaceConf conf;
	if(!mace::MaceSupported())
	{
		const auto problems = Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.3.9999.7.1","CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf);
		REQUIRE(problems.size() == 1);
		REQUIRE(problems[0] == "\"MACE\": this build has no MACE support: rebuild with ODC_ASIO_SSL=ON (and OpenSSL) to use \"Enabled\": true");
		return;
	}
	const std::string good = R"("Enabled":true,"MechanismName":"1.3.9999.7.1","CertFile":"a","KeyFile":"b","CAFile":"c")";
	REQUIRE(Parse("{\"MACE\":{" + good + "}}", conf).empty());
	REQUIRE(conf.mechanismName == acse::Oid({1, 3, 9999, 7, 1}));

	//MechanismName is optional: the official one by default, validated when given
	REQUIRE(Parse(R"({"MACE":{"Enabled":true,"CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf).empty());
	REQUIRE(conf.mechanismText == "1.0.62351.4.3.1");
	REQUIRE(conf.mechanismName == mace::OfficialMechanismName);
	{
		ber::Writer w;
		w.ObjectIdentifier(conf.mechanismName);
		REQUIRE(w.Release() == ber::Bytes({0x06, 0x07, 0x28, 0x83, 0xE7, 0x0F, 0x04, 0x03, 0x01}));
	}
	REQUIRE(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.0.840.0.1.0.1.1","CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf).empty());
	REQUIRE(conf.mechanismName == acse::Oid({1, 0, 840, 0, 1, 0, 1, 1}));
	REQUIRE(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.0.62351.4.3.1","CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf).empty());
	REQUIRE(conf.mechanismName == mace::OfficialMechanismName);
	REQUIRE(Mentions(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"","CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf), "dotted object identifier"));
	REQUIRE_FALSE(Mentions(Parse(R"({"MACE":{"Enabled":true,"CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf), "MechanismName"));
	for(const char* bad : {"1", "1.3.x", "3.1.2", "1.40.1", "1..2", "1.3.9999.", "a.b"})
		REQUIRE(Mentions(Parse(std::string(R"({"MACE":{"Enabled":true,"MechanismName":")") + bad + R"(","CertFile":"a","KeyFile":"b","CAFile":"c"}})", conf), "dotted object identifier"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.3.9999.7.1","KeyFile":"b","CAFile":"c"}})", conf), "\"CertFile\" is required"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.3.9999.7.1","CertFile":"a","CAFile":"c"}})", conf), "\"KeyFile\" is required"));
	REQUIRE(Mentions(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.3.9999.7.1","CertFile":"a","KeyFile":"b"}})", conf), "\"CAFile\" (or \"CAPath\") is required"));
	REQUIRE(Parse(R"({"MACE":{"Enabled":true,"MechanismName":"1.3.9999.7.1","CertFile":"a","KeyFile":"b","VerifyPeer":false}})", conf).empty());
	REQUIRE(Mentions(Parse("{\"MACE\":{" + good + R"(,"MinKeyBits":4096,"MaxKeyBits":2048}})", conf), "\"MinKeyBits\" must not exceed"));
	REQUIRE(Mentions(Parse("{\"MACE\":{" + good + R"(,"VerifyPeer":false,"PeerCertAllowList":["CN:x"]}})", conf), "needs \"VerifyPeer\""));
	REQUIRE(Mentions(Parse("{\"MACE\":{" + good + R"(,"CRLCheckChain":true}})", conf), "needs a \"CRLFile\""));
}

#ifdef ODC_ASIO_SSL

TEST_CASE(SUITE("The port configuration builds the context and reports the files that do not fit"))
{
	auto& pki = macetest::GetPki();
	auto load = [&](const Json::Value& mace)
			{
				Json::Value root;
				root["MACE"] = mace;
				ICCPPortConf conf;
				auto problems = conf.Load(root, false);
				return std::make_pair(problems, conf.maceContext != nullptr);
			};
	auto maceJson = [&](const macetest::Files& id)
			    {
				    Json::Value m;
				    m["Enabled"] = true;
				    m["MechanismName"] = macetest::Mechanism;
				    m["CertFile"] = id.cert;
				    m["KeyFile"] = id.key;
				    m["CAFile"] = pki.caFile;
				    return m;
			    };
	{
		const auto r = load(maceJson(pki.a));
		const std::string firstProblem = r.first.empty() ? std::string("no problems") : r.first[0];
		INFO(firstProblem);
		REQUIRE(r.second);
		REQUIRE_FALSE(Mentions(r.first, "MACE"));
	}
	{ //the certificate of one node with the key of another
		Json::Value m = maceJson(pki.a);
		m["KeyFile"] = pki.b.key;
		const auto r = load(m);
		REQUIRE_FALSE(r.second);
		REQUIRE(Mentions(r.first, "do not belong together"));
	}
	{
		Json::Value m = maceJson(pki.a);
		m["CertFile"] = pki.dir.Path("missing.crt");
		REQUIRE(Mentions(load(m).first, "\"CertFile\""));
		m = maceJson(pki.a);
		m["KeyFile"] = pki.a.cert; //a certificate is not a key
		REQUIRE(Mentions(load(m).first, "not a valid PEM private key"));
		m = maceJson(pki.a);
		m["CAFile"] = pki.a.key;
		REQUIRE(Mentions(load(m).first, "\"CAFile\""));
		m = maceJson(pki.a);
		m["CRLFile"] = pki.a.key;
		REQUIRE(Mentions(load(m).first, "\"CRLFile\""));
	}
	{ //size limits: the key must be inside them, the limits themselves are bounded
		Json::Value m = maceJson(pki.rsa3072F);
		REQUIRE(load(m).second);
		m["MaxKeyBits"] = 2048;
		auto r = load(m);
		REQUIRE_FALSE(r.second);
		REQUIRE(Mentions(r.first, "3072 bits, outside the accepted 2048 to 2048"));
		m = maceJson(pki.a);
		m["MinKeyBits"] = 4096;
		r = load(m);
		REQUIRE(Mentions(r.first, "2048 bits, outside the accepted 4096 to 8192"));
		m["MinKeyBits"] = 2048;
		m["MaxKeyBits"] = 8193;
		REQUIRE(Mentions(load(m).first, "\"MaxKeyBits\" must be between 1024 and 8192"));
	}
	{ //EC keys are not supported for MACE
		const auto r = load(maceJson(pki.ecF));
		REQUIRE_FALSE(r.second);
		REQUIRE(Mentions(r.first, "not an RSA key"));
	}
}

TEST_CASE(SUITE("A mechanism-name that is not the official one gives a configuration warning"))
{
	auto& pki = macetest::GetPki();
	auto load = [&](const char* mechanism)
			{
				Json::Value m;
				m["Enabled"] = true;
				if(mechanism)
					m["MechanismName"] = mechanism;
				m["CertFile"] = pki.a.cert;
				m["KeyFile"] = pki.a.key;
				m["CAFile"] = pki.caFile;
				Json::Value root;
				root["MACE"] = m;
				auto conf = std::make_unique<ICCPPortConf>();
				const auto problems = conf->Load(root, false);
				REQUIRE(problems.empty());
				REQUIRE(conf->maceContext != nullptr);
				return conf;
			};
	auto count = [](const ICCPPortConf& c)
			 {
				 size_t n = 0;
				 for(const auto& w : c.warnings)
					 if(w.find("MechanismName") != std::string::npos)
						 ++n;
				 return n;
			 };
	REQUIRE(count(*load(nullptr)) == 0);           //the default
	REQUIRE(count(*load("1.0.62351.4.3.1")) == 0); //the official one, explicit
	const auto legacy = load("1.0.840.0.1.0.1.1");
	REQUIRE(count(*legacy) == 1);
	REQUIRE(legacy->maceConf.mechanismName == mace::LegacyMechanismName);
	const std::string& w = legacy->warnings.back();
	REQUIRE(w.find("1.0.840.0.1.0.1.1") != std::string::npos);
	REQUIRE(w.find("not the mechanism-name IEC 62351-4 defines (1.0.62351.4.3.1)") != std::string::npos);
	REQUIRE(w.find("only for peers that use that value") != std::string::npos);
	REQUIRE(w.find("legacy value of IEC TS 62351-4:2007") != std::string::npos);
	const auto other = load("1.3.9999.7.1");
	REQUIRE(count(*other) == 1);
	REQUIRE(other->warnings.back().find("legacy value") == std::string::npos);
}

TEST_CASE(SUITE("An encrypted key is read with the password from the environment"))
{
	auto& pki = macetest::GetPki();
	tlstest::CertSpec spec = macetest::Pki::Node("mace-enc.test", 40);
	spec.keyPassword = "synthetic-password";
	const tlstest::Cert enc = tlstest::MakeCert(spec, &pki.ca);
	const macetest::Files f = pki.Save("enc", enc);
	mace::MaceConf conf = macetest::Conf(f, pki.caFile);
	std::string error;
	REQUIRE(mace::Context::Create(conf, error) == nullptr);
	REQUIRE(error.find("not a valid PEM private key") != std::string::npos);
	conf.keyPasswordEnv = "ICCP_MACE_TEST_PASSWORD_UNSET_XYZ";
	REQUIRE(mace::Context::Create(conf, error) == nullptr);
	REQUIRE(error.find("is not set") != std::string::npos);
	conf.keyPasswordEnv = "ICCP_MACE_TEST_PASSWORD";
	macetest::SetEnv("ICCP_MACE_TEST_PASSWORD", "wrong");
	REQUIRE(mace::Context::Create(conf, error) == nullptr);
	REQUIRE(error.find("password is wrong") != std::string::npos);
	macetest::SetEnv("ICCP_MACE_TEST_PASSWORD", "synthetic-password");
	REQUIRE(mace::Context::Create(conf, error) != nullptr);
}

#endif
