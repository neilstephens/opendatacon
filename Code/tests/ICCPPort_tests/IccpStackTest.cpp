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
 * IccpStackTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the association state machine: two Connection instances wired together in memory,
 *  no sockets, with a fake clock.
 */

#include "IccpStack.h"
#include "StackTestHelpers.h"
#include <catch.hpp>

#define SUITE(name) "ICCP stack - " name

using namespace iccp::stack;
using iccp::stacktest::Bytes;
using iccp::stacktest::Endpoint;
using iccp::stacktest::Link;
using iccp::stacktest::Tweak;
namespace mms = iccp::mms;

TEST_CASE(SUITE("Association, negotiation and addressing"))
{
	Link link;
	REQUIRE(link.a.conn.GetState() == State::Idle);
	link.Start();
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE(link.b.conn.IsAssociated());
	REQUIRE(link.a.associated);
	REQUIRE(link.b.associated);

	//negotiated values, seen from each end
	const Negotiated& na = link.a.conn.GetNegotiated();
	const Negotiated& nb = link.b.conn.GetNegotiated();
	REQUIRE(na.tpduSizeCode == iccp::cotp::TpduSize1024);
	REQUIRE(nb.tpduSizeCode == iccp::cotp::TpduSize1024);
	REQUIRE(na.acseContextId == 1);
	REQUIRE(na.mmsContextId == 3);
	REQUIRE(nb.mmsContextId == 3);
	REQUIRE(na.maxOutstandingLocal == 10);
	REQUIRE(na.maxOutstandingPeer == 10);
	REQUIRE(nb.maxOutstandingLocal == 10);
	REQUIRE(na.nestingLevel == 4);
	REQUIRE(na.peerServices == DefaultServicesSupported());
	REQUIRE(nb.peerServices == DefaultServicesSupported());

	//what the responder was told
	REQUIRE(link.b.lastAssociate.has_value());
	REQUIRE(link.b.lastAssociate->callingApTitle == link.a.conn.GetConfig().local.apTitle);
	REQUIRE(link.b.lastAssociate->callingAeQualifier == 7);
	REQUIRE(link.b.lastAssociate->calledApTitle == link.a.conn.GetConfig().remote.apTitle);
	REQUIRE(link.b.lastAssociate->calledAeQualifier == 9);
	REQUIRE(link.b.lastAssociate->callingSSelector == Bytes({0x00, 0x11}));
	REQUIRE(link.b.lastAssociate->calledPSelector == Bytes({0x00, 0x00, 0x00, 0x22}));
	REQUIRE(link.b.lastAssociate->initiate.proposedMaxServOutstandingCalling == 10);

	//the Initiate negotiation takes the smaller limits
	Link small([](Tweak&, Tweak& b)
		{
			b.config.maxOutstandingCalling = 3;
			b.config.maxOutstandingCalled = 2;
			b.config.nestingLevel = 2;
		});
	small.Start();
	REQUIRE(small.a.conn.IsAssociated());
	REQUIRE(small.a.conn.GetNegotiated().maxOutstandingLocal == 3);
	REQUIRE(small.a.conn.GetNegotiated().maxOutstandingPeer == 2);
	REQUIRE(small.b.conn.GetNegotiated().maxOutstandingLocal == 2);
	REQUIRE(small.b.conn.GetNegotiated().maxOutstandingPeer == 3);
	REQUIRE(small.a.conn.GetNegotiated().nestingLevel == 2);
}

TEST_CASE(SUITE("Smaller TPDU sizes are negotiated"))
{
	Link link([](Tweak& a, Tweak& b)
		{
			a.config.tpduSizeCode = iccp::cotp::TpduSize4096;
			b.config.tpduSizeCode = iccp::cotp::TpduSize512;
		});
	link.Start();
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE(link.a.conn.GetNegotiated().tpduSizeCode == iccp::cotp::TpduSize512);
	REQUIRE(link.b.conn.GetNegotiated().tpduSizeCode == iccp::cotp::TpduSize512);
}

TEST_CASE(SUITE("Requests in both directions at once, with the outstanding limit"))
{
	Link link([](Tweak& a, Tweak& b)
		{
			a.config.maxOutstandingCalling = 2;
			b.config.maxOutstandingCalling = 2;
			a.config.maxOutstandingCalled = 2;
			b.config.maxOutstandingCalled = 2;
		});
	link.b.autoRespond = false; //hold the answers so that requests pile up
	link.a.autoRespond = false;
	link.Start();
	REQUIRE(link.a.conn.GetNegotiated().maxOutstandingLocal == 2);

	std::vector<int> aDone, bDone;
	for(int i = 0; i < 5; ++i)
	{
		link.a.conn.SendRequest(mms::IdentifyRequest{}, [&aDone, i](const Outcome& o) { if(o.Ok()) aDone.push_back(i);});
		link.b.conn.SendRequest(mms::IdentifyRequest{}, [&bDone, i](const Outcome& o) { if(o.Ok()) bDone.push_back(i);});
	}
	link.Pump();
	//two on the wire each way, three queued
	REQUIRE(link.a.conn.OutstandingRequests() == 2);
	REQUIRE(link.a.conn.QueuedRequests() == 3);
	REQUIRE(link.b.conn.OutstandingRequests() == 2);
	REQUIRE(link.b.incomingRequests.size() == 2);
	REQUIRE(link.a.incomingRequests.size() == 2);

	//answer as they arrive
	for(int round = 0; round < 10; ++round)
	{
		link.AnswerAll();
		link.Pump();
	}
	REQUIRE(aDone == std::vector<int>({0, 1, 2, 3, 4}));
	REQUIRE(bDone == std::vector<int>({0, 1, 2, 3, 4}));
	REQUIRE(link.a.conn.OutstandingRequests() == 0);
	REQUIRE(link.a.conn.QueuedRequests() == 0);
	REQUIRE(link.a.maxSeenIncoming <= 2);
	REQUIRE(link.b.maxSeenIncoming <= 2);
}

TEST_CASE(SUITE("Service errors, rejects, and requests the handler cannot answer"))
{
	Link link;
	link.Start();

	//a service error
	link.b.errorForNext = true;
	Outcome::Kind kind = Outcome::Kind::Response;
	mms::ServiceError seen;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; seen = o.error; });
	link.Pump();
	REQUIRE(kind == Outcome::Kind::Error);
	REQUIRE(seen.errorClass == mms::ErrorClass::Definition);
	REQUIRE(seen.errorCode == 1);

	//the handler rejects
	link.b.rejectForNext = true;
	kind = Outcome::Kind::Response;
	mms::Reject rj;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; rj = o.reject; });
	link.Pump();
	REQUIRE(kind == Outcome::Kind::Rejected);
	REQUIRE(rj.reason == mms::RejectReason::UnrecognizedService);

	//no request handler at all: every request is rejected, not ignored
	Link none([](Tweak&, Tweak& b) { b.noRequestHandler = true; });
	none.Start();
	kind = Outcome::Kind::Response;
	none.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; });
	none.Pump();
	REQUIRE(kind == Outcome::Kind::Rejected);

	//answering an id that is not outstanding
	REQUIRE_FALSE(link.b.conn.Respond(999, mms::IdentifyResponse{}));
	REQUIRE_FALSE(link.b.conn.RespondError(999, mms::ServiceError{}));
	REQUIRE_FALSE(link.b.conn.RejectRequest(999));
}

TEST_CASE(SUITE("Wire level: unsupported and malformed MMS requests, duplicate ids, limits, unknown responses"))
{
	Link link;
	link.Start();
	const auto frame = [&link](const Bytes& mmsPdu) { return iccp::stacktest::FrameMms(mmsPdu, link.a.conn.GetNegotiated()); };

	//an unsupported service (Status): rejected, with the id
	link.b.sent.clear();
	link.a.out.clear();
	link.b.conn.OnData(frame({0xa0, 0x06, 0x02, 0x02, 0x12, 0x34, 0x80, 0x00}).data(), frame({0xa0, 0x06, 0x02, 0x02, 0x12, 0x34, 0x80, 0x00}).size());
	REQUIRE(link.b.conn.IsAssociated());
	REQUIRE(link.b.out.size() == 1);
	const auto reply = iccp::stacktest::DecodeMms(link.b.out.front(), link.b.conn.GetNegotiated());
	const auto* rj = std::get_if<mms::Reject>(&reply);
	REQUIRE(rj != nullptr);
	REQUIRE(rj->originalInvokeId == 0x1234u);
	REQUIRE(rj->reason == mms::RejectReason::UnrecognizedService);
	link.b.out.clear();

	//a malformed request body: rejected with invalid-argument
	const Bytes bad = frame({0xa0, 0x07, 0x02, 0x02, 0x12, 0x35, 0xa1, 0x01, 0xff});
	link.b.conn.OnData(bad.data(), bad.size());
	REQUIRE(link.b.conn.IsAssociated());
	const auto reply2 = iccp::stacktest::DecodeMms(link.b.out.front(), link.b.conn.GetNegotiated());
	REQUIRE(std::get<mms::Reject>(reply2).reason == mms::RejectReason::InvalidArgument);
	link.b.out.clear();

	//a response to a request we never made is ignored
	const Bytes unknown = iccp::stacktest::FrameMms(mms::Encode(mms::ConfirmedResponse{0x5555, mms::IdentifyResponse{"V", "M", "R", std::nullopt}
		}), link.a.conn.GetNegotiated());
	link.b.conn.OnData(unknown.data(), unknown.size());
	REQUIRE(link.b.conn.IsAssociated());
	REQUIRE(link.b.out.empty());

	//duplicate invoke id: hold an identify, send the same id again
	link.b.autoRespond = false;
	const Bytes identify = frame({0xa0, 0x06, 0x02, 0x02, 0x00, 0x20, 0x82, 0x00});
	link.b.conn.OnData(identify.data(), identify.size());
	REQUIRE(link.b.incomingRequests.size() == 1);
	link.b.conn.OnData(identify.data(), identify.size());
	REQUIRE(link.b.incomingRequests.size() == 1);
	REQUIRE(std::get<mms::Reject>(iccp::stacktest::DecodeMms(link.b.out.front(), link.b.conn.GetNegotiated())).reason == mms::RejectReason::InvalidInvokeId);
	link.b.out.clear();

	//beyond the peer's limit
	for(int id = 0x21; id < 0x21 + 12; ++id)
	{
		const Bytes more = frame({0xa0, 0x06, 0x02, 0x02, 0x00, static_cast<uint8_t>(id), 0x82, 0x00});
		link.b.conn.OnData(more.data(), more.size());
	}
	REQUIRE(link.b.incomingRequests.size() == 10);
	REQUIRE(link.b.conn.IsAssociated());
	REQUIRE(link.b.out.size() == 3);
	REQUIRE(std::get<mms::Reject>(iccp::stacktest::DecodeMms(link.b.out.back(), link.b.conn.GetNegotiated())).reason == mms::RejectReason::MaxServOutstandingExceeded);

	//a malformed response to a request of ours completes it as Malformed, the association survives
	Link l2;
	l2.Start();
	Outcome::Kind kind = Outcome::Kind::Response;
	l2.a.autoRespond = false;
	l2.b.autoRespond = false;
	l2.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; });
	l2.Pump();
	REQUIRE(l2.b.incomingRequests.size() == 1);
	const uint32_t id = l2.b.incomingRequests.front();
	const Bytes malformed = iccp::stacktest::FrameMms({0xa1, 0x07, 0x02, 0x02, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id), 0xa2, 0x01, 0xff}, l2.a.conn.GetNegotiated());
	l2.a.conn.OnData(malformed.data(), malformed.size());
	REQUIRE(kind == Outcome::Kind::Malformed);
	REQUIRE(l2.a.conn.IsAssociated());

	//an Initiate in the data phase is a protocol error
	Link l3;
	l3.Start();
	const Bytes init = iccp::stacktest::FrameMms(mms::Encode(mms::InitiateRequest{std::nullopt, 1, 1, std::nullopt, 1, mms::MakeBits({0}, 1), mms::MakeBits({0}, 1), std::nullopt, std::nullopt, std::nullopt}), l3.a.conn.GetNegotiated());
	l3.b.conn.OnData(init.data(), init.size());
	REQUIRE(l3.b.conn.GetState() == State::Closed);
	REQUIRE(l3.b.endReason == EndReason::AbortedLocal);
}

TEST_CASE(SUITE("Unconfirmed information reports, both ways"))
{
	Link link;
	link.Start();
	mms::InformationReport r;
	r.specification = mms::ObjectName::Domain("DOMAIN_A", "DS_TEST_001");
	r.results = {mms::AccessResult::Success(mms::Data::Integer(5)), mms::AccessResult::Failure(mms::DataAccessError::HardwareFault)};
	REQUIRE(link.a.conn.SendReport(r));
	REQUIRE(link.b.conn.SendReport(r));
	link.Pump();
	REQUIRE(link.b.reports.size() == 1);
	REQUIRE(link.b.reports[0] == r);
	REQUIRE(link.a.reports.size() == 1);

	//not while closed
	link.a.conn.Abort();
	REQUIRE_FALSE(link.a.conn.SendReport(r));
}

TEST_CASE(SUITE("No reports once the peer has concluded"))
{
	Link link;
	link.Start();
	mms::InformationReport r;
	r.specification = mms::ObjectName::Domain("DOMAIN_A", "DS_TEST_001");
	r.results = {mms::AccessResult::Success(mms::Data::Integer(5))};
	REQUIRE(link.b.conn.SendReport(r));
	link.a.conn.Conclude();
	link.PumpOnly(link.a); //only the Conclude reaches b: it answers, the release is still to come
	REQUIRE(link.b.conn.GetState() == State::Associated);
	REQUIRE_FALSE(link.b.conn.SendReport(r));
	link.Pump();
	REQUIRE(link.b.conn.GetState() == State::Closed);
}

TEST_CASE(SUITE("Large messages are segmented and reassembled, however the bytes arrive"))
{
	for(size_t chunk : {size_t(0), size_t(1), size_t(7), size_t(333)})
	{
		INFO("chunk " << chunk);
		Link link([](Tweak& a, Tweak& b)
			{
				a.config.tpduSizeCode = iccp::cotp::TpduSize128;
				b.config.tpduSizeCode = iccp::cotp::TpduSize128;
			});
		link.chunk = chunk;
		link.Start();
		REQUIRE(link.a.conn.GetNegotiated().tpduSizeCode == iccp::cotp::TpduSize128);

		link.b.autoRespond = false;
		mms::GetNameListRequest req;
		req.scope = mms::ObjectScope::DomainSpecific;
		req.domainId = "DOMAIN_A";
		bool done = false;
		size_t count = 0;
		link.a.conn.SendRequest(req, [&](const Outcome& o)
			{
				if(const auto* r = o.Get<mms::GetNameListResponse>())
				{
					done = true;
					count = r->identifiers.size();
				}
			});
		link.Pump();
		REQUIRE(link.b.incomingRequests.size() == 1);
		mms::GetNameListResponse resp;
		for(size_t i = 0; i < 2000; ++i)
		{
			char name[32];
			snprintf(name, sizeof(name), "POINT_%04zu_MW", i);
			resp.identifiers.push_back(name);
		}
		link.b.sent.clear();
		REQUIRE(link.b.conn.Respond(link.b.incomingRequests.front(), resp));
		link.Pump();
		REQUIRE(done);
		REQUIRE(count == 2000);
		REQUIRE(link.a.conn.IsAssociated());
		//30000 octets in 123 octet TPDU payloads
		REQUIRE(link.b.bytesSent > 30000);
	}
}

TEST_CASE(SUITE("Release: the side that concludes"))
{
	for(bool initiatorConcludes : {true, false})
	{
		Link link;
		link.Start();
		Endpoint& closer = initiatorConcludes ? link.a : link.b;
		Endpoint& other = initiatorConcludes ? link.b : link.a;
		closer.conn.Conclude();
		link.Pump();
		REQUIRE(closer.conn.GetState() == State::Closed);
		REQUIRE(other.conn.GetState() == State::Closed);
		REQUIRE(closer.endReason == EndReason::Released);
		REQUIRE(other.endReason == EndReason::ReleasedByPeer);
		REQUIRE(closer.transportClosed);
		REQUIRE(other.transportClosed);
		REQUIRE(closer.closedCount == 1);
		REQUIRE(other.closedCount == 1);
	}
}

TEST_CASE(SUITE("Release: both ends conclude at the same time"))
{
	//e.g. an opendatacon shutdown disables the client and server ports together. The association initiator's Conclude
	//goes through, the responder gives way, and nobody has to wait for a timeout.
	for(bool aFirst : {true, false})
	{
		Link link;
		link.Start();
		if(aFirst)
		{
			link.a.conn.Conclude();
			link.b.conn.Conclude();
		}
		else
		{
			link.b.conn.Conclude();
			link.a.conn.Conclude();
		}
		link.Pump();
		REQUIRE(link.a.conn.GetState() == State::Closed);
		REQUIRE(link.b.conn.GetState() == State::Closed);
		REQUIRE(link.a.endReason == EndReason::Released);
		REQUIRE(link.b.endReason == EndReason::ReleasedByPeer);
		REQUIRE(link.a.closedCount == 1);
		REQUIRE(link.b.closedCount == 1);
		REQUIRE(link.a.transportClosed);
		REQUIRE(link.b.transportClosed);
	}
}

TEST_CASE(SUITE("Release: outstanding requests are failed when the association ends"))
{
	Link link;
	link.b.autoRespond = false;
	link.Start();
	std::vector<Outcome::Kind> kinds;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kinds.push_back(o.kind); });
	link.Pump();
	link.a.conn.Conclude();
	link.Pump();
	REQUIRE(link.a.conn.GetState() == State::Closed);
	REQUIRE(kinds == std::vector<Outcome::Kind>({Outcome::Kind::Aborted}));
	//requests after the end are refused immediately
	Outcome::Kind late = Outcome::Kind::Response;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { late = o.kind; });
	REQUIRE(late == Outcome::Kind::Aborted);
}

TEST_CASE(SUITE("Release: a peer that finishes at the session level only, or just closes after the Conclude"))
{
	//the peer concludes, answers our release, then drops
	Link link;
	link.Start();
	link.a.conn.Conclude();
	//deliver the Conclude, let b answer; but never let a see b's FINISH handling: simply close b's transport
	link.PumpOnly(link.a);
	link.b.conn.OnTransportClosed();
	REQUIRE(link.b.conn.GetState() == State::Closed);

	//the transport closing while we wait for the release completes it (the Conclude was answered)
	Link link2;
	link2.Start();
	link2.a.conn.Conclude();
	link2.PumpOnly(link2.a); //b gets the Conclude request and answers it (a's bytes only)
	link2.PumpOnly(link2.b); //a gets the Conclude response and sends FINISH
	REQUIRE(link2.a.conn.GetState() == State::Releasing);
	link2.a.conn.OnTransportClosed();
	REQUIRE(link2.a.endReason == EndReason::Released);
}

TEST_CASE(SUITE("Abort"))
{
	for(bool initiator : {true, false})
	{
		Link link;
		link.Start();
		Endpoint& aborter = initiator ? link.a : link.b;
		Endpoint& other = initiator ? link.b : link.a;
		std::vector<Outcome::Kind> kinds;
		other.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kinds.push_back(o.kind); });
		aborter.autoRespond = false;
		aborter.conn.Abort("test abort");
		link.Pump();
		REQUIRE(aborter.endReason == EndReason::AbortedLocal);
		REQUIRE(other.endReason == EndReason::AbortedByPeer);
		REQUIRE(aborter.transportClosed);
		REQUIRE(other.transportClosed);
		REQUIRE(other.conn.GetState() == State::Closed);
	}
}

TEST_CASE(SUITE("Association refusal"))
{
	Link link;
	link.b.acceptAssociation = false;
	link.Start();
	REQUIRE(link.a.conn.GetState() == State::Closed);
	REQUIRE(link.b.conn.GetState() == State::Closed);
	REQUIRE(link.a.endReason == EndReason::Refused);
	REQUIRE(link.b.endReason == EndReason::Refused);
	REQUIRE_FALSE(link.a.associated);
	REQUIRE_FALSE(link.b.associated);

	//the initiator says why it was refused
	REQUIRE(link.a.endText.find("rejected-permanent") != std::string::npos);
	REQUIRE(link.a.endText.find("calling-AP-title-not-recognized (3)") != std::string::npos);

	//requests queued before the association completes fail
	Link l2;
	l2.b.acceptAssociation = false;
	Outcome::Kind kind = Outcome::Kind::Response;
	l2.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; });
	REQUIRE(kind == Outcome::Kind::Aborted);
}

TEST_CASE(SUITE("A refusal that carries its presentation data inside the session reason code"))
{
	//the form a real-world stack uses: the REFUSE SPDU has the reason code parameter holding the reason and then
	//the presentation PPDU, and no user data parameter. Take our own responder's refusal and send it in that form.
	using namespace iccp;
	Link link;
	link.b.acceptAssociation = false;
	link.a.conn.Start();
	link.b.conn.Start();
	link.PumpOnly(link.a); //the connection request
	link.PumpOnly(link.b); //the connection confirm
	link.PumpOnly(link.a); //the session connect with the association request: b refuses
	REQUIRE_FALSE(link.b.out.empty());

	tpkt::Reader reader;
	cotp::TsduReassembler re(cotp::TsduReassembler::DefaultMaxTsdu, 0);
	Bytes payload, tsdu;
	for(const Bytes& frames : link.b.out)
		reader.Feed(frames);
	link.b.out.clear();
	while(reader.Next(payload))
	{
		const cotp::Tpdu t = cotp::Decode(payload);
		if(re.Add(t))
			tsdu = re.Take();
	}
	const session::Spdu refuse = session::Decode(tsdu);
	REQUIRE(refuse.type == session::SpduType::Refuse);
	REQUIRE(refuse.hasUserData);

	Bytes reasonParameter{session::RefuseReason::RejectedByCalledUserWithData};
	reasonParameter.insert(reasonParameter.end(), refuse.userData, refuse.userData + refuse.userDataLength);
	REQUIRE(reasonParameter.size() < 0xFF);
	Bytes spdu{static_cast<uint8_t>(session::SpduType::Refuse), 0, 0x14, 0x02, 0x00, 0x02, 0x16, 0x01, 0x02, 0x32, static_cast<uint8_t>(reasonParameter.size())};
	spdu.insert(spdu.end(), reasonParameter.begin(), reasonParameter.end());
	spdu[1] = static_cast<uint8_t>(spdu.size() - 2);
	REQUIRE(session::Decode(spdu).hasUserData);

	Bytes wire;
	for(const Bytes& seg : cotp::Segment(spdu, 10))
	{
		const Bytes f = tpkt::Frame(seg);
		wire.insert(wire.end(), f.begin(), f.end());
	}
	link.a.conn.OnData(wire.data(), wire.size());

	REQUIRE(link.a.conn.GetState() == State::Closed);
	REQUIRE(link.a.endReason == EndReason::Refused);
	REQUIRE(link.a.endText.find("rejected-permanent") != std::string::npos);
	REQUIRE(link.a.endText.find("calling-AP-title-not-recognized (3)") != std::string::npos);
}

TEST_CASE(SUITE("Association failures: no MMS context, wrong application context, no Initiate"))
{
	using namespace iccp;
	for(int variant = 0; variant < 4; ++variant)
	{
		Link link;
		link.b.conn.Start();
		const Bytes cr = tpkt::Frame(cotp::EncodeConnectRequest(1, std::nullopt, std::nullopt, cotp::TpduSize1024));
		link.b.conn.OnData(cr.data(), cr.size());
		REQUIRE(link.b.conn.GetState() == State::AwaitConnect);

		mms::InitiateRequest init;
		init.proposedMaxServOutstandingCalling = 5;
		init.proposedMaxServOutstandingCalled = 5;
		init.proposedParameterCbb = mms::MakeBits({0}, 11);
		init.servicesSupportedCalling = mms::MakeBits({1}, 85);
		const Bytes initBytes = variant == 2 ? mms::Encode(mms::ConcludeRequest{}) : mms::Encode(init);
		acse::Aarq aarq;
		if(variant == 1)
			aarq.applicationContextName = {1, 2, 3}
		;
		if(variant != 3)
			aarq.userInformation.push_back(acse::MakeExternal(3, initBytes));
		const Bytes aarqBytes = acse::Encode(aarq);

		presentation::ConnectRequest cp;
		cp.contexts.push_back({1, presentation::AcseAbstractSyntax, {presentation::BerTransferSyntax}
			});
		if(variant != 0)
			cp.contexts.push_back({3, presentation::MmsAbstractSyntax, {presentation::BerTransferSyntax}
				});
		cp.userData.push_back(presentation::MakePdv(1, aarqBytes));
		const Bytes cpBytes = presentation::Encode(cp);
		session::Spdu cn;
		cn.type = session::SpduType::Connect;
		cn.protocolOptions = 0;
		cn.version = session::Version::Version2;
		cn.requirements = session::Requirement::Duplex;
		cn.hasUserData = true;
		cn.userData = cpBytes.data();
		cn.userDataLength = cpBytes.size();
		const Bytes frame = tpkt::Frame(cotp::Segment(session::Encode(cn), cotp::TpduSize1024).front());
		link.b.conn.OnData(frame.data(), frame.size());
		INFO("variant " << variant);
		REQUIRE(link.b.conn.GetState() == State::Closed);
		REQUIRE(link.b.endReason == EndReason::Refused);
		REQUIRE(link.b.transportClosed);
	}
}

TEST_CASE(SUITE("Timeouts use the clock the owner provides"))
{
	//a request without an answer
	Link link;
	link.Start();
	link.b.autoRespond = false;
	Outcome::Kind kind = Outcome::Kind::Response;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kind = o.kind; });
	link.Pump();
	REQUIRE(link.a.conn.NextDeadline().has_value());
	link.clock.now += std::chrono::milliseconds(4999);
	link.a.conn.Tick();
	REQUIRE(link.a.conn.OutstandingRequests() == 1);
	link.clock.now += std::chrono::milliseconds(2);
	link.a.conn.Tick();
	REQUIRE(kind == Outcome::Kind::Timeout);
	REQUIRE(link.a.conn.OutstandingRequests() == 0);
	REQUIRE(link.a.conn.IsAssociated());
	REQUIRE_FALSE(link.a.conn.NextDeadline().has_value());
	//a late answer is ignored
	link.b.AnswerAll();
	link.Pump();
	REQUIRE(link.a.conn.IsAssociated());

	//a slot is freed by the timeout so queued requests proceed
	Link l2([](Tweak& a, Tweak& b)
		{
			a.config.maxOutstandingCalling = 1;
			b.config.maxOutstandingCalling = 1;
		});
	l2.Start();
	l2.b.autoRespond = false;
	std::vector<Outcome::Kind> kinds;
	l2.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kinds.push_back(o.kind); });
	l2.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kinds.push_back(o.kind); });
	l2.Pump();
	REQUIRE(l2.a.conn.QueuedRequests() == 1);
	l2.b.AnswerAll(); //the peer answers, but too late: the answer is still on the wire when the timeout fires
	l2.clock.now += std::chrono::milliseconds(6000);
	l2.a.conn.Tick();
	l2.Pump();
	REQUIRE(kinds == std::vector<Outcome::Kind>({Outcome::Kind::Timeout}));
	REQUIRE(l2.a.conn.QueuedRequests() == 0);
	REQUIRE(l2.a.conn.OutstandingRequests() == 1);

	//set-up that never completes
	Link l3;
	l3.b.silent = true; //b swallows everything
	l3.Start();
	REQUIRE(l3.a.conn.GetState() == State::TransportPending);
	l3.clock.now += std::chrono::milliseconds(9999);
	l3.a.conn.Tick();
	REQUIRE(l3.a.conn.GetState() == State::TransportPending);
	l3.clock.now += std::chrono::milliseconds(2);
	l3.a.conn.Tick();
	REQUIRE(l3.a.conn.GetState() == State::Closed);
	REQUIRE(l3.a.endReason == EndReason::Timeout);
	REQUIRE(l3.a.transportClosed);

	//a connection that sends nothing, as seen by a responder
	Link l4;
	l4.b.conn.Start();
	l4.clock.now += std::chrono::milliseconds(10001);
	l4.b.conn.Tick();
	REQUIRE(l4.b.endReason == EndReason::Timeout);

	//a release that is never answered
	Link l5;
	l5.Start();
	l5.b.silent = true;
	l5.a.conn.Conclude();
	l5.Pump();
	REQUIRE(l5.a.conn.GetState() == State::Concluding);
	l5.clock.now += std::chrono::milliseconds(5001);
	l5.a.conn.Tick();
	REQUIRE(l5.a.conn.GetState() == State::Closed);
}

TEST_CASE(SUITE("The transport dropping ends the association and fails the requests"))
{
	Link link;
	link.Start();
	link.b.autoRespond = false;
	std::vector<Outcome::Kind> kinds;
	link.a.conn.SendRequest(mms::IdentifyRequest{}, [&](const Outcome& o) { kinds.push_back(o.kind); });
	link.Pump();
	link.a.conn.OnTransportClosed();
	REQUIRE(link.a.endReason == EndReason::TransportLost);
	REQUIRE(kinds == std::vector<Outcome::Kind>({Outcome::Kind::Aborted}));
	REQUIRE(link.a.closedCount == 1);
	link.a.conn.OnTransportClosed();
	REQUIRE(link.a.closedCount == 1);
	//input after the end is ignored
	const uint8_t junk[] = {1, 2, 3};
	link.a.conn.OnData(junk, sizeof(junk));
	REQUIRE(link.a.closedCount == 1);
}

TEST_CASE(SUITE("Garbage in any state aborts cleanly and never throws"))
{
	uint32_t state = 4242;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};
	//garbage at every stage of the association
	for(int stage = 0; stage < 6; ++stage)
	{
		for(int iter = 0; iter < 50; ++iter)
		{
			Link link;
			if(stage == 0)
				link.b.conn.Start();
			if(stage >= 1)
				link.a.conn.Start();
			if(stage >= 2)
				link.PumpSteps(stage - 1);
			Bytes junk(1 + next() % 200);
			for(auto& b : junk)
				b = static_cast<uint8_t>(next());
			if(iter % 3 == 0 && junk.size() > 4)
			{
				junk[0] = 3;
				junk[1] = 0;
				junk[2] = static_cast<uint8_t>((junk.size()) >> 8);
				junk[3] = static_cast<uint8_t>(junk.size());
			}
			Endpoint& victim = (iter % 2 == 0) ? link.b : link.a;
			REQUIRE_NOTHROW(victim.conn.OnData(junk.data(), junk.size()));
			REQUIRE_NOTHROW(victim.conn.Tick());
			REQUIRE(victim.closedCount <= 1);
		}
	}
}

TEST_CASE(SUITE("Mutating a complete association never crashes"))
{
	//record the bytes of a full association with traffic, then replay mutated copies to a fresh responder
	Link rec;
	rec.record = true;
	rec.Start();
	rec.a.conn.SendRequest(mms::IdentifyRequest{}, [](const Outcome&) {});
	mms::ReadRequest rd;
	rd.specification = mms::ListOfVariable{mms::VariableSpec{mms::ObjectName::Vmd("POINT_X"), std::nullopt}
	};
	rec.a.conn.SendRequest(rd, [](const Outcome&) {});
	rec.Pump();
	rec.a.conn.Conclude();
	rec.Pump();
	const Bytes stream = rec.aToB;
	REQUIRE(stream.size() > 100);

	uint32_t state = 99;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};
	for(int iter = 0; iter < 3000; ++iter)
	{
		Bytes m = stream;
		const unsigned changes = 1 + next() % 3;
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		if(next() % 4 == 0)
			m.resize(next() % (m.size() + 1));
		Link victim;
		victim.b.conn.Start();
		for(size_t off = 0; off < m.size(); off += 61)
			REQUIRE_NOTHROW(victim.b.conn.OnData(m.data() + off, std::min<size_t>(61, m.size() - off)));
		REQUIRE_NOTHROW(victim.b.conn.Tick());
		REQUIRE(victim.b.closedCount <= 1);
	}
}
