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
 * StackTestHelpers.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  In-memory wiring of two stack::Connection instances for tests (no sockets, fake clock).
 */

#ifndef ICCP_STACK_TEST_HELPERS_H_
#define ICCP_STACK_TEST_HELPERS_H_

#include "IccpStack.h"
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace iccp
{
namespace stacktest
{

using Bytes = ber::Bytes;

struct FakeClock
{
	stack::Clock::time_point now = stack::Clock::time_point() + std::chrono::hours(1);
};

//Configuration (and construction-time behaviour) of one end, tweakable before the Connection is built
struct Tweak
{
	stack::Config config;
	bool noRequestHandler = false;
};

//the whole TSDU for an MMS PDU, as TPKT frames
inline Bytes FrameMms(const Bytes& mmsPdu, const stack::Negotiated& n)
{
	const Bytes ppdu = presentation::EncodeData(n.mmsContextId, mmsPdu);
	const Bytes tsdu = session::EncodeData(ppdu);
	Bytes out;
	for(const Bytes& seg : cotp::Segment(tsdu, n.tpduSizeCode))
	{
		const Bytes f = tpkt::Frame(seg);
		out.insert(out.end(), f.begin(), f.end());
	}
	return out;
}

//Decode the MMS PDU in the frames of one TSDU
inline mms::Pdu DecodeMms(const Bytes& frames, const stack::Negotiated& n)
{
	tpkt::Reader reader;
	cotp::TsduReassembler re(cotp::TsduReassembler::DefaultMaxTsdu, n.tpduSizeCode);
	reader.Feed(frames);
	Bytes payload, tsdu;
	while(reader.Next(payload))
	{
		const cotp::Tpdu t = cotp::Decode(payload);
		if(re.Add(t))
			tsdu = re.Take();
	}
	const session::Spdu s = session::Decode(tsdu);
	const presentation::Pdv pdv = presentation::DecodeData(s.userData, s.userDataLength);
	if(pdv.contextId != n.mmsContextId)
		throw ber::DecodeError("not an MMS PDU");
	return mms::Decode(pdv.data, pdv.dataLength);
}

struct Endpoint
{
	std::unique_ptr<stack::Connection> connPtr;
	stack::Connection& conn;

	std::deque<Bytes> out; //emitted, not yet delivered
	std::vector<Bytes> sent;
	size_t bytesSent = 0;
	bool transportClosed = false;
	bool peerNotified = false;
	bool silent = false; //swallows everything it is given

	bool autoRespond = true;
	bool errorForNext = false;
	bool rejectForNext = false;
	bool acceptAssociation = true;
	std::vector<uint32_t> incomingRequests; //held while autoRespond is off
	//if set, sees each incoming confirmed request first; true = it answered (or deliberately did not answer) the request itself
	std::function<bool(uint32_t, const mms::ConfirmedServiceRequest&)> requestHook;
	size_t maxSeenIncoming = 0;
	std::vector<mms::InformationReport> reports;
	bool associated = false;
	std::optional<stack::AssociateRequestInfo> lastAssociate;
	stack::EndReason endReason = stack::EndReason::TransportLost;
	std::string endText;
	int closedCount = 0;
	std::vector<std::pair<stack::LogLevel, std::string>> logs; //what the stack logged
	std::vector<mace::Status> maceStates;                      //each change of the MACE status

	//how many log lines contain the text (at the level, if one is given)
	size_t CountLogs(const std::string& part, std::optional<stack::LogLevel> level = std::nullopt) const
	{
		size_t n = 0;
		for(const auto& l : logs)
			if((!level || l.first == *level) && l.second.find(part) != std::string::npos)
				++n;
		return n;
	}
	std::string FirstLog(const std::string& part) const
	{
		for(const auto& l : logs)
			if(l.second.find(part) != std::string::npos)
				return l.second;
		return "";
	}
	Endpoint(stack::Role role, const Tweak& t):
		connPtr(Make(role, t, *this)),
		conn(*connPtr)
	{}
	Endpoint(const Endpoint&) = delete;

	void AnswerAll()
	{
		const auto ids = incomingRequests;
		incomingRequests.clear();
		for(uint32_t id : ids)
			conn.Respond(id, mms::IdentifyResponse{"VENDOR_X", "MODEL_Y", "REV_Z", std::nullopt});
	}

private:
	static std::unique_ptr<stack::Connection> Make(stack::Role role, const Tweak& t, Endpoint& e)
	{
		stack::Callbacks cb;
		cb.send = [&e](const Bytes& b)
			    {
				    e.out.push_back(b);
				    e.sent.push_back(b);
				    e.bytesSent += b.size();
			    };
		cb.closeTransport = [&e]() { e.transportClosed = true; };
		cb.onAssociateRequest = [&e](const stack::AssociateRequestInfo& info)
						{
							e.lastAssociate = info;
							return e.acceptAssociation;
						};
		cb.onAssociated = [&e]() { e.associated = true; };
		cb.onClosed = [&e](stack::EndReason r, const std::string& text)
				  {
					  e.endReason = r;
					  e.endText = text;
					  ++e.closedCount;
				  };
		if(!t.noRequestHandler)
			cb.onRequest = [&e](uint32_t id, const mms::ConfirmedServiceRequest& req)
					   {
						   if(e.requestHook && e.requestHook(id, req))
							   return;
						   if(e.errorForNext)
						   {
							   e.errorForNext = false;
							   mms::ServiceError err;
							   err.errorClass = mms::ErrorClass::Definition;
							   err.errorCode = 1;
							   e.conn.RespondError(id, err);
						   }
						   else if(e.rejectForNext)
						   {
							   e.rejectForNext = false;
							   e.conn.RejectRequest(id);
						   }
						   else if(e.autoRespond)
							   e.conn.Respond(id, mms::IdentifyResponse{"VENDOR_X", "MODEL_Y", "REV_Z", std::nullopt});
						   else
						   {
							   e.incomingRequests.push_back(id);
							   e.maxSeenIncoming = std::max(e.maxSeenIncoming, e.incomingRequests.size());
						   }
					   }
		;
		cb.onInformationReport = [&e](const mms::InformationReport& r) { e.reports.push_back(r); };
		cb.log = [&e](stack::LogLevel l, const std::string& text) { e.logs.emplace_back(l, text); };
		cb.onMaceStatus = [&e](const mace::Status& s) { e.maceStates.push_back(s); };
		return std::make_unique<stack::Connection>(role, t.config, std::move(cb));
	}
};

//Two ends wired together. a is the initiator, b the responder.
struct Link
{
	FakeClock clock;
	static void Base(Tweak& a, Tweak& b, FakeClock& clock)
	{
		const auto clockFn = [&clock]() { return clock.now; };
		a.config.clock = clockFn;
		b.config.clock = clockFn;
		a.config.local.tsap = {0x00, 0x01};
		a.config.remote.tsap = {0x00, 0x02};
		a.config.local.sSelector = {0x00, 0x11};
		a.config.remote.sSelector = {0x00, 0x12};
		a.config.local.pSelector = {0x00, 0x00, 0x00, 0x11};
		a.config.remote.pSelector = {0x00, 0x00, 0x00, 0x22};
		a.config.local.apTitle = {1, 3, 9999, 1, 1};
		a.config.remote.apTitle = {1, 3, 9999, 1, 2};
		a.config.local.aeQualifier = 7;
		a.config.remote.aeQualifier = 9;
		b.config.local = a.config.remote;
		b.config.remote = a.config.local;
	}
	static std::pair<Tweak, Tweak> Configs(const std::function<void(Tweak&, Tweak&)>& tweak, FakeClock& clock)
	{
		std::pair<Tweak, Tweak> p;
		Base(p.first, p.second, clock);
		if(tweak)
			tweak(p.first, p.second);
		return p;
	}

	std::pair<Tweak, Tweak> tweaks;
	Endpoint a;
	Endpoint b;
	size_t chunk = 0; //deliver in pieces of this many octets (0: whole)
	bool record = false;
	Bytes aToB;

	explicit Link(const std::function<void(Tweak&, Tweak&)>& tweak = nullptr):
		tweaks(Configs(tweak, clock)),
		a(stack::Role::Initiator, tweaks.first),
		b(stack::Role::Responder, tweaks.second)
	{}

	void Start()
	{
		a.conn.Start();
		b.conn.Start();
		Pump();
	}

	void Deliver(Endpoint& from, Endpoint& to)
	{
		while(!from.out.empty())
		{
			const Bytes bytes = from.out.front();
			from.out.pop_front();
			if(record && &from == &a)
				aToB.insert(aToB.end(), bytes.begin(), bytes.end());
			if(to.silent)
				continue;
			if(chunk == 0)
				to.conn.OnData(bytes.data(), bytes.size());
			else
				for(size_t off = 0; off < bytes.size(); off += chunk)
					to.conn.OnData(bytes.data() + off, std::min(chunk, bytes.size() - off));
		}
		if(from.transportClosed && !from.peerNotified)
		{
			from.peerNotified = true;
			to.conn.OnTransportClosed();
		}
	}

	//deliver one round: everything a has emitted to b, and b's to a
	void Step()
	{
		Deliver(a, b);
		Deliver(b, a);
	}

	void Pump()
	{
		for(int guard = 0; guard < 100000 && (!a.out.empty() || !b.out.empty() || (a.transportClosed && !a.peerNotified) || (b.transportClosed && !b.peerNotified)); ++guard)
			Step();
	}

	void PumpSteps(int steps)
	{
		for(int i = 0; i < steps; ++i)
			Step();
	}

	//deliver only what `from` has emitted
	void PumpOnly(Endpoint& from)
	{
		Deliver(from, &from == &a ? b : a);
	}

	void AnswerAll()
	{
		a.AnswerAll();
		b.AnswerAll();
	}
};

} // namespace stacktest
} // namespace iccp

#endif /* ICCP_STACK_TEST_HELPERS_H_ */
