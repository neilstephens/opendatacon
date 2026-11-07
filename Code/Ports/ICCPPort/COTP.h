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
 * COTP.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 8073 / ITU-T X.224 transport protocol, class 0 (connection oriented, no
 *  multiplexing, no flow control), as used over TCP by RFC 1006.
 *
 *  TPDUs handled: CR, CC, DR and DT. Each TPKT carries exactly one TPDU, so Decode()
 *  takes the TPKT payload as a whole.
 *
 *  Decoding is defensive: every read is bounds checked and any malformed input
 *  results in a DecodeError (the input is untrusted network data). Decoded
 *  TPDUs hold views into the caller's buffer, which must outlive the Tpdu.
 *
 *  Encoding produces the TPDU only; wrap it with tpkt::Frame() for the wire.
 */

#ifndef ICCP_COTP_H_
#define ICCP_COTP_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace iccp
{
namespace cotp
{

using ber::Bytes;
using ber::DecodeError;

enum class TpduType : uint8_t
{
	ConnectRequest = 0xE0,    //CR
	ConnectConfirm = 0xD0,    //CC
	DisconnectRequest = 0x80, //DR
	Data = 0xF0               //DT
};

//DR reason codes (ISO 8073 table 14)
namespace DisconnectReason
{
constexpr uint8_t NotSpecified = 0;
constexpr uint8_t CongestionAtTsap = 1;
constexpr uint8_t SessionEntityNotAttached = 2;
constexpr uint8_t AddressUnknown = 3;
constexpr uint8_t NormalDisconnect = 128;
constexpr uint8_t RemoteCongestion = 129;
constexpr uint8_t NegotiationFailed = 130;
constexpr uint8_t DuplicateSourceRef = 131;
constexpr uint8_t MismatchedReferences = 132;
constexpr uint8_t ProtocolError = 133;
constexpr uint8_t ReferenceOverflow = 135;
constexpr uint8_t ConnectionRefused = 136;
constexpr uint8_t InvalidHeader = 138;
}

//TPDU size parameter codes: the maximum TPDU length (header included) is 2^code octets
constexpr uint8_t TpduSize128 = 7;
constexpr uint8_t TpduSize256 = 8;
constexpr uint8_t TpduSize512 = 9;
constexpr uint8_t TpduSize1024 = 10;
constexpr uint8_t TpduSize2048 = 11;
constexpr uint8_t TpduSize4096 = 12; //RFC 1006 / ISO 8073 amendment for class 0
constexpr uint8_t TpduSize8192 = 13;
//size in force when the parameter is absent from a CR (and, here, from a CC)
constexpr uint8_t DefaultTpduSize = TpduSize128;

constexpr size_t MaxTsapLength = 32;
//class 0 DT header: LI, code, TPDU-NR/EOT
constexpr size_t DataHeaderSize = 3;

inline bool IsValidTpduSizeCode(uint8_t code)
{
	return code >= TpduSize128 && code <= TpduSize8192;
}
//Maximum TPDU length in octets for a (valid) size code, 0 if the code is invalid
inline size_t TpduSizeOctets(uint8_t code)
{
	return IsValidTpduSizeCode(code) ? (static_cast<size_t>(1) << code) : 0;
}

//A decoded or to-be-encoded TPDU. Which members apply depends on `type`:
//  CR/CC: dstRef, srcRef, callingTsap, calledTsap, tpduSizeCode
//  DR:    dstRef, srcRef, reason, data (optional user data)
//  DT:    eot, data
struct Tpdu
{
	TpduType type = TpduType::Data;
	uint16_t dstRef = 0;
	uint16_t srcRef = 0;
	std::optional<Bytes> callingTsap;
	std::optional<Bytes> calledTsap;
	std::optional<uint8_t> tpduSizeCode;
	uint8_t reason = DisconnectReason::NotSpecified;
	bool eot = true;
	//view of DT user data / DR additional data; not owned
	const uint8_t* data = nullptr;
	size_t dataLength = 0;
};

//Decode one TPDU (the whole TPKT payload). Throws DecodeError on anything malformed
//or unsupported (other TPDU types, classes other than 0, user data in CR/CC).
//Unknown optional parameters in CR/CC/DR are skipped.
Tpdu Decode(const uint8_t* data, size_t length);
inline Tpdu Decode(const Bytes& data)
{
	return Decode(data.data(), data.size());
}

//Encode one TPDU. Throws std::invalid_argument if the Tpdu is not encodable
//(TSAP over 32 octets, invalid size code, DR data over 64 octets, null data with length...).
Bytes Encode(const Tpdu& tpdu);

//Convenience builders
Bytes EncodeConnectRequest(uint16_t srcRef, const std::optional<Bytes>& callingTsap,
	const std::optional<Bytes>& calledTsap, std::optional<uint8_t> tpduSizeCode);
Bytes EncodeConnectConfirm(uint16_t dstRef, uint16_t srcRef, const std::optional<Bytes>& callingTsap,
	const std::optional<Bytes>& calledTsap, std::optional<uint8_t> tpduSizeCode);
Bytes EncodeDisconnectRequest(uint16_t dstRef, uint16_t srcRef, uint8_t reason);
Bytes EncodeData(const uint8_t* data, size_t length, bool eot = true);

//Split a TSDU into DT TPDUs no larger than the negotiated TPDU size; the last has EOT set.
//An empty TSDU yields a single empty DT with EOT. Throws std::invalid_argument for a bad size code.
std::vector<Bytes> Segment(const uint8_t* tsdu, size_t length, uint8_t tpduSizeCode);
inline std::vector<Bytes> Segment(const Bytes& tsdu, uint8_t tpduSizeCode)
{
	return Segment(tsdu.data(), tsdu.size(), tpduSizeCode);
}

//Responder side: choose the TPDU size for the CC, given what the CR proposed
//(nullopt = parameter absent = 128) and the largest size this entity supports.
uint8_t NegotiateTpduSize(std::optional<uint8_t> proposedByCr, uint8_t localMaxCode);

//Initiator side: validate the size in the CC against what the CR proposed (nullopt = we sent
//no parameter, i.e. 128). Absent in the CC means the default, 128 (the safe, smallest choice).
//Returns the size code to use. Throws DecodeError if the CC selects a size larger than proposed
//or an invalid one.
uint8_t ConfirmTpduSize(std::optional<uint8_t> proposedByCr, std::optional<uint8_t> selectedByCc);

//Reassembles DT TPDUs into a TSDU, with a cap on the TSDU size.
class TsduReassembler
{
public:
	static constexpr size_t DefaultMaxTsdu = 1024 * 1024;

	//maxTpduSizeCode: if valid, DTs longer than that TPDU size are rejected (0 disables the check).
	explicit TsduReassembler(size_t maxTsduSize = DefaultMaxTsdu, uint8_t maxTpduSizeCode = 0);

	//Add a DT. Returns true when this DT completed a TSDU, which is then available from Take().
	//Throws DecodeError if not a DT, oversize, or a TSDU is already complete and not yet taken.
	//After a throw the partial TSDU is discarded.
	bool Add(const Tpdu& dt);

	//Move out the completed TSDU (empty if none is complete)
	Bytes Take();

	size_t Pending() const
	{
		return tsdu.size();
	}
	void Reset();

private:
	Bytes tsdu;
	bool complete = false;
	size_t maxTsduSize;
	size_t maxTpduOctets;
};

} // namespace cotp
} // namespace iccp

#endif /* ICCP_COTP_H_ */
