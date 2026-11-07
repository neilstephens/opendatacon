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
 * IccpStack.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  One OSI association for ICCP, see IccpStack.h.
 */

#include "IccpStack.h"
#include "MmsNames.h"
#include <algorithm>
#include <stdexcept>

namespace iccp
{
namespace stack
{

mms::BitString DefaultServicesSupported()
{
	using namespace mms::ServiceBit;
	return mms::MakeBits({GetNameList, Identify, Read, Write, GetVariableAccessAttributes, DefineNamedVariableList,
		                GetNamedVariableListAttributes, DeleteNamedVariableList, InformationReport, Conclude}, 85);
}

mms::BitString DefaultParameterCbb()
{
	using namespace mms::ParameterBit;
	return mms::MakeBits({Str1, Str2, Vnam, Vlis}, 11);
}

const char* ToString(EndReason r)
{
	switch(r)
	{
		case EndReason::Released: return "released";
		case EndReason::ReleasedByPeer: return "released by peer";
		case EndReason::AbortedLocal: return "aborted";
		case EndReason::AbortedByPeer: return "aborted by peer";
		case EndReason::TransportLost: return "transport lost";
		case EndReason::Refused: return "refused";
		case EndReason::Timeout: return "timeout";
	}
	return "?";
}

std::string Describe(const Outcome& o)
{
	switch(o.kind)
	{
		case Outcome::Kind::Response: return "ok";
		case Outcome::Kind::Error: return "service error: " + mms::Describe(o.error);
		case Outcome::Kind::Rejected: return "rejected: " + mms::Describe(o.reject);
		case Outcome::Kind::Timeout: return "timed out";
		case Outcome::Kind::Aborted: return "association ended";
		case Outcome::Kind::Malformed: return "malformed response (" + o.text + ")";
	}
	return "?";
}

const char* UserDiagnosticName(int64_t v)
{
	switch(v)
	{
		case acse::UserDiagnostic::Null: return "null";
		case acse::UserDiagnostic::NoReasonGiven: return "no-reason-given";
		case acse::UserDiagnostic::ApplicationContextNameNotSupported: return "application-context-name-not-supported";
		case acse::UserDiagnostic::CallingApTitleNotRecognized: return "calling-AP-title-not-recognized";
		case acse::UserDiagnostic::CallingApInvocationIdentifierNotRecognized: return "calling-AP-invocation-identifier-not-recognized";
		case acse::UserDiagnostic::CallingAeQualifierNotRecognized: return "calling-AE-qualifier-not-recognized";
		case acse::UserDiagnostic::CallingAeInvocationIdentifierNotRecognized: return "calling-AE-invocation-identifier-not-recognized";
		case acse::UserDiagnostic::CalledApTitleNotRecognized: return "called-AP-title-not-recognized";
		case acse::UserDiagnostic::CalledApInvocationIdentifierNotRecognized: return "called-AP-invocation-identifier-not-recognized";
		case acse::UserDiagnostic::CalledAeQualifierNotRecognized: return "called-AE-qualifier-not-recognized";
		case acse::UserDiagnostic::CalledAeInvocationIdentifierNotRecognized: return "called-AE-invocation-identifier-not-recognized";
		case acse::UserDiagnostic::AuthenticationMechanismNameNotRecognized: return "authentication-mechanism-name-not-recognized";
		case acse::UserDiagnostic::AuthenticationMechanismNameRequired: return "authentication-mechanism-name-required";
		case acse::UserDiagnostic::AuthenticationFailure: return "authentication-failure";
		case acse::UserDiagnostic::AuthenticationRequired: return "authentication-required";
	}
	return "unknown";
}

//what an association response that refuses says: the result, who gave the diagnostic and what it is, and the MMS initiate error if one is carried
std::string DescribeRefusal(const acse::Aare& aare)
{
	std::string result = std::to_string(aare.result);
	if(aare.result == acse::Result::RejectedPermanent)
		result = "rejected-permanent";
	else if(aare.result == acse::Result::RejectedTransient)
		result = "rejected-transient";
	std::string text = "ACSE result " + result + ", diagnostic from the " + (aare.diagnostic.fromProvider ? "ACSE service provider" : "ACSE service user") + ": ";
	if(aare.diagnostic.fromProvider)
		text += std::to_string(aare.diagnostic.value);
	else
		text += std::string(UserDiagnosticName(aare.diagnostic.value)) + " (" + std::to_string(aare.diagnostic.value) + ")";
	if(!aare.userInformation.empty())
	{
		try
		{
			const acse::External& x = aare.userInformation[0];
			const mms::Pdu pdu = mms::Decode(x.data, x.dataLength);
			if(const auto* ie = std::get_if<mms::InitiateError>(&pdu))
				text += "; MMS initiate error: " + mms::Describe(ie->error);
		}
		catch(const std::exception&)
		{}
	}
	return text;
}

namespace
{
Bytes View(const uint8_t* p, size_t n)
{
	return n == 0 ? Bytes() : Bytes(p, p + n);
}

std::optional<Bytes> NonEmpty(const Bytes& b)
{
	if(b.empty())
		return std::nullopt;
	return b;
}

std::string SelectorText(const Bytes& b)
{
	if(b.empty())
		return "none";
	static constexpr char Digits[] = "0123456789abcdef";
	std::string s;
	for(uint8_t v : b)
	{
		s += Digits[v >> 4];
		s += Digits[v & 0x0F];
	}
	return s;
}

//what a transport disconnect request says, with the transport selectors we sent when the peer turned the connection request down
std::string DescribeTransportDisconnect(const cotp::Tpdu& tpdu, const Config& config, bool refusedConnect)
{
	std::string name;
	switch(tpdu.reason)
	{
		case cotp::DisconnectReason::NotSpecified:
			name = "not specified";
			break;
		case cotp::DisconnectReason::CongestionAtTsap:
			name = "congestion at TSAP";
			break;
		case cotp::DisconnectReason::SessionEntityNotAttached:
			name = "session entity not attached to TSAP";
			break;
		case cotp::DisconnectReason::AddressUnknown:
			name = "address unknown";
			break;
		case cotp::DisconnectReason::NormalDisconnect:
			name = "normal disconnect";
			break;
		case cotp::DisconnectReason::ConnectionRefused:
			name = "connection refused on this network connection";
			break;
		default:
			name = "reason " + std::to_string(tpdu.reason);
			break;
	}
	std::string text = "transport disconnect request from the peer (" + name + ")";
	if(refusedConnect)
	{
		text += " in answer to our connection request (calling TSAP " + SelectorText(config.local.tsap) + ", called TSAP " + SelectorText(config.remote.tsap) + ")";
		if(tpdu.reason == cotp::DisconnectReason::AddressUnknown || tpdu.reason == cotp::DisconnectReason::SessionEntityNotAttached)
			text += ": the peer does not accept the transport selectors, check LocalTSelector and RemoteTSelector";
	}
	return text;
}
}

Connection::Connection(Role r, Config c, Callbacks callbacks):
	role(r),
	config(std::move(c)),
	cb(std::move(callbacks)),
	reassembler(config.maxTsduSize, config.tpduSizeCode),
	tracker(1)
{
	negotiated.tpduSizeCode = config.tpduSizeCode;
	maceStatus.enabled = config.mace != nullptr;
}

void Connection::SetMaceState(mace::State s, const std::string& error)
{
	maceStatus.enabled = true;
	maceStatus.state = s;
	if(!error.empty())
		maceStatus.lastError = error;
	if(cb.onMaceStatus)
		cb.onMaceStatus(maceStatus);
}

//Make our authentication for the AARQ or AARE with a fresh signature; on failure the state is Failed and the reason is logged
bool Connection::SignMace(acse::Authentication& out, std::optional<int64_t> authContextId)
{
	mace::Signed s;
	std::string error;
	if(!config.mace->Sign(config.utcClock(), s, error, authContextId))
	{
		const std::string text = "MACE: cannot make our authentication: " + error;
		Log(LogLevel::Error, text);
		SetMaceState(mace::State::Failed, text);
		return false;
	}
	out = std::move(s.authentication);
	maceStatus.signingTime = s.signingTime;
	Log(LogLevel::Info, "MACE: authenticating with certificate " + s.subject + ", signing time " + mace::IsoTime(s.signingTime));
	return true;
}

bool Connection::CheckResponderAuthentication(const acse::Aare& aare)
{
	const mace::MaceConf& conf = config.mace->Conf();
	if(!aare.authentication.authenticationValue)
	{
		if(conf.requireResponderAuthentication)
		{
			const std::string text = "MACE: the association response carries no authentication (\"RequireResponderAuthentication\" is true)";
			Log(LogLevel::Error, text);
			SetMaceState(mace::State::Failed, text);
			Abort(text);
			return false;
		}
		Log(LogLevel::Warn, "MACE: the peer did not authenticate in its association response (\"RequireResponderAuthentication\" is false)");
		return true;
	}
	mace::PeerInfo peer;
	mace::Failure failure;
	if(!config.mace->Verify(aare.authentication, config.utcClock(), peer, failure))
	{
		Log(LogLevel::Error, failure.text);
		SetMaceState(mace::State::Failed, failure.text);
		Abort(failure.text);
		return false;
	}
	maceStatus.peerSubject = peer.subject;
	maceStatus.peerNotAfter = peer.notAfter;
	maceStatus.peerSigningTime = peer.signingTime;
	LogPeerMechanism(peer);
	Log(LogLevel::Info, "MACE: peer authenticated: " + peer.subject + ", signing time " + mace::IsoTime(peer.signingTime));
	SetMaceState(mace::State::Verified);
	return true;
}

//One line per association when the peer's mechanism-name is not the official one (a warning) or, if it is, not the same as ours
//(info). The value is tolerated either way: IEC 62351-4 11.1.4.3.
void Connection::LogPeerMechanism(const mace::PeerInfo& peer) const
{
	const std::string ours = mace::OidToString(config.mace->Conf().mechanismName);
	if(!peer.mechanismOfficial)
	{
		std::string text = "MACE: the peer's mechanism-name " + peer.mechanismName + " is not the one IEC 62351-4 defines (" + mace::OfficialMechanismText + ")";
		if(peer.mechanismName == mace::LegacyMechanismText)
			text += ": it is the legacy value of IEC TS 62351-4:2007";
		if(peer.mechanismDiffers)
			text += "; it also differs from ours " + ours;
		Log(LogLevel::Warn, text + ": accepted, because the standard asks implementations to be tolerant of it");
	}
	else if(peer.mechanismDiffers)
		Log(LogLevel::Info, "MACE: the peer's mechanism-name " + peer.mechanismName + " differs from ours " + ours + ": accepted, as IEC 62351-4 asks implementations to be tolerant");
}

void Connection::Log(LogLevel level, const std::string& text) const
{
	if(cb.log)
		cb.log(level, text);
}

void Connection::SetDeadline(std::chrono::milliseconds after)
{
	deadline = Now() + after;
	haveDeadline = true;
}

void Connection::ClearDeadline()
{
	haveDeadline = false;
}

// ---------------------------------------------------------------------------------------------
// sending
// ---------------------------------------------------------------------------------------------

void Connection::Emit(const Bytes& bytes)
{
	if(cb.send && !bytes.empty())
		cb.send(bytes);
}

void Connection::SendTpdu(const Bytes& tpdu)
{
	Emit(tpkt::Frame(tpdu));
}

void Connection::SendTsdu(const Bytes& tsdu)
{
	Bytes out;
	for(const Bytes& segment : cotp::Segment(tsdu, negotiated.tpduSizeCode))
	{
		const Bytes frame = tpkt::Frame(segment);
		out.insert(out.end(), frame.begin(), frame.end());
	}
	Emit(out);
}

void Connection::SendSpdu(const session::Spdu& spdu)
{
	SendTsdu(session::Encode(spdu));
}

void Connection::SendAcseOnSession(session::SpduType type, const Bytes& apdu)
{
	const Bytes ppdu = presentation::EncodeData(negotiated.acseContextId, apdu);
	session::Spdu s;
	s.type = type;
	s.hasUserData = true;
	s.userData = ppdu.data();
	s.userDataLength = ppdu.size();
	SendSpdu(s);
}

void Connection::SendMms(const mms::Pdu& pdu)
{
	const Bytes bytes = mms::Encode(pdu);
	const Bytes ppdu = presentation::EncodeData(negotiated.mmsContextId, bytes);
	SendTsdu(session::EncodeData(ppdu));
}

void Connection::Start()
{
	if(state != State::Idle)
		return;
	SetDeadline(config.associateTimeout);
	if(role != Role::Initiator)
		return;
	try
	{
		state = State::TransportPending;
		proposedSizeCode = config.tpduSizeCode;
		SendTpdu(cotp::EncodeConnectRequest(config.localReference, NonEmpty(config.local.tsap), NonEmpty(config.remote.tsap), proposedSizeCode));
	}
	catch(const std::exception& e)
	{
		ProtocolError(std::string("cannot start: ") + e.what());
	}
}

void Connection::SendConnect()
{
	proposedContexts = {
		{1, presentation::AcseAbstractSyntax, {presentation::BerTransferSyntax}
		},
		{3, presentation::MmsAbstractSyntax, {presentation::BerTransferSyntax}
		}
	};
	negotiated.acseContextId = 1;
	negotiated.mmsContextId = 3;

	mms::InitiateRequest init;
	init.localDetailCalling = config.localDetail;
	init.proposedMaxServOutstandingCalling = config.maxOutstandingCalling;
	init.proposedMaxServOutstandingCalled = config.maxOutstandingCalled;
	init.proposedDataStructureNestingLevel = config.nestingLevel;
	init.proposedVersionNumber = 1;
	init.proposedParameterCbb = config.parameterCbb;
	init.servicesSupportedCalling = config.servicesSupported;
	const Bytes initBytes = mms::Encode(init);

	acse::Aarq aarq;
	aarq.encodeProtocolVersion = config.writeProtocolVersion;
	if(!config.remote.apTitle.empty())
		aarq.calledApTitle = acse::ApTitle{true, config.remote.apTitle, {}
		}
	;
	if(config.remote.aeQualifier)
		aarq.calledAeQualifier = acse::AeQualifier{true, *config.remote.aeQualifier, {}
		}
	;
	if(!config.local.apTitle.empty())
		aarq.callingApTitle = acse::ApTitle{true, config.local.apTitle, {}
		}
	;
	if(config.local.aeQualifier)
		aarq.callingAeQualifier = acse::AeQualifier{true, *config.local.aeQualifier, {}
		}
	;
	if(config.mace)
	{
		if(!SignMace(aarq.authentication))
		{
			ProtocolError("MACE: our authentication could not be made");
			return;
		}
		SetMaceState(mace::State::Sent);
	}
	aarq.userInformation.push_back(acse::MakeExternal(negotiated.mmsContextId, initBytes));
	const Bytes aarqBytes = acse::Encode(aarq);

	presentation::ConnectRequest cp;
	cp.callingSelector = NonEmpty(config.local.pSelector);
	cp.calledSelector = NonEmpty(config.remote.pSelector);
	cp.contexts = proposedContexts;
	cp.presentationRequirements = 0;
	cp.userData.push_back(presentation::MakePdv(negotiated.acseContextId, aarqBytes));
	const Bytes cpBytes = presentation::Encode(cp);

	session::Spdu s;
	s.type = session::SpduType::Connect;
	s.protocolOptions = 0;
	s.version = session::Version::Version2;
	s.requirements = session::Requirement::Duplex;
	s.callingSelector = NonEmpty(config.local.sSelector);
	s.calledSelector = NonEmpty(config.remote.sSelector);
	s.hasUserData = true;
	s.userData = cpBytes.data();
	s.userDataLength = cpBytes.size();
	SendSpdu(s);
}

// ---------------------------------------------------------------------------------------------
// receiving
// ---------------------------------------------------------------------------------------------

void Connection::OnData(const uint8_t* data, size_t length)
{
	if(state == State::Closed)
		return;
	try
	{
		reader.Feed(data, length);
		Bytes payload;
		while(state != State::Closed && reader.Next(payload))
			HandleTpdu(payload);
	}
	catch(const std::exception& e)
	{
		ProtocolError(std::string("invalid input: ") + e.what());
	}
}

void Connection::OnTransportClosed()
{
	if(state == State::Closed)
		return;
	if(state == State::Releasing || peerConcluded)
		Finish(role == Role::Initiator && state == State::Releasing ? EndReason::Released : EndReason::ReleasedByPeer, "transport closed during release", false);
	else
		Finish(EndReason::TransportLost, "transport closed", false);
}

void Connection::HandleTpdu(const Bytes& payload)
{
	const cotp::Tpdu tpdu = cotp::Decode(payload);
	switch(tpdu.type)
	{
		case cotp::TpduType::ConnectRequest:
			HandleConnectRequestTpdu(tpdu);
			break;
		case cotp::TpduType::ConnectConfirm:
			HandleConnectConfirmTpdu(tpdu);
			break;
		case cotp::TpduType::DisconnectRequest:
			Finish(state == State::TransportPending ? EndReason::Refused : EndReason::TransportLost,
				DescribeTransportDisconnect(tpdu, config, state == State::TransportPending), true);
			break;
		case cotp::TpduType::Data:
			if(state == State::Idle || state == State::TransportPending)
			{
				ProtocolError("data before the transport connection was established");
				return;
			}
			if(reassembler.Add(tpdu))
				HandleTsdu(reassembler.Take());
			break;
	}
}

void Connection::HandleConnectRequestTpdu(const cotp::Tpdu& tpdu)
{
	if(role != Role::Responder || state != State::Idle)
	{
		ProtocolError("unexpected transport connect request");
		return;
	}
	peerReference = tpdu.srcRef;
	negotiated.tpduSizeCode = cotp::NegotiateTpduSize(tpdu.tpduSizeCode, config.tpduSizeCode);
	state = State::AwaitConnect;
	SendTpdu(cotp::EncodeConnectConfirm(peerReference, config.localReference, tpdu.callingTsap, tpdu.calledTsap, negotiated.tpduSizeCode));
}

void Connection::HandleConnectConfirmTpdu(const cotp::Tpdu& tpdu)
{
	if(role != Role::Initiator || state != State::TransportPending)
	{
		ProtocolError("unexpected transport connect confirm");
		return;
	}
	peerReference = tpdu.srcRef;
	negotiated.tpduSizeCode = cotp::ConfirmTpduSize(proposedSizeCode, tpdu.tpduSizeCode);
	state = State::AssociatePending;
	SendConnect();
}

void Connection::HandleTsdu(const Bytes& tsdu)
{
	const session::Spdu spdu = session::Decode(tsdu);
	switch(state)
	{
		case State::AwaitConnect:
			if(spdu.type == session::SpduType::Connect)
				HandleSessionConnect(spdu);
			else
				ProtocolError("expected a session connect");
			return;
		case State::AssociatePending:
			if(spdu.type == session::SpduType::Accept)
				HandleSessionAccept(spdu);
			else if(spdu.type == session::SpduType::Refuse)
				HandleSessionRefuse(spdu);
			else if(spdu.type == session::SpduType::Abort)
				Finish(EndReason::AbortedByPeer, "peer aborted during association", true);
			else
				ProtocolError("expected a session accept");
			return;
		case State::Associated:
		case State::Concluding:
		case State::Releasing:
			break;
		default:
			ProtocolError("session data in the wrong state");
			return;
	}

	switch(spdu.type)
	{
		case session::SpduType::Data:
			HandleDataPhase(spdu);
			break;
		case session::SpduType::Finish:
			HandleFinish(spdu);
			break;
		case session::SpduType::Disconnect:
			if(state == State::Releasing)
				Finish(EndReason::Released, "released", true);
			else
				ProtocolError("unexpected session disconnect");
			break;
		case session::SpduType::Abort:
			try
			{
				session::Spdu aa;
				aa.type = session::SpduType::AbortAccept;
				SendSpdu(aa);
			}
			catch(const std::exception&)
			{}
			Finish(EndReason::AbortedByPeer, "peer aborted", true);
			break;
		case session::SpduType::AbortAccept:
			break;
		default:
			ProtocolError("unexpected session PDU");
			break;
	}
}

void Connection::Refuse(const std::string& why, int64_t acseDiagnostic)
{
	Log(LogLevel::Warn, "refusing the association: " + why);
	try
	{
		acse::Aare aare;
		aare.encodeProtocolVersion = config.writeProtocolVersion;
		aare.result = acse::Result::RejectedPermanent;
		aare.diagnostic = acse::SourceDiagnostic{false, acseDiagnostic};
		const Bytes aareBytes = acse::Encode(aare);
		presentation::ConnectRefuse cpr;
		cpr.userData.push_back(presentation::MakePdv(negotiated.acseContextId, aareBytes));
		const Bytes cprBytes = presentation::Encode(cpr);
		session::Spdu rf;
		rf.type = session::SpduType::Refuse;
		rf.reasonCode = session::RefuseReason::RejectedByCalledUserWithData;
		rf.transportDisconnect = session::TransportDisconnect::ConnectionReleased;
		rf.hasUserData = true;
		rf.userData = cprBytes.data();
		rf.userDataLength = cprBytes.size();
		SendSpdu(rf);
	}
	catch(const std::exception& e)
	{
		Log(LogLevel::Debug, std::string("could not send the refusal: ") + e.what());
	}
	Finish(EndReason::Refused, why, true);
}

void Connection::HandleSessionConnect(const session::Spdu& spdu)
{
	if(!spdu.hasUserData)
	{
		ProtocolError("session connect without user data");
		return;
	}
	const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
	const presentation::ConnectRequest cp = presentation::DecodeConnectRequest(ppdu);

	//the contexts: find ACSE and MMS (both with BER), answer in the proposed order
	int64_t acseId = 0, mmsId = 0, maceId = 0;
	std::vector<presentation::ContextResult> results;
	for(const auto& c : cp.contexts)
	{
		const bool ber = std::find(c.transferSyntaxes.begin(), c.transferSyntaxes.end(), presentation::BerTransferSyntax) != c.transferSyntaxes.end();
		if(ber && c.abstractSyntax == presentation::AcseAbstractSyntax && acseId == 0)
		{
			acseId = c.id;
			results.push_back({presentation::Result::Acceptance, presentation::BerTransferSyntax, std::nullopt});
		}
		else if(ber && c.abstractSyntax == presentation::MmsAbstractSyntax && mmsId == 0)
		{
			mmsId = c.id;
			results.push_back({presentation::Result::Acceptance, presentation::BerTransferSyntax, std::nullopt});
		}
		else if(ber && config.mace && c.abstractSyntax == presentation::MmsAuthenticationAbstractSyntax && maceId == 0)
		{
			//the A-security abstract syntax (IEC 62351-4 11.1.3): accepted when offered, and only when we do MACE. Nothing is ever sent in it.
			maceId = c.id;
			results.push_back({presentation::Result::Acceptance, presentation::BerTransferSyntax, std::nullopt});
		}
		else
			results.push_back({presentation::Result::ProviderRejection, std::nullopt, presentation::ContextReason::AbstractSyntaxNotSupported});
	}
	negotiated.acseContextId = acseId != 0 ? acseId : 1;
	if(acseId == 0 || mmsId == 0)
	{
		Refuse("the ACSE and MMS presentation contexts were not both proposed", acse::UserDiagnostic::NoReasonGiven);
		return;
	}
	negotiated.mmsContextId = mmsId;
	if(cp.userData.size() != 1 || cp.userData[0].contextId != acseId || cp.userData[0].kind != presentation::PdvKind::SingleAsn1Type)
	{
		Refuse("no ACSE association request in the connect", acse::UserDiagnostic::NoReasonGiven);
		return;
	}

	const Bytes apdu = View(cp.userData[0].data, cp.userData[0].dataLength);
	const acse::Aarq aarq = acse::DecodeAarq(apdu);
	if(aarq.applicationContextName != acse::MmsApplicationContext)
	{
		Refuse("application context is not MMS", acse::UserDiagnostic::ApplicationContextNameNotSupported);
		return;
	}
	if(aarq.userInformation.empty())
	{
		Refuse("the association request carries no MMS Initiate", acse::UserDiagnostic::NoReasonGiven);
		return;
	}
	const acse::External& x = aarq.userInformation[0];
	const mms::Pdu initPdu = mms::Decode(x.data, x.dataLength);
	const auto* init = std::get_if<mms::InitiateRequest>(&initPdu);
	if(init == nullptr)
	{
		Refuse("the association request does not carry an MMS Initiate request", acse::UserDiagnostic::NoReasonGiven);
		return;
	}
	if(init->proposedVersionNumber < 1)
	{
		Refuse("no common MMS version", acse::UserDiagnostic::NoReasonGiven);
		return;
	}

	//MACE: verify the initiator's authentication before anything else is accepted; our own goes into the response
	bool respondWithAuthentication = false;
	std::optional<acse::Oid> echoMechanismName; //the peer's mechanism-name, when it is not ours: our response carries that one [S]
	if(config.mace)
	{
		const mace::MaceConf& mc = config.mace->Conf();
		if(!aarq.authentication.authenticationValue)
		{
			if(mc.requireAuthentication)
			{
				const std::string why = "MACE: the association request carries no authentication (\"RequireAuthentication\" is true)";
				SetMaceState(mace::State::Failed, why);
				Refuse(why, acse::UserDiagnostic::AuthenticationRequired);
				return;
			}
			Log(LogLevel::Warn, "MACE: the association request carries no authentication: accepted, because \"RequireAuthentication\" is false");
		}
		else
		{
			mace::PeerInfo peer;
			mace::Failure failure;
			if(!config.mace->Verify(aarq.authentication, config.utcClock(), peer, failure))
			{
				SetMaceState(mace::State::Failed, failure.text);
				Refuse(failure.text, failure.diagnostic);
				return;
			}
			maceStatus.peerSubject = peer.subject;
			maceStatus.peerNotAfter = peer.notAfter;
			maceStatus.peerSigningTime = peer.signingTime;
			LogPeerMechanism(peer);
			Log(LogLevel::Info, "MACE: peer authenticated: " + peer.subject + ", signing time " + mace::IsoTime(peer.signingTime));
			respondWithAuthentication = true; //(the status goes to Verified once our response is signed)
			if(peer.mechanismDiffers)
				echoMechanismName = aarq.authentication.mechanismName;
		}
	}
	else if((aarq.authentication.authenticationValue || aarq.authentication.mechanismName || aarq.authentication.acseRequirements) && !maceWarned)
	{
		maceWarned = true;
		Log(LogLevel::Warn, "the association request carries authentication fields, which are ignored because MACE is not enabled");
	}

	AssociateRequestInfo info;
	info.callingSSelector = spdu.callingSelector.value_or(Bytes());
	info.calledSSelector = spdu.calledSelector.value_or(Bytes());
	info.callingPSelector = cp.callingSelector.value_or(Bytes());
	info.calledPSelector = cp.calledSelector.value_or(Bytes());
	if(aarq.callingApTitle && aarq.callingApTitle->form2)
		info.callingApTitle = aarq.callingApTitle->oid;
	if(aarq.callingAeQualifier && aarq.callingAeQualifier->form2)
		info.callingAeQualifier = aarq.callingAeQualifier->number;
	if(aarq.calledApTitle && aarq.calledApTitle->form2)
		info.calledApTitle = aarq.calledApTitle->oid;
	if(aarq.calledAeQualifier && aarq.calledAeQualifier->form2)
		info.calledAeQualifier = aarq.calledAeQualifier->number;
	info.initiate = *init;
	if(cb.onAssociateRequest && !cb.onAssociateRequest(info))
	{
		Refuse("the association request was not accepted", acse::UserDiagnostic::CallingApTitleNotRecognized);
		return;
	}

	//negotiate: the initiator's "calling" limit is what we accept from it, "called" what we may issue to it
	const int64_t calling = std::max<int64_t>(1, std::min(init->proposedMaxServOutstandingCalling, config.maxOutstandingCalling));
	const int64_t called = std::max<int64_t>(1, std::min(init->proposedMaxServOutstandingCalled, config.maxOutstandingCalled));
	mms::InitiateResponse resp;
	resp.localDetailCalled = init->localDetailCalling ? std::min(*init->localDetailCalling, config.localDetail) : config.localDetail;
	resp.negotiatedMaxServOutstandingCalling = calling;
	resp.negotiatedMaxServOutstandingCalled = called;
	resp.negotiatedDataStructureNestingLevel = init->proposedDataStructureNestingLevel ?
	                                           std::min(*init->proposedDataStructureNestingLevel, config.nestingLevel) : config.nestingLevel;
	resp.negotiatedVersionNumber = 1;
	resp.negotiatedParameterCbb = mms::MakeBits({}, config.parameterCbb.BitCount());
	for(size_t i = 0; i < config.parameterCbb.BitCount(); ++i)
		if(config.parameterCbb.Get(i) && init->proposedParameterCbb.Get(i))
			resp.negotiatedParameterCbb.Set(i, true);
	resp.servicesSupportedCalled = config.servicesSupported;
	const Bytes respBytes = mms::Encode(resp);

	negotiated.maxOutstandingLocal = static_cast<size_t>(called);
	negotiated.maxOutstandingPeer = static_cast<size_t>(calling);
	negotiated.nestingLevel = *resp.negotiatedDataStructureNestingLevel;
	negotiated.version = 1;
	negotiated.peerServices = init->servicesSupportedCalling;
	negotiated.peerCbb = init->proposedParameterCbb;

	acse::Aare aare;
	aare.encodeProtocolVersion = config.writeProtocolVersion;
	aare.result = acse::Result::Accepted;
	aare.diagnostic = acse::SourceDiagnostic{false, acse::UserDiagnostic::Null};
	if(!config.local.apTitle.empty())
		aare.respondingApTitle = acse::ApTitle{true, config.local.apTitle, {}
		}
	;
	if(config.local.aeQualifier)
		aare.respondingAeQualifier = acse::AeQualifier{true, *config.local.aeQualifier, {}
		}
	;
	aare.userInformation.push_back(acse::MakeExternal(mmsId, respBytes));
	if(respondWithAuthentication)
	{
		if(!SignMace(aare.authentication, maceId != 0 ? std::optional<int64_t>(maceId) : std::nullopt))
		{
			Refuse("MACE: our authentication could not be made", acse::UserDiagnostic::NoReasonGiven);
			return;
		}
		if(echoMechanismName)
			aare.authentication.mechanismName = *echoMechanismName;
		SetMaceState(mace::State::Verified);
	}
	const Bytes aareBytes = acse::Encode(aare);

	presentation::ConnectAccept cpa;
	cpa.respondingSelector = NonEmpty(config.local.pSelector.empty() ? cp.calledSelector.value_or(Bytes()) : config.local.pSelector);
	cpa.results = results;
	cpa.presentationRequirements = 0;
	cpa.userData.push_back(presentation::MakePdv(acseId, aareBytes));
	const Bytes cpaBytes = presentation::Encode(cpa);

	session::Spdu ac;
	ac.type = session::SpduType::Accept;
	ac.protocolOptions = 0;
	ac.version = session::Version::Version2;
	ac.requirements = session::Requirement::Duplex;
	ac.hasUserData = true;
	ac.userData = cpaBytes.data();
	ac.userDataLength = cpaBytes.size();
	SendSpdu(ac);

	tracker.SetMaxOutstanding(negotiated.maxOutstandingLocal);
	BecomeAssociated();
}

void Connection::HandleSessionAccept(const session::Spdu& spdu)
{
	if(!spdu.hasUserData)
	{
		ProtocolError("session accept without user data");
		return;
	}
	const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
	const presentation::ConnectAccept cpa = presentation::DecodeConnectAccept(ppdu);
	if(cpa.results.size() != proposedContexts.size())
	{
		ProtocolError("presentation context result list does not match the proposal");
		return;
	}
	for(const auto& r : cpa.results)
		if(r.result != presentation::Result::Acceptance)
		{
			Finish(EndReason::Refused, "the peer rejected a presentation context", true);
			return;
		}
	if(cpa.userData.size() != 1 || cpa.userData[0].kind != presentation::PdvKind::SingleAsn1Type)
	{
		ProtocolError("no ACSE response in the session accept");
		return;
	}
	const Bytes apdu = View(cpa.userData[0].data, cpa.userData[0].dataLength);
	const acse::Aare aare = acse::DecodeAare(apdu);
	if(aare.result != acse::Result::Accepted)
	{
		Finish(EndReason::Refused, "association rejected by the peer (" + DescribeRefusal(aare) + ")", true);
		return;
	}
	if(config.mace && !CheckResponderAuthentication(aare))
		return;
	if(aare.userInformation.empty())
	{
		ProtocolError("the association response carries no MMS Initiate");
		return;
	}
	const acse::External& x = aare.userInformation[0];
	const mms::Pdu pdu = mms::Decode(x.data, x.dataLength);
	if(const auto* ie = std::get_if<mms::InitiateError>(&pdu))
	{
		Finish(EndReason::Refused, "MMS initiate refused by the peer: " + mms::Describe(ie->error), true);
		return;
	}
	const auto* resp = std::get_if<mms::InitiateResponse>(&pdu);
	if(resp == nullptr)
	{
		ProtocolError("the association response does not carry an MMS Initiate response");
		return;
	}
	negotiated.maxOutstandingLocal = static_cast<size_t>(std::max<int64_t>(1, resp->negotiatedMaxServOutstandingCalling));
	negotiated.maxOutstandingPeer = static_cast<size_t>(std::max<int64_t>(1, resp->negotiatedMaxServOutstandingCalled));
	negotiated.nestingLevel = resp->negotiatedDataStructureNestingLevel.value_or(0);
	negotiated.version = resp->negotiatedVersionNumber;
	negotiated.peerServices = resp->servicesSupportedCalled;
	negotiated.peerCbb = resp->negotiatedParameterCbb;
	tracker.SetMaxOutstanding(negotiated.maxOutstandingLocal);
	BecomeAssociated();
}

void Connection::HandleSessionRefuse(const session::Spdu& spdu)
{
	std::string why = "association refused by the peer";
	if(spdu.hasUserData)
	{
		try
		{
			//the decoded structures point into these buffers: keep them alive while they are used
			const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
			const presentation::ConnectRefuse cpr = presentation::DecodeConnectRefuse(ppdu);
			if(cpr.userData.size() == 1)
			{
				const Bytes apdu = View(cpr.userData[0].data, cpr.userData[0].dataLength);
				const acse::Aare aare = acse::DecodeAare(apdu);
				why += " (" + DescribeRefusal(aare) + ")";
			}
		}
		catch(const std::exception& e)
		{
			why += " (its explanation could not be decoded: " + std::string(e.what()) + ")";
		}
	}
	Finish(EndReason::Refused, why, true);
}

void Connection::BecomeAssociated()
{
	state = State::Associated;
	ClearDeadline();
	if(cb.onAssociated)
		cb.onAssociated();
}

void Connection::HandleDataPhase(const session::Spdu& spdu)
{
	const presentation::Pdv pdv = presentation::DecodeData(spdu.userData, spdu.userDataLength);
	if(pdv.kind != presentation::PdvKind::SingleAsn1Type)
	{
		ProtocolError("presentation data is not a single ASN.1 type");
		return;
	}
	if(pdv.contextId == negotiated.mmsContextId)
		HandleMms(pdv.data, pdv.dataLength);
	else if(pdv.contextId == negotiated.acseContextId)
	{
		const Bytes apdu = View(pdv.data, pdv.dataLength);
		if(acse::PeekApduType(apdu) == acse::ApduType::Abrt)
			Finish(EndReason::AbortedByPeer, "peer sent an ACSE abort", true);
		else
			Log(LogLevel::Warn, "ignoring an unexpected ACSE APDU in the data phase");
	}
	else
		ProtocolError("data on an unknown presentation context");
}

void Connection::HandleFinish(const session::Spdu&)
{
	try
	{
		acse::Rlre rlre;
		rlre.reason = acse::ReleaseReason::Normal;
		SendAcseOnSession(session::SpduType::Disconnect, acse::Encode(rlre));
	}
	catch(const std::exception& e)
	{
		Log(LogLevel::Debug, std::string("could not send the release response: ") + e.what());
	}
	Finish(state == State::Releasing ? EndReason::Released : EndReason::ReleasedByPeer, "session released", true);
}

void Connection::HandleMms(const uint8_t* data, size_t length)
{
	std::optional<mms::Pdu> decoded;
	try
	{
		decoded = mms::Decode(data, length);
	}
	catch(const ber::DecodeError& e)
	{
		const auto id = mms::PeekInvokeId(data, length);
		const uint8_t top = length > 0 ? data[0] : 0;
		if(top == 0xa0 && id)
		{
			const bool unsupported = std::string(e.what()).find("unsupported") != std::string::npos;
			Log(LogLevel::Warn, std::string("rejecting an undecodable request: ") + e.what());
			mms::Reject rj;
			rj.originalInvokeId = *id;
			rj.type = mms::RejectType::ConfirmedRequestPdu;
			rj.reason = unsupported ? mms::RejectReason::UnrecognizedService : mms::RejectReason::InvalidArgument;
			SendMms(rj);
			return;
		}
		if((top == 0xa1 || top == 0xa2) && id && outstanding.count(*id))
		{
			Outcome o;
			o.kind = Outcome::Kind::Malformed;
			o.text = e.what();
			CompleteOutstanding(*id, o);
			return;
		}
		if(top == 0xa3)
		{
			Log(LogLevel::Warn, std::string("ignoring an undecodable unconfirmed PDU: ") + e.what());
			return;
		}
		ProtocolError(std::string("undecodable MMS PDU: ") + e.what());
		return;
	}

	const mms::Pdu& pdu = *decoded;
	if(const auto* req = std::get_if<mms::ConfirmedRequest>(&pdu))
	{
		if(incoming.count(req->invokeId))
		{
			mms::Reject rj;
			rj.originalInvokeId = req->invokeId;
			rj.reason = mms::RejectReason::InvalidInvokeId;
			SendMms(rj);
		}
		else if(incoming.size() >= negotiated.maxOutstandingPeer)
		{
			mms::Reject rj;
			rj.originalInvokeId = req->invokeId;
			rj.reason = mms::RejectReason::MaxServOutstandingExceeded;
			SendMms(rj);
		}
		else if(!cb.onRequest)
		{
			mms::Reject rj;
			rj.originalInvokeId = req->invokeId;
			rj.reason = mms::RejectReason::UnrecognizedService;
			SendMms(rj);
		}
		else
		{
			incoming.insert(req->invokeId);
			cb.onRequest(req->invokeId, req->service);
		}
	}
	else if(const auto* rsp = std::get_if<mms::ConfirmedResponse>(&pdu))
	{
		Outcome o;
		o.kind = Outcome::Kind::Response;
		o.response = rsp->service;
		CompleteOutstanding(rsp->invokeId, o);
	}
	else if(const auto* err = std::get_if<mms::ConfirmedError>(&pdu))
	{
		Outcome o;
		o.kind = Outcome::Kind::Error;
		o.error = err->error;
		CompleteOutstanding(err->invokeId, o);
	}
	else if(const auto* rep = std::get_if<mms::InformationReport>(&pdu))
	{
		if(cb.onInformationReport)
			cb.onInformationReport(*rep);
	}
	else if(const auto* rj = std::get_if<mms::Reject>(&pdu))
	{
		if(rj->originalInvokeId && outstanding.count(*rj->originalInvokeId))
		{
			Outcome o;
			o.kind = Outcome::Kind::Rejected;
			o.reject = *rj;
			CompleteOutstanding(*rj->originalInvokeId, o);
		}
		else
			Log(LogLevel::Warn, "the peer rejected something we cannot match to a request: " + mms::Describe(*rj));
	}
	else if(std::holds_alternative<mms::ConcludeRequest>(pdu))
	{
		if(state == State::Associated)
		{
			peerConcluded = true;
			SendMms(mms::ConcludeResponse{});
			SetDeadline(config.releaseTimeout);
		}
		else if(state == State::Concluding && role == Role::Responder)
		{
			//both ends concluding at once: the association's initiator wins. Give way: drop our own conclusion (the initiator
			//refuses it, which is then ignored) and answer theirs.
			state = State::Associated;
			peerConcluded = true;
			SendMms(mms::ConcludeResponse{});
			SetDeadline(config.releaseTimeout);
		}
		else
		{
			//the initiator keeps its conclusion going and refuses the responder's
			mms::ConcludeError ce;
			ce.error.errorClass = mms::ErrorClass::Conclude;
			SendMms(ce);
		}
	}
	else if(std::holds_alternative<mms::ConcludeResponse>(pdu))
	{
		if(state == State::Concluding)
		{
			state = State::Releasing;
			SetDeadline(config.releaseTimeout);
			acse::Rlrq rlrq;
			rlrq.reason = acse::ReleaseReason::Normal;
			SendAcseOnSession(session::SpduType::Finish, acse::Encode(rlrq));
		}
		else
			Log(LogLevel::Warn, "unexpected Conclude response");
	}
	else if(std::holds_alternative<mms::ConcludeError>(pdu))
	{
		if(state == State::Concluding)
		{
			state = State::Associated;
			ClearDeadline();
			Log(LogLevel::Warn, "the peer refused the Conclude");
		}
	}
	else
		ProtocolError("initiate PDU in the data phase");
}

// ---------------------------------------------------------------------------------------------
// requests
// ---------------------------------------------------------------------------------------------

void Connection::SendRequest(mms::ConfirmedServiceRequest request, ResponseHandler handler)
{
	if(state != State::Associated)
	{
		Outcome o;
		o.kind = Outcome::Kind::Aborted;
		o.text = "the association is not established";
		if(handler)
			handler(o);
		return;
	}
	if(queued.size() >= config.maxQueuedRequests)
	{
		Outcome o;
		o.kind = Outcome::Kind::Aborted;
		o.text = "too many requests queued";
		if(handler)
			handler(o);
		return;
	}
	queued.push_back(Queued{std::move(request), std::move(handler)});
	PumpQueue();
}

void Connection::PumpQueue()
{
	while(state == State::Associated && !queued.empty())
	{
		const auto id = tracker.Allocate();
		if(!id)
			break;
		Queued q = std::move(queued.front());
		queued.pop_front();
		try
		{
			SendMms(mms::ConfirmedRequest{*id, q.request});
		}
		catch(const std::exception& e)
		{
			tracker.Complete(*id);
			Outcome o;
			o.kind = Outcome::Kind::Malformed;
			o.text = std::string("could not encode the request: ") + e.what();
			if(q.handler)
				q.handler(o);
			continue;
		}
		outstanding[*id] = Outstanding{std::move(q.handler), Now() + config.requestTimeout};
	}
}

void Connection::CompleteOutstanding(uint32_t id, Outcome outcome)
{
	const auto it = outstanding.find(id);
	if(it == outstanding.end())
	{
		Log(LogLevel::Warn, "response for an unknown invoke id " + std::to_string(id) + " ignored");
		return;
	}
	ResponseHandler handler = std::move(it->second.handler);
	outstanding.erase(it);
	tracker.Complete(id);
	if(handler)
		handler(outcome);
	PumpQueue();
}

bool Connection::Respond(uint32_t invokeId, const mms::ConfirmedServiceResponse& response)
{
	if(state == State::Closed || incoming.erase(invokeId) == 0)
		return false;
	try
	{
		SendMms(mms::ConfirmedResponse{invokeId, response});
	}
	catch(const std::exception& e)
	{
		Log(LogLevel::Error, std::string("could not encode a response: ") + e.what());
		mms::ServiceError err;
		err.errorClass = mms::ErrorClass::Others;
		err.additionalDescription = "response could not be encoded";
		try
		{
			SendMms(mms::ConfirmedError{invokeId, std::nullopt, err});
		}
		catch(const std::exception&)
		{}
		return false;
	}
	return true;
}

bool Connection::RespondError(uint32_t invokeId, const mms::ServiceError& error)
{
	if(state == State::Closed || incoming.erase(invokeId) == 0)
		return false;
	try
	{
		SendMms(mms::ConfirmedError{invokeId, std::nullopt, error});
	}
	catch(const std::exception& e)
	{
		Log(LogLevel::Error, std::string("could not encode an error: ") + e.what());
		return false;
	}
	return true;
}

bool Connection::RejectRequest(uint32_t invokeId, int64_t reason)
{
	if(state == State::Closed || incoming.erase(invokeId) == 0)
		return false;
	mms::Reject rj;
	rj.originalInvokeId = invokeId;
	rj.type = mms::RejectType::ConfirmedRequestPdu;
	rj.reason = reason;
	try
	{
		SendMms(rj);
	}
	catch(const std::exception&)
	{
		return false;
	}
	return true;
}

bool Connection::SendReport(const mms::InformationReport& report)
{
	//nothing may follow the peer's Conclude, which has been answered (the release is under way)
	if(state != State::Associated || peerConcluded)
		return false;
	try
	{
		SendMms(report);
	}
	catch(const std::exception& e)
	{
		Log(LogLevel::Error, std::string("could not encode a report: ") + e.what());
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// time, release, abort
// ---------------------------------------------------------------------------------------------

void Connection::Tick()
{
	if(state == State::Closed)
		return;
	const auto now = Now();
	if(haveDeadline && now >= deadline)
	{
		if(state == State::Associated && peerConcluded)
			Finish(EndReason::Timeout, "the peer concluded but did not release", true);
		else if(state == State::Concluding || state == State::Releasing)
		{
			Abort("the release timed out");
			return;
		}
		else
			Finish(EndReason::Timeout, "association set-up timed out", true);
		return;
	}
	std::vector<uint32_t> expired;
	for(const auto& [id, o] : outstanding)
		if(now >= o.deadline)
			expired.push_back(id);
	for(uint32_t id : expired)
	{
		Outcome o;
		o.kind = Outcome::Kind::Timeout;
		o.text = "request timed out";
		CompleteOutstanding(id, o);
	}
}

std::optional<Clock::time_point> Connection::NextDeadline() const
{
	std::optional<Clock::time_point> next;
	if(state == State::Closed)
		return next;
	if(haveDeadline)
		next = deadline;
	for(const auto& [id, o] : outstanding)
		if(!next || o.deadline < *next)
			next = o.deadline;
	return next;
}

void Connection::Conclude()
{
	if(state == State::Closed || state == State::Concluding || state == State::Releasing)
		return;
	if(state != State::Associated)
	{
		Finish(EndReason::AbortedLocal, "closed before the association was established", true);
		return;
	}
	state = State::Concluding;
	SetDeadline(config.releaseTimeout);
	try
	{
		SendMms(mms::ConcludeRequest{});
	}
	catch(const std::exception& e)
	{
		ProtocolError(std::string("could not send the Conclude: ") + e.what());
	}
}

void Connection::Abort(const std::string& reason)
{
	if(state == State::Closed)
		return;
	if(state == State::Associated || state == State::Concluding || state == State::Releasing || state == State::AssociatePending)
	{
		try
		{
			acse::Abrt abrt;
			abrt.source = acse::AbortSource::AcseServiceUser;
			const Bytes abrtBytes = acse::Encode(abrt);
			presentation::Abort aru;
			aru.providerAbort = false;
			aru.userData.push_back(presentation::MakePdv(negotiated.acseContextId, abrtBytes));
			const Bytes aruBytes = presentation::Encode(aru);
			session::Spdu ab;
			ab.type = session::SpduType::Abort;
			ab.transportDisconnect = session::TransportDisconnect::UserAbort;
			ab.hasUserData = true;
			ab.userData = aruBytes.data();
			ab.userDataLength = aruBytes.size();
			SendSpdu(ab);
		}
		catch(const std::exception&)
		{}
	}
	Finish(EndReason::AbortedLocal, reason, true);
}

void Connection::ProtocolError(const std::string& text)
{
	Log(LogLevel::Error, "protocol error: " + text);
	Abort(text);
}

void Connection::FailAll(Outcome::Kind kind, const std::string& text)
{
	auto pending = std::move(outstanding);
	outstanding.clear();
	auto waiting = std::move(queued);
	queued.clear();
	tracker.Clear();
	incoming.clear();
	Outcome o;
	o.kind = kind;
	o.text = text;
	for(auto& [id, p] : pending)
		if(p.handler)
			p.handler(o);
	for(auto& q : waiting)
		if(q.handler)
			q.handler(o);
}

void Connection::Finish(EndReason reason, const std::string& text, bool closeTransport)
{
	if(state == State::Closed)
		return;
	state = State::Closed;
	ClearDeadline();
	FailAll(Outcome::Kind::Aborted, text);
	if(closeTransport && cb.closeTransport)
		cb.closeTransport();
	Log(LogLevel::Info, std::string("association ended (") + ToString(reason) + "): " + text);
	if(cb.onClosed)
		cb.onClosed(reason, text);
}

} // namespace stack
} // namespace iccp
