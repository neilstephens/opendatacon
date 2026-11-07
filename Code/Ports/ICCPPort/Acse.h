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
 * Acse.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8650-1 / ITU-T X.227 association control service element, as used for ICCP/MMS.
 *
 *     AARQ  [APPLICATION 0]   A-ASSOCIATE request
 *     AARE  [APPLICATION 1]   A-ASSOCIATE response
 *     RLRQ  [APPLICATION 2]   A-RELEASE request
 *     RLRE  [APPLICATION 3]   A-RELEASE response
 *     ABRT  [APPLICATION 4]   A-ABORT
 *
 *  Each APDU is carried as the single-ASN.1-type PDV of a presentation context (the ACSE
 *  context negotiated in the CP/CPA), so Decode* take the PDV data and Encode returns the
 *  bytes for it. The ACSE module uses EXPLICIT tags by default: fields typed by a CHOICE or
 *  tagged without IMPLICIT carry an extra constructed wrapper (e.g. application-context-name
 *  is `a1 07 06 05 ...`), which the codec handles.
 *
 *  User information is Association-information, a sequence of EXTERNAL; for MMS it holds the
 *  MMS Initiate PDU. Each EXTERNAL here is a direct-reference and/or indirect-reference (the
 *  presentation context id), plus the single-ASN.1-type (or octet-aligned) data.
 *
 *  The authentication fields (acse-requirements, mechanism-name, authentication-value) are
 *  carried and round-tripped as they are; MACE (MMS Application Certificate Exchange) has typed helpers
 *  for its authentication value (EncodeMaceAuthValue, DecodeMaceAuthValue), the cryptography is in Mace.h.
 *
 *  This is a pure byte-buffer codec, with no I/O. Decoding is defensive: every read is
 *  bounds checked and any malformed input results in a DecodeError (the input is
 *  untrusted network data). Decoded APDUs hold views of EXTERNAL data into the caller's
 *  buffer, which must outlive them. Unknown optional fields are skipped.
 */

#ifndef ICCP_ACSE_H_
#define ICCP_ACSE_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace iccp
{
namespace acse
{

using ber::Bytes;
using ber::DecodeError;
using Oid = std::vector<uint32_t>;

//application-context-name for MMS {iso standard 9506 part2 mms-annex-version1}  (28 ca 22 02 03)
inline const Oid MmsApplicationContext{1, 0, 9506, 2, 3};

enum class ApduType : uint8_t
{
	Aarq = 0,
	Aare = 1,
	Rlrq = 2,
	Rlre = 3,
	Abrt = 4
};

//Association-result
namespace Result
{
constexpr int64_t Accepted = 0;
constexpr int64_t RejectedPermanent = 1;
constexpr int64_t RejectedTransient = 2;
}

//acse-service-user diagnostic values
namespace UserDiagnostic
{
constexpr int64_t Null = 0;
constexpr int64_t NoReasonGiven = 1;
constexpr int64_t ApplicationContextNameNotSupported = 2;
constexpr int64_t CallingApTitleNotRecognized = 3;
constexpr int64_t CallingApInvocationIdentifierNotRecognized = 4;
constexpr int64_t CallingAeQualifierNotRecognized = 5;
constexpr int64_t CallingAeInvocationIdentifierNotRecognized = 6;
constexpr int64_t CalledApTitleNotRecognized = 7;
constexpr int64_t CalledApInvocationIdentifierNotRecognized = 8;
constexpr int64_t CalledAeQualifierNotRecognized = 9;
constexpr int64_t CalledAeInvocationIdentifierNotRecognized = 10;
constexpr int64_t AuthenticationMechanismNameNotRecognized = 11;
constexpr int64_t AuthenticationMechanismNameRequired = 12;
constexpr int64_t AuthenticationFailure = 13;
constexpr int64_t AuthenticationRequired = 14;
}

//acse-service-provider diagnostic values
namespace ProviderDiagnostic
{
constexpr int64_t Null = 0;
constexpr int64_t NoReasonGiven = 1;
constexpr int64_t NoCommonAcseVersion = 2;
}

//Release-request-reason / Release-response-reason
namespace ReleaseReason
{
constexpr int64_t Normal = 0;
constexpr int64_t UrgentOrNotFinished = 1; //urgent (RLRQ) / not-finished (RLRE)
constexpr int64_t UserDefined = 30;
}

//ABRT-source
namespace AbortSource
{
constexpr int64_t AcseServiceUser = 0;
constexpr int64_t AcseServiceProvider = 1;
}

//ABRT-diagnostic
namespace AbortDiagnostic
{
constexpr int64_t NoReasonGiven = 1;
constexpr int64_t ProtocolError = 2;
constexpr int64_t AuthenticationMechanismNameNotRecognized = 3;
constexpr int64_t AuthenticationMechanismNameRequired = 4;
constexpr int64_t AuthenticationFailure = 5;
constexpr int64_t AuthenticationRequired = 6;
}

//ACSE-requirements bits (bit i -> mask 1 << i)
namespace AcseRequirement
{
constexpr uint8_t Authentication = 0x01;
constexpr uint8_t ApplicationContextNegotiation = 0x02;
}

//protocol-version bits; version 1 is the default when the field is absent
namespace ProtocolVersion
{
constexpr uint8_t Version1 = 0x01;
}

constexpr size_t MaxExternals = 4; //EXTERNALs in one Association-information

//AP-title: form 2 is an OBJECT IDENTIFIER (what ICCP uses); form 1 is an X.500 Name, carried
//as the complete BER element and not interpreted.
struct ApTitle
{
	bool form2 = true;
	Oid oid;     //form 2
	Bytes form1; //form 1: the complete BER element
};

//AE-qualifier: form 2 is an INTEGER (what ICCP uses); form 1 is an RDN, carried as the complete
//BER element and not interpreted.
struct AeQualifier
{
	bool form2 = true;
	int64_t number = 0; //form 2
	Bytes form1;        //form 1: the complete BER element
};

//Authentication hook (IEC 62351-4, and the MACE form below). The fields are decoded and encoded generically; the helpers below
//build and read the MACE authentication value.
struct Authentication
{
	std::optional<uint8_t> acseRequirements; //sender-acse-requirements (AARQ) / responder-acse-requirements (AARE)
	std::optional<Oid> mechanismName;
	std::optional<Bytes> authenticationValue; //the complete BER element of the Authentication-value CHOICE alternative
};

//---- MACE: MMS Application Certificate Exchange [V: decoded from a real peer's association, both directions]
//
//Authentication-value is the CHOICE alternative external [2] (an IMPLICIT EXTERNAL with no references or descriptor), whose
//single-ASN1-type [0] holds one constructed [0] with three primitive members:
//
//   [2] { [0] { [0] { [0] certificate (raw DER X.509, the sender's leaf certificate only)
//                     [1] signing time (GeneralizedTime text "YYYYMMDDHHMMSSZ", UTC, 15 octets)
//                     [2] signature (raw RSA output, no BIT STRING unused-bits octet) } } }
//
//The same value is used for the AARQ (calling-authentication-value [12]) and the AARE (responding-authentication-value [10]).
//The signature is not over a digest: see mace::SignedBlock (Mace.h).

//The ACSE-requirements bit that says "authentication" and the mechanism-name come with the value: all three go into the APDU
Authentication MakeMaceAuthentication(const Oid& mechanismName, const Bytes& authenticationValue);

//The pieces of a MACE authentication value, as views into the buffer that was decoded (which must outlive this)
struct MaceAuthValue
{
	const uint8_t* certificate = nullptr;
	size_t certificateLength = 0;
	const uint8_t* signingTime = nullptr;
	size_t signingTimeLength = 0;
	const uint8_t* signature = nullptr;
	size_t signatureLength = 0;

	std::string SigningTimeText() const
	{
		return std::string(reinterpret_cast<const char*>(signingTime), signingTimeLength);
	}
};

//Encode the Authentication-value element (the [2] element, to put in Authentication::authenticationValue). Throws
//std::invalid_argument if the certificate, time or signature is empty or the time is not a 15 octet GeneralizedTime.
//identification: the presentation context identifier of the A-security abstract syntax, when that was agreed for the association
//(IEC 62351-4 11.1.3, 11.1.4.4 [T]); it is then written as the INTEGER that precedes the single-ASN1-type [0] inside the [2].
//Without it the value is as shown above. [S: this form was never seen on the wire]
Bytes EncodeMaceAuthValue(const uint8_t* certificate, size_t certificateLength, const std::string& signingTime,
	const uint8_t* signature, size_t signatureLength, std::optional<int64_t> identification = std::nullopt);
inline Bytes EncodeMaceAuthValue(const Bytes& certificate, const std::string& signingTime, const Bytes& signature,
	std::optional<int64_t> identification = std::nullopt)
{
	return EncodeMaceAuthValue(certificate.data(), certificate.size(), signingTime, signature.data(), signature.size(), identification);
}

//Decode the Authentication-value element. Throws DecodeError unless it has exactly the structure above: one element at each of
//the three outer levels (the [2] may also start with the identification INTEGER, which is skipped), then the three members once
//each (unknown further members are skipped), nothing trailing.
MaceAuthValue DecodeMaceAuthValue(const uint8_t* data, size_t length);
inline MaceAuthValue DecodeMaceAuthValue(const Bytes& value)
{
	return DecodeMaceAuthValue(value.data(), value.size());
}

//GeneralizedTime in the MACE form "YYYYMMDDHHMMSSZ" (UTC, whole seconds) <-> seconds since 1970-01-01T00:00:00Z.
//Format throws std::invalid_argument outside the years 0000 to 9999; Parse is strict (15 octets, valid calendar fields).
std::string FormatGeneralizedTime(int64_t unixSeconds);
bool ParseGeneralizedTime(const std::string& text, int64_t& unixSeconds);

enum class ExternalEncoding : uint8_t
{
	SingleAsn1Type, //[0]: the data is one complete BER element (e.g. the MMS Initiate PDU)
	OctetAligned    //[1]: the data is an octet string
};

//One EXTERNAL in the user information. The data is a view; not owned.
struct External
{
	std::optional<Oid> directReference;       //transfer syntax or abstract syntax name
	std::optional<int64_t> indirectReference; //the presentation context identifier
	ExternalEncoding encoding = ExternalEncoding::SingleAsn1Type;
	const uint8_t* data = nullptr;
	size_t dataLength = 0;
};
using UserInformation = std::vector<External>;

inline External MakeExternal(int64_t presentationContextId, const uint8_t* data, size_t length,
	std::optional<Oid> directReference = std::nullopt)
{
	External e;
	e.directReference = std::move(directReference);
	e.indirectReference = presentationContextId;
	e.data = data;
	e.dataLength = length;
	return e;
}
inline External MakeExternal(int64_t presentationContextId, const Bytes& data,
	std::optional<Oid> directReference = std::nullopt)
{
	return MakeExternal(presentationContextId, data.data(), data.size(), std::move(directReference));
}

struct Aarq
{
	uint8_t protocolVersion = ProtocolVersion::Version1;
	//Write protocol-version even when it is the default (version 1). Set by the decoder when the field was
	//present; some peers always send it ("80 02 07 80"), so set it to be byte-identical with them.
	bool encodeProtocolVersion = false;
	Oid applicationContextName = MmsApplicationContext;
	std::optional<ApTitle> calledApTitle;
	std::optional<AeQualifier> calledAeQualifier;
	std::optional<int64_t> calledApInvocationId;
	std::optional<int64_t> calledAeInvocationId;
	std::optional<ApTitle> callingApTitle;
	std::optional<AeQualifier> callingAeQualifier;
	std::optional<int64_t> callingApInvocationId;
	std::optional<int64_t> callingAeInvocationId;
	Authentication authentication; //sender-acse-requirements, mechanism-name, calling-authentication-value
	std::optional<std::string> implementationInformation;
	UserInformation userInformation;
};

//Which side of the ACSE diagnostic choice
struct SourceDiagnostic
{
	bool fromProvider = false; //acse-service-provider when true, else acse-service-user
	int64_t value = UserDiagnostic::Null;
};

struct Aare
{
	uint8_t protocolVersion = ProtocolVersion::Version1;
	bool encodeProtocolVersion = false; //as for Aarq
	Oid applicationContextName = MmsApplicationContext;
	int64_t result = Result::Accepted;
	SourceDiagnostic diagnostic;
	std::optional<ApTitle> respondingApTitle;
	std::optional<AeQualifier> respondingAeQualifier;
	std::optional<int64_t> respondingApInvocationId;
	std::optional<int64_t> respondingAeInvocationId;
	Authentication authentication; //responder-acse-requirements, mechanism-name, responding-authentication-value
	std::optional<std::string> implementationInformation;
	UserInformation userInformation;
};

struct Rlrq
{
	std::optional<int64_t> reason;
	UserInformation userInformation;
};

struct Rlre
{
	std::optional<int64_t> reason;
	UserInformation userInformation;
};

struct Abrt
{
	int64_t source = AbortSource::AcseServiceUser;
	std::optional<int64_t> diagnostic;
	UserInformation userInformation;
};

//Which APDU this is; throws DecodeError unless the data is one complete [APPLICATION 0..4] element
ApduType PeekApduType(const uint8_t* data, size_t length);

//Decoders take the whole PDV data. They throw DecodeError on anything malformed (wrong APDU type,
//trailing octets, a missing mandatory field, duplicate fields, wrongly typed fields, a result outside
//0..2, an abort source outside 0..1, negative reasons or diagnostics, more than MaxExternals EXTERNALs,
//arbitrary-bit-string EXTERNALs, single-ASN.1-type data that is not exactly one BER element).
//protocol-version is reported, not enforced: check it against ProtocolVersion::Version1.
Aarq DecodeAarq(const uint8_t* data, size_t length);
Aare DecodeAare(const uint8_t* data, size_t length);
Rlrq DecodeRlrq(const uint8_t* data, size_t length);
Rlre DecodeRlre(const uint8_t* data, size_t length);
Abrt DecodeAbrt(const uint8_t* data, size_t length);

inline ApduType PeekApduType(const Bytes& b)
{
	return PeekApduType(b.data(), b.size());
}
inline Aarq DecodeAarq(const Bytes& b)
{
	return DecodeAarq(b.data(), b.size());
}
inline Aare DecodeAare(const Bytes& b)
{
	return DecodeAare(b.data(), b.size());
}
inline Rlrq DecodeRlrq(const Bytes& b)
{
	return DecodeRlrq(b.data(), b.size());
}
inline Rlre DecodeRlre(const Bytes& b)
{
	return DecodeRlre(b.data(), b.size());
}
inline Abrt DecodeAbrt(const Bytes& b)
{
	return DecodeAbrt(b.data(), b.size());
}

//Encoders throw std::invalid_argument if the APDU is not encodable (result or source out of range,
//negative reasons, too many EXTERNALs, form 1 titles that are not one BER element, single-ASN.1-type
//data that is not exactly one BER element, null data with a length).
Bytes Encode(const Aarq& aarq);
Bytes Encode(const Aare& aare);
Bytes Encode(const Rlrq& rlrq);
Bytes Encode(const Rlre& rlre);
Bytes Encode(const Abrt& abrt);

} // namespace acse
} // namespace iccp

#endif /* ICCP_ACSE_H_ */
