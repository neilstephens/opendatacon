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
 * Session.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8327-1 / ITU-T X.225 session protocol, the subset used by ICCP/MMS
 *  (kernel + duplex functional units, no tokens or synchronisation):
 *
 *     SPDU                          SI
 *     CONNECT (CN)                  13
 *     ACCEPT (AC)                   14
 *     REFUSE (RF)                   12
 *     FINISH (FN)                    9
 *     DISCONNECT (DN)               10
 *     ABORT (AB)                    25
 *     ABORT ACCEPT (AA)             26
 *     GIVE TOKENS + DATA TRANSFER    1  (always sent as the pair 01 00 01 00 <user information>)
 *
 *  SPDU layout: SI, LI, parameter field of LI octets. LI, and the length of every
 *  parameter, is one octet, or 0xFF followed by two octets (big endian) for 255 and above.
 *  Parameters are PI/PGI, length, value. A parameter group (PGI, here 1 and 5) holds PIs.
 *  User data (PI 193, or extended user data PI 194) is carried as a parameter within LI;
 *  for tolerance, octets trailing the parameter field are accepted as user data when
 *  no such parameter is present.
 *
 *  This is a pure byte-buffer codec, with no I/O. Decoding is defensive: every read is
 *  bounds checked and any malformed input results in a DecodeError (the input is
 *  untrusted network data). Decoded SPDUs hold views into the caller's buffer, which
 *  must outlive the Spdu.
 *
 *  A TSDU carries exactly one SPDU (or the GT+DT pair), so Decode() takes a whole TSDU,
 *  as produced by cotp::TsduReassembler.
 */

#ifndef ICCP_SESSION_H_
#define ICCP_SESSION_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace iccp
{
namespace session
{

using ber::Bytes;
using ber::DecodeError;

enum class SpduType : uint8_t
{
	Data = 1,        //GT + DT pair
	Finish = 9,      //FN
	Disconnect = 10, //DN
	Refuse = 12,     //RF
	Connect = 13,    //CN
	Accept = 14,     //AC
	Abort = 25,      //AB
	AbortAccept = 26 //AA
};

//Session user requirements (PI 20) functional unit bits
namespace Requirement
{
constexpr uint16_t HalfDuplex = 0x0001;
constexpr uint16_t Duplex = 0x0002;
constexpr uint16_t Expedited = 0x0004;
constexpr uint16_t MinorSync = 0x0008;
constexpr uint16_t MajorSync = 0x0010;
constexpr uint16_t Resynchronize = 0x0020;
constexpr uint16_t ActivityManagement = 0x0040;
constexpr uint16_t NegotiatedRelease = 0x0080;
constexpr uint16_t CapabilityData = 0x0100;
constexpr uint16_t Exceptions = 0x0200;
constexpr uint16_t TypedData = 0x0400;
constexpr uint16_t SymmetricSync = 0x0800;
constexpr uint16_t DataSeparation = 0x1000;
}

//Protocol version number (PI 22) bits
namespace Version
{
constexpr uint8_t Version1 = 0x01;
constexpr uint8_t Version2 = 0x02;
}

//Transport disconnect (PI 17) bits
namespace TransportDisconnect
{
constexpr uint8_t ConnectionReleased = 0x01;
constexpr uint8_t UserAbort = 0x02;
constexpr uint8_t ProtocolError = 0x04;
constexpr uint8_t NoReason = 0x08;
constexpr uint8_t ImplementationRestriction = 0x10;
}

//Refuse reason codes (PI 50, first octet); X.225 table 7
namespace RefuseReason
{
constexpr uint8_t RejectedByCalledUserUnspecified = 0;
constexpr uint8_t RejectedByCalledUserCongestion = 1;
constexpr uint8_t RejectedByCalledUserWithData = 2;
constexpr uint8_t SelectorUnknown = 128;
constexpr uint8_t UserNotAttached = 129;
constexpr uint8_t ProviderCongestion = 130;
constexpr uint8_t VersionsNotSupported = 131;
constexpr uint8_t ProviderRejectUnspecified = 132;
constexpr uint8_t ProviderRejectImplementationRestriction = 133;
}

constexpr size_t MaxSelectorLength = 16;
//User data (PI 193) limit; larger user data in CN/AC goes in extended user data (PI 194)
constexpr size_t MaxUserData = 512;
constexpr size_t MaxExtendedUserData = 10240;
//the largest value of a 3-octet length
constexpr size_t MaxLength = 0xFFFF;

//Maximum TSDU size (PI 21)
struct TsduMaxSize
{
	uint16_t initiatorToResponder = 0;
	uint16_t responderToInitiator = 0;
	bool operator==(const TsduMaxSize& r) const
	{
		return initiatorToResponder == r.initiatorToResponder && responderToInitiator == r.responderToInitiator;
	}
};

//A decoded or to-be-encoded SPDU. Absent optional members are not encoded. Which apply:
//  CN: callingSelector, calledSelector, protocolOptions, tsduMaxSize, version, tokenSetting,
//      requirements, user data
//  AC: calledSelector (the responding session selector), callingSelector, the CN items, user data
//  RF: transportDisconnect, reasonCode, requirements, version, user data
//  FN, DN: transportDisconnect (FN), user data
//  AB: transportDisconnect, user data
//  AA: none
//  Data: user data only (the TSDU payload)
//Decode accepts the known parameters wherever they appear (top level, or within PGI 1 or 5);
//Encode writes the CN/AC items in PGI 5 and the rest at top level.
struct Spdu
{
	SpduType type = SpduType::Data;
	std::optional<Bytes> callingSelector;       //PI 51
	std::optional<Bytes> calledSelector;        //PI 52
	std::optional<uint8_t> protocolOptions;     //PI 19
	std::optional<TsduMaxSize> tsduMaxSize;     //PI 21
	std::optional<uint8_t> version;             //PI 22
	std::optional<uint8_t> tokenSetting;        //PI 26
	std::optional<uint16_t> requirements;       //PI 20
	std::optional<uint8_t> transportDisconnect; //PI 17
	std::optional<uint8_t> reasonCode;          //PI 50 (first octet)
	//view of the octets after the reason code in PI 50 (a reason text, or on a refusal the presentation PPDU: some stacks
	//carry the refusal's user data there and send no PI 193); not owned
	const uint8_t* reasonText = nullptr;
	size_t reasonTextLength = 0;
	//view of user data (PI 193/194, trailing octets, or the Data payload; for an RF with none of those, the octets after
	//the reason code); not owned
	bool hasUserData = false;
	const uint8_t* userData = nullptr;
	size_t userDataLength = 0;
};

//Decode one SPDU (the whole TSDU). Throws DecodeError on anything malformed or unsupported
//(other SPDU types, a GT/DT that is not exactly 01 00 01 00, duplicate or mis-sized parameters,
//selectors over 16 octets, user data given twice). Unknown parameters are skipped.
Spdu Decode(const uint8_t* data, size_t length);
inline Spdu Decode(const Bytes& data)
{
	return Decode(data.data(), data.size());
}

//Encode one SPDU. Throws std::invalid_argument if the Spdu is not encodable (selector over 16
//octets, user data over 512 octets - 10240 for CN/AC - or null data with a length).
Bytes Encode(const Spdu& spdu);

//Convenience for the data phase: 01 00 01 00 followed by the user information
Bytes EncodeData(const uint8_t* data, size_t length);
inline Bytes EncodeData(const Bytes& data)
{
	return EncodeData(data.data(), data.size());
}

} // namespace session
} // namespace iccp

#endif /* ICCP_SESSION_H_ */
