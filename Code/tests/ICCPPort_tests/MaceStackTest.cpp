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
 * MaceStackTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE in the association state machine: two Connection instances back to back in memory (see StackTestHelpers.h), with a fake UTC
 *  clock and certificates made at run time. Only with ODC_ASIO_SSL. Synthetic names only.
 */

#ifdef ODC_ASIO_SSL

#include "IccpStack.h"
#include "MaceTestPki.h"
#include "StackTestHelpers.h"
#include <algorithm>
#include <catch.hpp>

#define SUITE(name) "ICCP MACE stack - " name

using namespace iccp;
using iccp::stack::EndReason;
using iccp::stack::LogLevel;
using iccp::stacktest::Endpoint;
using iccp::stacktest::Link;
using iccp::stacktest::Tweak;
using macetest::Conf;
using macetest::GetPki;
using macetest::Make;
using macetest::RealNow;

namespace
{
using ContextPtr = std::shared_ptr<mace::Context>;

//the UTC clocks of the two ends
struct Clocks
{
	int64_t a = RealNow();
	int64_t b = a;
};

std::function<void(Tweak&, Tweak&)> Setup(Clocks& clocks, ContextPtr a, ContextPtr b)
{
	return [&clocks, a, b](Tweak& ta, Tweak& tb)
		 {
			 ta.config.mace = a;
			 tb.config.mace = b;
			 ta.config.utcClock = [&clocks] { return clocks.a; };
			 tb.config.utcClock = [&clocks] { return clocks.b; };
		 };
}

ContextPtr Node(const macetest::Files& id, const std::string& ca, const std::function<void(mace::MaceConf&)>& tweak = nullptr)
{
	auto conf = Conf(id, ca);
	if(tweak)
		tweak(conf);
	return Make(conf);
}

//change the last digit of the seconds of a signing time in the bytes (so the signature no longer fits it)
void SpoilTime(ber::Bytes& frames, int64_t utc)
{
	const std::string text = acse::FormatGeneralizedTime(utc);
	const auto pos = std::search(frames.begin(), frames.end(), text.begin(), text.end());
	REQUIRE(pos != frames.end());
	char& digit = *reinterpret_cast<char*>(&*(pos + 13));
	digit = digit == '9' ? '0' : static_cast<char>(digit + 1);
}

//run the exchange by hand up to the initiator's connect (the AARQ) in a.out
void ToAarq(Link& link)
{
	link.a.conn.Start();
	link.b.conn.Start();
	link.PumpOnly(link.a); //CR
	link.PumpOnly(link.b); //CC: the initiator now emits the session connect with the AARQ
	REQUIRE_FALSE(link.a.out.empty());
}

const char* Refused14 = "diagnostic from the ACSE service user: authentication-required (14)";
const char* Refused13 = "diagnostic from the ACSE service user: authentication-failure (13)";

//the TSDU in a set of TPKT frames
ber::Bytes TsduOfFrames(const ber::Bytes& frames, const stack::Connection& conn)
{
	tpkt::Reader reader;
	cotp::TsduReassembler re(cotp::TsduReassembler::DefaultMaxTsdu, conn.GetNegotiated().tpduSizeCode);
	reader.Feed(frames);
	ber::Bytes payload, tsdu;
	while(reader.Next(payload))
	{
		const cotp::Tpdu t = cotp::Decode(payload);
		if(re.Add(t))
			tsdu = re.Take();
	}
	REQUIRE_FALSE(tsdu.empty());
	return tsdu;
}

ber::Bytes FramesOfTsdu(const ber::Bytes& tsdu, const stack::Connection& conn)
{
	ber::Bytes out;
	for(const ber::Bytes& seg : cotp::Segment(tsdu, conn.GetNegotiated().tpduSizeCode))
	{
		const ber::Bytes f = tpkt::Frame(seg);
		out.insert(out.end(), f.begin(), f.end());
	}
	return out;
}

//replace the first occurrence of `from` in the bytes by `to` (the same length)
void Replace(ber::Bytes& frames, const ber::Bytes& from, const ber::Bytes& to)
{
	REQUIRE(from.size() == to.size());
	const auto pos = std::search(frames.begin(), frames.end(), from.begin(), from.end());
	REQUIRE(pos != frames.end());
	std::copy(to.begin(), to.end(), pos);
}

//the association accept the responder emitted last: the presentation results and the ACSE AARE
struct AcceptParts
{
	ber::Bytes tsdu;
	ber::Bytes ppdu;
	ber::Bytes aareBytes;
	presentation::ConnectAccept cpa;
	acse::Aare aare;
};
void DecodeAccept(const ber::Bytes& frames, const stack::Connection& conn, AcceptParts& out)
{
	out.tsdu = TsduOfFrames(frames, conn);
	const session::Spdu s = session::Decode(out.tsdu);
	REQUIRE(s.type == session::SpduType::Accept);
	out.ppdu = ber::Bytes(s.userData, s.userData + s.userDataLength);
	out.cpa = presentation::DecodeConnectAccept(out.ppdu);
	REQUIRE(out.cpa.userData.size() == 1);
	out.aareBytes = ber::Bytes(out.cpa.userData[0].data, out.cpa.userData[0].data + out.cpa.userData[0].dataLength);
	out.aare = acse::DecodeAare(out.aareBytes);
}

//the initiator's association request as the responder gets it, with an extra presentation context offered: delivered to b by hand.
//Leaves the responder's answer in b.out.
void OfferContext(Link& link, const presentation::ContextDefinition& extra)
{
	ToAarq(link);
	const ber::Bytes tsdu = TsduOfFrames(link.a.out.back(), link.a.conn);
	link.a.out.clear();
	session::Spdu s = session::Decode(tsdu);
	REQUIRE(s.type == session::SpduType::Connect);
	presentation::ConnectRequest cp = presentation::DecodeConnectRequest(s.userData, s.userDataLength);
	cp.contexts.push_back(extra);
	const ber::Bytes cpBytes = presentation::Encode(cp);
	s.userData = cpBytes.data();
	s.userDataLength = cpBytes.size();
	const ber::Bytes frames = FramesOfTsdu(session::Encode(s), link.a.conn);
	link.b.conn.OnData(frames.data(), frames.size());
	REQUIRE_FALSE(link.b.out.empty());
}
}

TEST_CASE(SUITE("Mutual authentication: both ends verify, both associate"))
{
	auto& pki = GetPki();
	for(size_t chunk : {size_t(0), size_t(97)})
	{
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		link.chunk = chunk;
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		REQUIRE(link.b.conn.IsAssociated());

		const mace::Status& sa = link.a.conn.GetMaceStatus();
		const mace::Status& sb = link.b.conn.GetMaceStatus();
		REQUIRE(sa.enabled);
		REQUIRE(sa.state == mace::State::Verified);
		REQUIRE(sb.state == mace::State::Verified);
		REQUIRE(sa.peerSubject.find("mace-b.test") != std::string::npos);
		REQUIRE(sb.peerSubject.find("mace-a.test") != std::string::npos);
		REQUIRE_FALSE(sa.peerNotAfter.empty());
		REQUIRE(sa.signingTime == clocks.a);
		REQUIRE(sa.peerSigningTime == clocks.b);
		REQUIRE(sb.signingTime == clocks.b);
		REQUIRE(sb.peerSigningTime == clocks.a);
		REQUIRE(sa.lastError.empty());

		//the states reported on the way: Sent (the AARQ is out), then Verified (the AARE checked)
		REQUIRE(link.a.maceStates.size() == 2);
		REQUIRE(link.a.maceStates[0].state == mace::State::Sent);
		REQUIRE(link.a.maceStates[1].state == mace::State::Verified);
		REQUIRE(link.b.maceStates.back().state == mace::State::Verified);

		const std::string iso = mace::IsoTime(clocks.a);
		REQUIRE(link.a.CountLogs("MACE: authenticating with certificate CN=mace-a.test, signing time " + iso, LogLevel::Info) == 1);
		REQUIRE(link.b.CountLogs("MACE: peer authenticated: CN=mace-a.test, signing time " + iso, LogLevel::Info) == 1);
		REQUIRE(link.b.CountLogs("MACE: authenticating with certificate CN=mace-b.test", LogLevel::Info) == 1);
		REQUIRE(link.a.CountLogs("MACE: peer authenticated: CN=mace-b.test", LogLevel::Info) == 1);
		REQUIRE(link.a.CountLogs("BEGIN") == 0); //no key material in the logs
		for(const auto* e : {&link.a, &link.b})
			REQUIRE(e->CountLogs("PRIVATE") == 0);

		//the association works
		bool answered = false;
		link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const stack::Outcome& o) { answered = o.Ok(); });
		link.Pump();
		REQUIRE(answered);
	}
}

TEST_CASE(SUITE("A fresh signature for every association"))
{
	auto& pki = GetPki();
	Clocks clocks;
	std::vector<int64_t> times;
	for(int i = 0; i < 3; ++i)
	{
		clocks.a = clocks.b = RealNow() + i * 7;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		times.push_back(link.b.conn.GetMaceStatus().peerSigningTime);
	}
	REQUIRE(times[0] != times[1]);
	REQUIRE(times[1] != times[2]);
}

TEST_CASE(SUITE("The responder requires authentication and none is sent"))
{
	auto& pki = GetPki();
	Clocks clocks;
	Link link(Setup(clocks, nullptr, Node(pki.b, pki.caFile)));
	link.Start();
	REQUIRE_FALSE(link.a.conn.IsAssociated());
	REQUIRE_FALSE(link.b.conn.IsAssociated());
	REQUIRE(link.b.endReason == EndReason::Refused);
	REQUIRE(link.b.CountLogs("refusing the association: MACE: the association request carries no authentication", LogLevel::Warn) == 1);
	REQUIRE(link.b.maceStates.back().state == mace::State::Failed);
	REQUIRE(link.b.conn.GetMaceStatus().lastError.find("RequireAuthentication") != std::string::npos);
	//the diagnostic as the initiator logs it
	REQUIRE(link.a.endReason == EndReason::Refused);
	REQUIRE(link.a.endText.find("association refused by the peer (ACSE result rejected-permanent") != std::string::npos);
	REQUIRE(link.a.endText.find(Refused14) != std::string::npos);
	//the diagnostic carries nothing more: our own log has the detail
	REQUIRE(link.a.endText.find("RequireAuthentication") == std::string::npos);
}

TEST_CASE(SUITE("The responder lets an unauthenticated request in when RequireAuthentication is false"))
{
	auto& pki = GetPki();
	Clocks clocks;
	Link link(Setup(clocks, nullptr, Node(pki.b, pki.caFile, [](mace::MaceConf& c) { c.requireAuthentication = false; })));
	link.Start();
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE(link.b.conn.IsAssociated());
	REQUIRE(link.b.conn.GetMaceStatus().state == mace::State::None);
	REQUIRE(link.b.CountLogs("accepted, because \"RequireAuthentication\" is false", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("Authentications that are refused, with the diagnostic the initiator sees"))
{
	auto& pki = GetPki();
	struct Case
	{
		const char* name;
		const macetest::Files* initiator;
		const std::string* initiatorCa; //the CA the initiator trusts (the responder's certificate must pass)
		int64_t initiatorClockOffset;
		const char* diagnostic;
		const char* responderLog;
	};
	const Case cases[] = {
		{"certificate of an untrusted CA", &pki.untrustedF, &pki.caFile, 0, Refused13, "is not acceptable"},
		{"expired certificate", &pki.expiredF, &pki.caFile, 0, Refused13, "certificate has expired"},
		{"certificate not yet valid", &pki.notYetF, &pki.caFile, 0, Refused13, "not yet valid"},
		{"signing time too late", &pki.a, &pki.caFile, 700, Refused13, "signing time outside the window (the peer's clock differs by -700 s"},
		{"signing time too early", &pki.a, &pki.caFile, -700, Refused13, "signing time outside the window (the peer's clock differs by 700 s"},
	};
	for(const Case& c : cases)
	{
		INFO(c.name);
		Clocks clocks;
		clocks.a += c.initiatorClockOffset;
		auto responder = Node(pki.b, pki.caFile);
		auto initiator = Node(*c.initiator, *c.initiatorCa);
		Link link(Setup(clocks, initiator, responder));
		link.Start();
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE_FALSE(link.b.conn.IsAssociated());
		REQUIRE(link.b.endReason == EndReason::Refused);
		REQUIRE(link.b.CountLogs(c.responderLog, LogLevel::Warn) == 1);
		REQUIRE(link.b.conn.GetMaceStatus().state == mace::State::Failed);
		REQUIRE(link.b.conn.GetMaceStatus().lastError.find(c.responderLog) != std::string::npos);
		REQUIRE(link.a.endReason == EndReason::Refused);
		REQUIRE(link.a.endText.find(c.diagnostic) != std::string::npos);
		REQUIRE(link.a.transportClosed);
		REQUIRE(link.b.transportClosed);
	}
}

TEST_CASE(SUITE("An invalid signature in the association request is refused"))
{
	auto& pki = GetPki();
	Clocks clocks;
	Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
	ToAarq(link);
	SpoilTime(link.a.out.back(), clocks.a);
	link.Pump();
	REQUIRE_FALSE(link.b.conn.IsAssociated());
	REQUIRE(link.b.endReason == EndReason::Refused);
	REQUIRE(link.b.CountLogs("the peer's signature is not valid for its certificate \"CN=mace-a.test\"", LogLevel::Warn) == 1);
	REQUIRE(link.a.endText.find(Refused13) != std::string::npos);
}

TEST_CASE(SUITE("MACE off on the responder: the fields are ignored with one warning; the initiator then wants authentication back"))
{
	auto& pki = GetPki();
	{ //RequireResponderAuthentication true (the default): the initiator aborts when the response has none
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), nullptr));
		link.Start();
		REQUIRE(link.b.CountLogs("authentication fields, which are ignored because MACE is not enabled", LogLevel::Warn) == 1);
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE(link.a.endReason == EndReason::AbortedLocal);
		REQUIRE(link.a.endText.find("the association response carries no authentication") != std::string::npos);
		REQUIRE(link.a.CountLogs("RequireResponderAuthentication", LogLevel::Error) == 1);
		REQUIRE(link.a.conn.GetMaceStatus().state == mace::State::Failed);
		REQUIRE(link.b.endReason == EndReason::AbortedByPeer); //(an ACSE abort, as a real peer does it)
		REQUIRE(link.a.transportClosed);
		REQUIRE(link.b.transportClosed);
	}
	{ //false: the association stands, unauthenticated in that direction
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile, [](mace::MaceConf& c) { c.requireResponderAuthentication = false; }), nullptr));
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		REQUIRE(link.b.conn.IsAssociated());
		REQUIRE(link.a.conn.GetMaceStatus().state == mace::State::Sent);
		REQUIRE(link.a.CountLogs("the peer did not authenticate in its association response", LogLevel::Warn) == 1);
		REQUIRE(link.b.CountLogs("which are ignored because MACE is not enabled", LogLevel::Warn) == 1);
	}
}

TEST_CASE(SUITE("The initiator checks the responder's authentication"))
{
	auto& pki = GetPki();
	{ //an invalid signature in the response
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		ToAarq(link);
		link.PumpOnly(link.a); //the responder verifies and answers
		REQUIRE_FALSE(link.b.out.empty());
		SpoilTime(link.b.out.back(), clocks.b);
		link.Pump();
		REQUIRE(link.b.conn.IsAssociated() == false);
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE(link.a.endReason == EndReason::AbortedLocal);
		REQUIRE(link.a.CountLogs("the peer's signature is not valid for its certificate \"CN=mace-b.test\"", LogLevel::Error) == 1);
		REQUIRE(link.a.conn.GetMaceStatus().state == mace::State::Failed);
		REQUIRE(link.b.endReason == EndReason::AbortedByPeer);
	}
	{ //a responder whose clock is off when it signs its response (it accepted the request, so its clock fitted then)
		Clocks clocks;
		const auto base = Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile));
		Link link([&](Tweak& ta, Tweak& tb)
			{
				base(ta, tb);
				tb.config.utcClock = [&clocks, calls = std::make_shared<int>(0)] { return clocks.b + ((*calls)++ == 0 ? 0 : 900); };
			});
		link.Start();
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE(link.a.CountLogs("signing time outside the window (the peer's clock differs by -900 s", LogLevel::Error) == 1);
		REQUIRE(link.b.maceStates.front().state == mace::State::Verified);
	}
	{ //the initiator does not trust the responder's CA
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.otherCaFile), Node(pki.b, pki.caFile)));
		link.Start();
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE(link.a.CountLogs("is not acceptable", LogLevel::Error) == 1);
	}
	{ //an unauthenticated responder is fine when not required, and an invalid one is still an abort
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile, [](mace::MaceConf& c) { c.requireResponderAuthentication = false; }), Node(pki.b, pki.caFile)));
		ToAarq(link);
		link.PumpOnly(link.a);
		SpoilTime(link.b.out.back(), clocks.b);
		link.Pump();
		REQUIRE_FALSE(link.a.conn.IsAssociated());
		REQUIRE(link.a.endReason == EndReason::AbortedLocal);
	}
}

TEST_CASE(SUITE("A responder that is MACE-enabled authenticates only a request that authenticated"))
{
	auto& pki = GetPki();
	//an initiator without MACE and a responder that does not require it: no authentication in the response
	Clocks clocks;
	Link link(Setup(clocks, nullptr, Node(pki.b, pki.caFile, [](mace::MaceConf& c) { c.requireAuthentication = false; })));
	link.Start();
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE(link.b.CountLogs("MACE: authenticating with certificate") == 0);
}

namespace
{
std::function<void(mace::MaceConf&)> WithMechanism(const acse::Oid& oid)
{
	return [oid](mace::MaceConf& c)
		 {
			 c.mechanismName = oid;
			 c.mechanismText = mace::OidToString(oid);
		 };
}
}

TEST_CASE(SUITE("The official and the legacy mechanism-names: tolerated, one warning per association for a non-official one, the response echoes"))
{
	auto& pki = GetPki();
	const std::string official = mace::OfficialMechanismText;
	struct Case
	{
		const char* name;
		acse::Oid initiator;
		acse::Oid responder;
		bool responderWarns; //the initiator's mechanism-name is not the official one
		bool responderAlsoDiffers;
		bool responderInfo;  //official, but not ours
		bool initiatorWarns; //the response's mechanism-name is not the official one
		acse::Oid echoed;
	};
	const Case cases[] = {
		{"both official", mace::OfficialMechanismName, mace::OfficialMechanismName, false, false, false, false, mace::OfficialMechanismName},
		{"initiator legacy, responder official", mace::LegacyMechanismName, mace::OfficialMechanismName, true, true, false, true, mace::LegacyMechanismName},
		{"initiator official, responder legacy", mace::OfficialMechanismName, mace::LegacyMechanismName, false, false, true, false, mace::OfficialMechanismName},
		{"both legacy", mace::LegacyMechanismName, mace::LegacyMechanismName, true, false, false, true, mace::LegacyMechanismName},
	};
	for(const Case& c : cases)
	{
		INFO(c.name);
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile, WithMechanism(c.initiator)), Node(pki.b, pki.caFile, WithMechanism(c.responder))));
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
		REQUIRE(link.b.conn.IsAssociated());
		REQUIRE(link.a.conn.GetMaceStatus().state == mace::State::Verified);
		REQUIRE(link.b.conn.GetMaceStatus().state == mace::State::Verified);

		const std::string notOfficial = "is not the one IEC 62351-4 defines (" + official + ")";
		const std::string initiatorValue = "the peer's mechanism-name " + mace::OidToString(c.initiator) + " " + notOfficial;
		const std::string echoedValue = "the peer's mechanism-name " + mace::OidToString(c.echoed) + " " + notOfficial;
		REQUIRE(link.b.CountLogs(initiatorValue, LogLevel::Warn) == (c.responderWarns ? 1u : 0u));
		REQUIRE(link.b.CountLogs("MACE: the peer's mechanism-name", LogLevel::Warn) == (c.responderWarns ? 1u : 0u));
		REQUIRE(link.b.CountLogs("it also differs from ours " + mace::OidToString(c.responder)) == (c.responderAlsoDiffers ? 1u : 0u));
		REQUIRE(link.b.CountLogs("differs from ours " + mace::OidToString(c.responder) + ": accepted, as IEC 62351-4 asks implementations to be tolerant", LogLevel::Info) == (c.responderInfo ? 1u : 0u));
		REQUIRE(link.b.CountLogs("the legacy value of IEC TS 62351-4:2007") == (c.responderWarns && c.initiator == mace::LegacyMechanismName ? 1u : 0u));
		REQUIRE(link.b.CountLogs("accepted, because the standard asks implementations to be tolerant of it") == (c.responderWarns ? 1u : 0u));
		//the initiator gets back what the responder received
		REQUIRE(link.a.CountLogs("MACE: the peer's mechanism-name", LogLevel::Warn) == (c.initiatorWarns ? 1u : 0u));
		REQUIRE(link.a.CountLogs(echoedValue, LogLevel::Warn) == (c.initiatorWarns ? 1u : 0u));
		REQUIRE(link.a.CountLogs("differs from ours") == 0);

		//and that is what is on the wire in the response
		REQUIRE(link.b.sent.size() >= 2);
		AcceptParts accept;
		DecodeAccept(link.b.sent[1], link.b.conn, accept);
		REQUIRE(accept.aare.authentication.mechanismName == c.echoed);
	}
}

TEST_CASE(SUITE("The initiator tolerates a different mechanism-name in the response, with one warning"))
{
	auto& pki = GetPki();
	Clocks clocks;
	Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
	ToAarq(link);
	link.PumpOnly(link.a); //the responder answers, with the mechanism-name it received: 1.3.9999.7.1
	REQUIRE_FALSE(link.b.out.empty());
	Replace(link.b.out.back(), {0x89, 0x05, 0x2B, 0xCE, 0x0F, 0x07, 0x01}, {0x89, 0x05, 0x2B, 0xCE, 0x0F, 0x07, 0x02});
	link.Pump();
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE(link.a.conn.GetMaceStatus().state == mace::State::Verified);
	REQUIRE(link.a.CountLogs("MACE: the peer's mechanism-name 1.3.9999.7.2 is not the one IEC 62351-4 defines (1.0.62351.4.3.1); it also differs from ours 1.3.9999.7.1: accepted", LogLevel::Warn) == 1);
	REQUIRE(link.a.CountLogs("MACE: the peer's mechanism-name", LogLevel::Warn) == 1);
}

TEST_CASE(SUITE("An association request without a mechanism-name is refused with authentication-mechanism-name-required"))
{
	auto& pki = GetPki();
	Clocks clocks;
	Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
	ToAarq(link);
	//the mechanism-name [11] becomes the (unknown, skipped) element [13]: it is absent for the decoder
	Replace(link.a.out.back(), {0x8B, 0x05, 0x2B, 0xCE, 0x0F, 0x07, 0x01}, {0x8D, 0x05, 0x2B, 0xCE, 0x0F, 0x07, 0x01});
	link.Pump();
	REQUIRE_FALSE(link.a.conn.IsAssociated());
	REQUIRE_FALSE(link.b.conn.IsAssociated());
	REQUIRE(link.b.endReason == EndReason::Refused);
	REQUIRE(link.b.CountLogs("the peer sent an authentication value without a mechanism-name", LogLevel::Warn) == 1);
	REQUIRE(link.a.endText.find("diagnostic from the ACSE service user: authentication-mechanism-name-required (12)") != std::string::npos);
}

TEST_CASE(SUITE("The A-security abstract syntax is accepted when offered, and an unknown one is not"))
{
	auto& pki = GetPki();
	const presentation::ContextDefinition aSecurity{5, presentation::MmsAuthenticationAbstractSyntax, {presentation::BerTransferSyntax}
	};
	const presentation::ContextDefinition unknown{5, {1, 3, 9999, 1, 1}, {presentation::BerTransferSyntax}
	};
	REQUIRE(presentation::MmsAuthenticationAbstractSyntax == presentation::Oid({1, 0, 840, 0, 1, 1, 4, 1}));

	{ //a MACE responder: acceptance with BER, and the identification of that context in its authentication value
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		OfferContext(link, aSecurity);
		AcceptParts accept;
		DecodeAccept(link.b.out.back(), link.b.conn, accept);
		REQUIRE(accept.aare.result == acse::Result::Accepted);
		REQUIRE(accept.cpa.results.size() == 3);
		REQUIRE(accept.cpa.results[0].result == presentation::Result::Acceptance);
		REQUIRE(accept.cpa.results[1].result == presentation::Result::Acceptance);
		REQUIRE(accept.cpa.results[2].result == presentation::Result::Acceptance);
		REQUIRE(accept.cpa.results[2].transferSyntax == presentation::BerTransferSyntax);
		REQUIRE_FALSE(accept.cpa.results[2].providerReason.has_value());
		REQUIRE(link.b.conn.GetMaceStatus().state == mace::State::Verified);
		const ber::Bytes& value = *accept.aare.authentication.authenticationValue;
		const ber::Element outer = ber::ParseElement(value.data(), value.size());
		REQUIRE(ber::Bytes(outer.content, outer.content + 3) == ber::Bytes({0x02, 0x01, 0x05}));
		REQUIRE_NOTHROW(acse::DecodeMaceAuthValue(value)); //(and the decoder reads it)
	}
	{ //an abstract syntax nobody knows is still refused for that context alone, and the association goes on
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		OfferContext(link, unknown);
		AcceptParts accept;
		DecodeAccept(link.b.out.back(), link.b.conn, accept);
		REQUIRE(accept.aare.result == acse::Result::Accepted);
		REQUIRE(accept.cpa.results.size() == 3);
		REQUIRE(accept.cpa.results[2].result == presentation::Result::ProviderRejection);
		REQUIRE(accept.cpa.results[2].providerReason == presentation::ContextReason::AbstractSyntaxNotSupported);
		const ber::Bytes& value = *accept.aare.authentication.authenticationValue;
		const ber::Element outer = ber::ParseElement(value.data(), value.size());
		REQUIRE(outer.content[0] != 0x02); //no identification
	}
	{ //a responder without MACE does not support it: as before, rejected for that context
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), nullptr));
		OfferContext(link, aSecurity);
		AcceptParts accept;
		DecodeAccept(link.b.out.back(), link.b.conn, accept);
		REQUIRE(accept.aare.result == acse::Result::Accepted);
		REQUIRE(accept.cpa.results.size() == 3);
		REQUIRE(accept.cpa.results[2].result == presentation::Result::ProviderRejection);
		REQUIRE_FALSE(accept.aare.authentication.authenticationValue.has_value());
	}
	{ //our own requests do not offer it
		Clocks clocks;
		Link link(Setup(clocks, Node(pki.a, pki.caFile), Node(pki.b, pki.caFile)));
		ToAarq(link);
		const ber::Bytes tsdu = TsduOfFrames(link.a.out.back(), link.a.conn);
		const session::Spdu s = session::Decode(tsdu);
		const presentation::ConnectRequest cp = presentation::DecodeConnectRequest(s.userData, s.userDataLength);
		REQUIRE(cp.contexts.size() == 2);
	}
}

#endif // ODC_ASIO_SSL
