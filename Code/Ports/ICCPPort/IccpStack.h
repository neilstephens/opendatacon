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
 * IccpStack.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  One OSI association for ICCP: TPKT/COTP, session, presentation, ACSE and MMS over a byte stream, as a state
 *  machine without sockets or timers of its own. The owner feeds it received bytes (OnData), the end of the
 *  transport (OnTransportClosed) and the passage of time (Tick), and receives bytes to send and events through
 *  Callbacks. All calls must be made from one thread at a time (the port serialises them on a strand).
 *
 *  Both ends can issue confirmed requests and serve the peer's, on the one association.
 *
 *  Association set-up (initiator): COTP CR/CC, session CN/AC carrying a presentation CP/CPA carrying an ACSE
 *  AARQ/AARE carrying the MMS Initiate request/response. Presentation context ids come from the negotiated result.
 *
 *  Release (CHOICE, recorded): the side that closes sends an MMS Conclude request; on the Conclude response it sends a
 *  session FINISH carrying an ACSE RLRQ in the presentation user data; the peer answers a session DISCONNECT carrying
 *  an RLRE; then the transport is closed. A peer that releases at the session level only (FINISH without a Conclude), or
 *  that closes the transport after the Conclude response, is accepted too.
 *
 *  Abort: session ABORT carrying a presentation ARU with an ACSE ABRT, then the transport is closed.
 *  Input that cannot be decoded aborts the association (except a malformed confirmed request with a readable invoke id,
 *  which is answered with an MMS Reject). The stack never throws out of its public methods.
 */

#ifndef ICCP_STACK_H_
#define ICCP_STACK_H_

#include "Acse.h"
#include "COTP.h"
#include "Mace.h"
#include "Mms.h"
#include "Presentation.h"
#include "Session.h"
#include "TPKT.h"
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace iccp
{
namespace stack
{

using ber::Bytes;
using Clock = std::chrono::steady_clock;

enum class Role : uint8_t
{
	Initiator, //opens the transport connection, sends CR / CN / AARQ / Initiate
	Responder
};

enum class State : uint8_t
{
	Idle,             //not started / responder waiting for the CR
	TransportPending, //initiator: CR sent
	AwaitConnect,     //responder: CC sent, waiting for the session CN
	AssociatePending, //initiator: CN sent, waiting for the AC
	Associated,       //data phase
	Concluding,       //we sent a Conclude request
	Releasing,        //we sent a session FINISH
	Closed
};

//Addressing for one end
struct Identity
{
	Bytes tsap;
	Bytes sSelector;
	Bytes pSelector;
	acse::Oid apTitle; //empty: not sent
	std::optional<int64_t> aeQualifier;
};

mms::BitString DefaultServicesSupported();
mms::BitString DefaultParameterCbb();

struct Config
{
	Identity local;
	Identity remote;                           //initiator: called address; responder: expected calling address (not enforced here)
	uint8_t tpduSizeCode = cotp::TpduSize1024; //proposed (initiator) / largest accepted (responder)
	size_t maxTsduSize = cotp::TsduReassembler::DefaultMaxTsdu;
	uint16_t localReference = 1; //COTP source reference

	//MMS Initiate
	int64_t localDetail = 32000;
	int64_t maxOutstandingCalling = 10; //requests the initiator may have outstanding
	int64_t maxOutstandingCalled = 10;  //requests the responder may have outstanding
	int64_t nestingLevel = 4;
	mms::BitString parameterCbb = DefaultParameterCbb();
	mms::BitString servicesSupported = DefaultServicesSupported();

	std::chrono::milliseconds associateTimeout{10000}; //from the start to the association being established
	std::chrono::milliseconds requestTimeout{5000};    //per confirmed request
	std::chrono::milliseconds releaseTimeout{5000};    //conclude / release exchange
	bool writeProtocolVersion = true;                  //ACSE protocol-version written although it is the default
	size_t maxQueuedRequests = 10000;
	std::function<Clock::time_point()> clock = [] { return Clock::now(); };

	//MACE authentication in the ACSE association (null: off). Used by both roles: the initiator authenticates in the AARQ, the responder
	//verifies it and authenticates in the AARE, and the initiator verifies that.
	std::shared_ptr<const mace::Context> mace;
	//UTC wall clock in seconds since the epoch: the signing time and the time window check use it (tests replace it)
	std::function<int64_t()> utcClock = []
							{
								return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
							};
};

//What a responder learns from the association request
struct AssociateRequestInfo
{
	Bytes callingSSelector;
	Bytes calledSSelector;
	Bytes callingPSelector;
	Bytes calledPSelector;
	std::optional<acse::Oid> callingApTitle;
	std::optional<int64_t> callingAeQualifier;
	std::optional<acse::Oid> calledApTitle;
	std::optional<int64_t> calledAeQualifier;
	mms::InitiateRequest initiate;
};

struct Negotiated
{
	uint8_t tpduSizeCode = cotp::DefaultTpduSize;
	int64_t acseContextId = 1;
	int64_t mmsContextId = 3;
	size_t maxOutstandingLocal = 1; //confirmed requests we may have outstanding
	size_t maxOutstandingPeer = 1;  //confirmed requests the peer may have outstanding
	int64_t nestingLevel = 0;
	int64_t version = 1;
	mms::BitString peerServices;
	mms::BitString peerCbb;
};

//Why the association ended
enum class EndReason : uint8_t
{
	Released,       //we concluded and the peer released
	ReleasedByPeer, //the peer concluded or finished
	AbortedLocal,   //we aborted (protocol error, local request)
	AbortedByPeer,
	TransportLost, //the transport closed or was refused
	Refused,       //the peer refused the association
	Timeout        //set-up, release or similar timed out
};
const char* ToString(EndReason r);

enum class LogLevel : uint8_t
{
	Trace,
	Debug,
	Info,
	Warn,
	Error
};

//Result of a confirmed request
struct Outcome
{
	enum class Kind : uint8_t
	{
		Response, //`response` is valid
		Error,    //the peer answered with a service error
		Rejected, //the peer rejected the request
		Timeout,
		Aborted,  //the association ended first
		Malformed //the response could not be decoded
	};
	Kind kind = Kind::Aborted;
	mms::ConfirmedServiceResponse response;
	mms::ServiceError error;
	mms::Reject reject;
	std::string text;
	bool Ok() const
	{
		return kind == Kind::Response;
	}
	template<class T>
	const T* Get() const
	{
		return kind == Kind::Response ? std::get_if<T>(&response) : nullptr;
	}
};
using ResponseHandler = std::function<void (const Outcome&)>;

//The outcome in words, for logs: "ok", "service error: definition [2] / object-undefined [1]", "rejected: ...", "timed out", ...
std::string Describe(const Outcome& outcome);

struct Callbacks
{
	std::function<void(const Bytes&)> send;                              //required
	std::function<void()> closeTransport;                                //required
	std::function<bool(const AssociateRequestInfo&)> onAssociateRequest; //responder: false refuses (default accepts)
	std::function<void()> onAssociated;
	std::function<void(EndReason, const std::string&)> onClosed;                  //exactly once
	std::function<void(uint32_t, const mms::ConfirmedServiceRequest&)> onRequest; //answer with Respond/RespondError/RejectRequest
	std::function<void(const mms::InformationReport&)> onInformationReport;
	std::function<void(LogLevel, const std::string&)> log;
	std::function<void(const mace::Status&)> onMaceStatus; //MACE: whenever the authentication state changes
};

class Connection
{
public:
	Connection(Role role, Config config, Callbacks callbacks);

	//Initiator: send the transport connect request. Responder: nothing to do (waits for the CR).
	void Start();
	void OnData(const uint8_t* data, size_t length);
	void OnTransportClosed();
	//checks the deadlines against Config::clock
	void Tick();
	//The earliest deadline, for owners that schedule their timer precisely
	std::optional<Clock::time_point> NextDeadline() const;

	State GetState() const
	{
		return state;
	}
	bool IsAssociated() const
	{
		return state == State::Associated;
	}
	const Negotiated& GetNegotiated() const
	{
		return negotiated;
	}
	const Config& GetConfig() const
	{
		return config;
	}
	size_t OutstandingRequests() const
	{
		return outstanding.size();
	}
	size_t QueuedRequests() const
	{
		return queued.size();
	}
	//MACE authentication of this association (enabled = false when the stack has no MACE context)
	const mace::Status& GetMaceStatus() const
	{
		return maceStatus;
	}

	//Issue a confirmed request. Queued while the peer's outstanding limit is reached. The handler is always called,
	//exactly once (immediately with Aborted if the association is not up).
	void SendRequest(mms::ConfirmedServiceRequest request, ResponseHandler handler);
	//Answer a request given to onRequest. false if the id is not an outstanding request of the peer.
	bool Respond(uint32_t invokeId, const mms::ConfirmedServiceResponse& response);
	bool RespondError(uint32_t invokeId, const mms::ServiceError& error);
	bool RejectRequest(uint32_t invokeId, int64_t reason = mms::RejectReason::UnrecognizedService);
	//Send an unconfirmed InformationReport
	bool SendReport(const mms::InformationReport& report);

	//Orderly release (see the header comment). The end is reported through onClosed.
	void Conclude();
	//Abort now
	void Abort(const std::string& reason = "aborted");

private:
	Role role;
	Config config;
	Callbacks cb;
	State state = State::Idle;
	Negotiated negotiated;
	tpkt::Reader reader;
	cotp::TsduReassembler reassembler;
	uint16_t peerReference = 0;
	uint8_t proposedSizeCode = 0;
	std::vector<presentation::ContextDefinition> proposedContexts;
	Clock::time_point deadline{};
	bool haveDeadline = false;
	bool peerConcluded = false;
	mace::Status maceStatus;
	bool maceWarned = false;

	struct Outstanding
	{
		ResponseHandler handler;
		Clock::time_point deadline;
	};
	mms::InvokeIdTracker tracker;
	std::map<uint32_t, Outstanding> outstanding;
	struct Queued
	{
		mms::ConfirmedServiceRequest request;
		ResponseHandler handler;
	};
	std::deque<Queued> queued;
	std::set<uint32_t> incoming;

	void Log(LogLevel level, const std::string& text) const;
	Clock::time_point Now() const
	{
		return config.clock();
	}
	void SetDeadline(std::chrono::milliseconds after);
	void ClearDeadline();

	//sending
	void Emit(const Bytes& bytes);
	void SendTpdu(const Bytes& tpdu);
	void SendTsdu(const Bytes& tsdu);
	void SendSpdu(const session::Spdu& spdu);
	void SendMms(const mms::Pdu& pdu);
	void SendAcseOnSession(session::SpduType type, const Bytes& apdu);
	void PumpQueue();

	//receiving
	void HandleTpdu(const Bytes& payload);
	void HandleTsdu(const Bytes& tsdu);
	void HandleConnectRequestTpdu(const cotp::Tpdu& tpdu);
	void HandleConnectConfirmTpdu(const cotp::Tpdu& tpdu);
	void HandleSessionConnect(const session::Spdu& spdu);
	void HandleSessionAccept(const session::Spdu& spdu);
	void HandleSessionRefuse(const session::Spdu& spdu);
	void HandleDataPhase(const session::Spdu& spdu);
	void HandleMms(const uint8_t* data, size_t length);
	void HandleFinish(const session::Spdu& spdu);

	void SendConnect();
	void SetMaceState(mace::State state, const std::string& error = "");
	bool SignMace(acse::Authentication& out, std::optional<int64_t> authContextId = std::nullopt);
	void LogPeerMechanism(const mace::PeerInfo& peer) const;
	//initiator: check the authentication of the association response; false = the association was aborted
	bool CheckResponderAuthentication(const acse::Aare& aare);
	void Refuse(const std::string& why, int64_t acseDiagnostic);
	void BecomeAssociated();
	void CompleteOutstanding(uint32_t id, Outcome outcome);
	void FailAll(Outcome::Kind kind, const std::string& text);

	//ends the association: state Closed, outstanding requests failed, transport closed, onClosed called once
	void Finish(EndReason reason, const std::string& text, bool closeTransport = true);
	void ProtocolError(const std::string& text);
};

} // namespace stack
} // namespace iccp

#endif /* ICCP_STACK_H_ */
