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
 * Presentation.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8823-1 / ITU-T X.226 connection-oriented presentation protocol, normal mode,
 *  kernel functional unit (no context management), as used by ICCP/MMS.
 *
 *  PPDUs are BER encoded (iccp::ber). Their roles:
 *
 *     CP   connect presentation      carried as user data of the session CONNECT
 *     CPA  connect accept            session ACCEPT user data
 *     CPR  connect reject            session REFUSE user data
 *     ARU  user abort                session ABORT user data
 *     ARP  provider abort            session ABORT user data
 *     User-data (TD)                 session data transfer user information
 *
 *  CP and CPA have the same outer encoding, so the caller (which knows the session SPDU
 *  type) chooses the decoder.
 *
 *  User data is "fully-encoded-data": a sequence of PDV-lists, each a presentation context
 *  identifier plus either a single ASN.1 type (a complete BER element, e.g. an ACSE or MMS
 *  PDU) or an octet-aligned string. ICCP uses exactly one single-ASN.1-type PDV per PPDU.
 *
 *  This is a pure byte-buffer codec, with no I/O. Decoding is defensive: every read is
 *  bounds checked and any malformed input results in a DecodeError (the input is
 *  untrusted network data), with caps on list sizes. Decoded PPDUs hold views of PDV
 *  data into the caller's buffer, which must outlive them.
 */

#ifndef ICCP_PRESENTATION_H_
#define ICCP_PRESENTATION_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace iccp
{
namespace presentation
{

using ber::Bytes;
using ber::DecodeError;
using Oid = std::vector<uint32_t>;

//Well known object identifiers
inline const Oid AcseAbstractSyntax{2, 2, 1, 0, 1};   //acse-as-id            (52 01 00 01)
inline const Oid MmsAbstractSyntax{1, 0, 9506, 2, 1}; //mms-abstract-syntax   (28 ca 22 02 01)
//the A-security abstract syntax mmsAuthenticationAS of IEC 62351-4 11.1.3 [T] (28 86 48 00 01 01 04 01): a responder that is
//offered it answers with acceptance (and echoes it with the identification of the authentication value, see Acse.h)
inline const Oid MmsAuthenticationAbstractSyntax{1, 0, 840, 0, 1, 1, 4, 1};
inline const Oid BerTransferSyntax{2, 1, 1}; //basic-encoding        (51 01)

//Presentation context definition result (Result)
namespace Result
{
constexpr int64_t Acceptance = 0;
constexpr int64_t UserRejection = 1;
constexpr int64_t ProviderRejection = 2;
}

//Provider-reason within a context result (reason for provider rejection)
namespace ContextReason
{
constexpr int64_t NotSpecified = 0;
constexpr int64_t AbstractSyntaxNotSupported = 1;
constexpr int64_t ProposedTransferSyntaxesNotSupported = 2;
constexpr int64_t LocalLimitOnDcsExceeded = 3;
}

//CPR provider-reason
namespace RefuseReason
{
constexpr int64_t NotSpecified = 0;
constexpr int64_t TemporaryCongestion = 1;
constexpr int64_t LocalLimitExceeded = 2;
constexpr int64_t CalledPresentationAddressUnknown = 3;
constexpr int64_t ProtocolVersionNotSupported = 4;
constexpr int64_t DefaultContextNotSupported = 5;
constexpr int64_t UserDataNotReadable = 6;
constexpr int64_t NoPsapAvailable = 7;
}

//ARP abort-reason
namespace AbortReason
{
constexpr int64_t NotSpecified = 0;
constexpr int64_t UnrecognizedPpdu = 1;
constexpr int64_t UnexpectedPpdu = 2;
constexpr int64_t UnexpectedSessionServicePrimitive = 3;
constexpr int64_t UnrecognizedPpduParameter = 4;
constexpr int64_t UnexpectedPpduParameter = 5;
constexpr int64_t InvalidPpduParameterValue = 6;
}

//Presentation requirements (bit string, bit i -> mask 1 << i)
namespace PresentationRequirement
{
constexpr uint8_t ContextManagement = 0x01;
constexpr uint8_t Restoration = 0x02;
}
//User session requirements are the session functional units, with the same bit values as
//session::Requirement (bit i -> mask 1 << i): bit 0 half-duplex, bit 1 duplex, ...

constexpr size_t MaxSelectorLength = 16;
constexpr size_t MaxContexts = 32;        //presentation contexts in a definition or result list
constexpr size_t MaxTransferSyntaxes = 8; //per presentation context
constexpr size_t MaxPdvs = 16;            //PDV-lists in one user-data

struct ContextDefinition
{
	int64_t id = 0; //odd, positive
	Oid abstractSyntax;
	std::vector<Oid> transferSyntaxes;
};

struct ContextResult
{
	int64_t result = Result::Acceptance;
	std::optional<Oid> transferSyntax;     //present when accepted
	std::optional<int64_t> providerReason; //present for provider rejection
};

struct DefaultContextName
{
	Oid abstractSyntax;
	Oid transferSyntax;
};

enum class PdvKind : uint8_t
{
	SingleAsn1Type, //[0]: the data is one complete BER element
	OctetAligned    //[1]: the data is an arbitrary octet string
};

//One presentation data value list. The data is a view; not owned.
struct Pdv
{
	std::optional<Oid> transferSyntax;
	int64_t contextId = 0;
	PdvKind kind = PdvKind::SingleAsn1Type;
	const uint8_t* data = nullptr;
	size_t dataLength = 0;
};
//empty means the user-data parameter is absent (the ASN.1 requires at least one PDV-list)
using UserData = std::vector<Pdv>;

//Build a single-ASN.1-type PDV viewing the given data
inline Pdv MakePdv(int64_t contextId, const uint8_t* data, size_t length)
{
	Pdv p;
	p.contextId = contextId;
	p.data = data;
	p.dataLength = length;
	return p;
}
inline Pdv MakePdv(int64_t contextId, const Bytes& data)
{
	return MakePdv(contextId, data.data(), data.size());
}

//CP-PPDU, normal mode. Protocol version 1 is implied (the default, so not encoded).
struct ConnectRequest
{
	std::optional<Bytes> callingSelector;
	std::optional<Bytes> calledSelector;
	std::vector<ContextDefinition> contexts; //empty: list not encoded
	std::optional<DefaultContextName> defaultContextName;
	std::optional<uint8_t> presentationRequirements;
	std::optional<uint16_t> userSessionRequirements;
	UserData userData;
};

//CPA-PPDU, normal mode
struct ConnectAccept
{
	std::optional<Bytes> respondingSelector;
	std::vector<ContextResult> results;
	std::optional<uint8_t> presentationRequirements;
	std::optional<uint16_t> userSessionRequirements;
	UserData userData;
};

//CPR-PPDU, normal mode
struct ConnectRefuse
{
	std::optional<Bytes> respondingSelector;
	std::vector<ContextResult> results;
	std::optional<int64_t> defaultContextResult;
	std::optional<int64_t> providerReason;
	UserData userData;
};

//ARU-PPDU (user abort) or ARP-PPDU (provider abort)
struct Abort
{
	bool providerAbort = false;             //ARP when true, else ARU
	UserData userData;                      //ARU only
	std::optional<int64_t> providerReason;  //ARP only
	std::optional<int64_t> eventIdentifier; //ARP only
};

//Decoders take the whole user data of the session SPDU. They throw DecodeError on anything
//malformed or unsupported (X.410 mode, trailing octets, duplicate or misplaced parameters,
//protocol versions without version 1, selectors over 16 octets, non-positive context
//identifiers, over-long lists, arbitrary-bit-string or simply-encoded data, a single-ASN.1-type
//PDV that is not exactly one BER element). Unknown context-specific parameters are skipped.
ConnectRequest DecodeConnectRequest(const uint8_t* data, size_t length);
ConnectAccept DecodeConnectAccept(const uint8_t* data, size_t length);
ConnectRefuse DecodeConnectRefuse(const uint8_t* data, size_t length);
Abort DecodeAbort(const uint8_t* data, size_t length);
//Data phase: user-data PPDU holding exactly one PDV-list
Pdv DecodeData(const uint8_t* data, size_t length);

inline ConnectRequest DecodeConnectRequest(const Bytes& b)
{
	return DecodeConnectRequest(b.data(), b.size());
}
inline ConnectAccept DecodeConnectAccept(const Bytes& b)
{
	return DecodeConnectAccept(b.data(), b.size());
}
inline ConnectRefuse DecodeConnectRefuse(const Bytes& b)
{
	return DecodeConnectRefuse(b.data(), b.size());
}
inline Abort DecodeAbort(const Bytes& b)
{
	return DecodeAbort(b.data(), b.size());
}
inline Pdv DecodeData(const Bytes& b)
{
	return DecodeData(b.data(), b.size());
}

//Encoders throw std::invalid_argument if the PPDU is not encodable (selector over 16 octets,
//non-positive context id, over-long lists, a single-ASN.1-type PDV that is not one BER element,
//null data with a length).
Bytes Encode(const ConnectRequest& cp);
Bytes Encode(const ConnectAccept& cpa);
Bytes Encode(const ConnectRefuse& cpr);
Bytes Encode(const Abort& ab);
//Data phase: a user-data PPDU with a single PDV-list
Bytes EncodeData(const Pdv& pdv);
inline Bytes EncodeData(int64_t contextId, const uint8_t* data, size_t length)
{
	return EncodeData(MakePdv(contextId, data, length));
}
inline Bytes EncodeData(int64_t contextId, const Bytes& data)
{
	return EncodeData(MakePdv(contextId, data));
}

} // namespace presentation
} // namespace iccp

#endif /* ICCP_PRESENTATION_H_ */
