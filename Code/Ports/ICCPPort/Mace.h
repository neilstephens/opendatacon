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
 * Mace.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE, the MMS Application Certificate Exchange: authentication in the ACSE association request and response. Both ends send
 *  their X.509 certificate with a signature, in the ACSE authentication fields (the layout is in Acse.h).
 *
 *  The signature is NOT a hash of anything. It is an RSA private-key operation with PKCS#1 v1.5 type 1 padding over a fixed
 *  30 octet DigestInfo header (that of SHA-1: 30 21 30 09 06 05 2b 0e 03 02 1a 05 00 04 14) followed directly by the 15 octets
 *  of the signing time text (the lengths in the header do not match what follows). So it covers the signing time only: it proves
 *  that the sender holds the private key at that time, and it is not bound to the association; the receiver's protection against
 *  replay is the time window. [V: verified with the certificate's public key against both directions of a real peer's association]
 *
 *  A Context holds our certificate and private key and the trust configuration. It is immutable once created and may be shared by
 *  all associations (and threads). It needs OpenSSL (ODC_ASIO_SSL): without it Create fails with a message, and the pure
 *  encoding and decoding of the authentication value (Acse.h) is all that is available.
 */

#ifndef ICCP_MACE_H_
#define ICCP_MACE_H_

#include "Acse.h"
#include "MaceConf.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace iccp
{
namespace mace
{

//The 30 octets that are signed for a signing time ("YYYYMMDDHHMMSSZ"): the SHA-1 DigestInfo header followed by the time text
acse::Bytes SignedBlock(const std::string& signingTime);

//"2026-10-21T04:05:47Z" for logs and status
std::string IsoTime(int64_t unixSeconds);

enum class State : uint8_t
{
	None,     //no authentication (MACE off, or the peer did not authenticate and that is allowed)
	Sent,     //our authentication was sent, the peer's has not been verified (yet)
	Verified, //the peer's authentication was verified
	Failed    //the peer's authentication was refused (or ours could not be made)
};
const char* ToString(State s);

//What GetStatus() reports
struct Status
{
	bool enabled = false;
	State state = State::None;
	std::string peerSubject;
	std::string peerNotAfter;
	int64_t signingTime = 0;     //our signing time of the current (or last) association, 0 = none yet
	int64_t peerSigningTime = 0; //the peer's, 0 = none
	std::string lastError;
};

//What was learnt about the peer from a valid authentication
struct PeerInfo
{
	std::string subject;
	std::string notAfter; //ISO time
	int64_t signingTime = 0;
	std::string mechanismName;      //the mechanism-name the peer sent, as dotted text
	bool mechanismOfficial = false; //it is the one IEC 62351-4 defines (11.1.4.3)
	bool mechanismDiffers = false;  //it is not the same as ours: accepted all the same
};

//Why an authentication was refused: the ACSE diagnostic to send (acse::UserDiagnostic) and the text for our own log
struct Failure
{
	int64_t diagnostic = acse::UserDiagnostic::AuthenticationFailure;
	std::string text;
};

//What Sign() makes
struct Signed
{
	acse::Authentication authentication; //acse-requirements, mechanism-name and the value, for the AARQ or the AARE
	std::string subject;                 //our certificate's subject
	int64_t signingTime = 0;
};

class Context
{
public:
	//Load the certificate, the key and the trust configuration; checks that the key is RSA, within the size limits and matches
	//the certificate. nullptr and error (naming the key) on any problem, and always without OpenSSL.
	static std::shared_ptr<Context> Create(const MaceConf& conf, std::string& error);
	~Context();
	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;

	const MaceConf& Conf() const;
	//Things to warn about when the port starts: a disabled chain validation, a certificate that expires soon
	const std::vector<std::string>& Warnings() const;
	//our certificate's subject
	const std::string& OwnSubject() const;

	//Make our authentication for the signing time nowUtc (seconds since the epoch, UTC): a fresh signature on every call. The mechanism-name
	//is ours; the caller may replace it (the responder echoes a different one it received). When the A-security abstract syntax was
	//agreed, authContextId is its presentation context identifier, which then goes into the value as its identification component.
	bool Sign(int64_t nowUtc, Signed& out, std::string& error, std::optional<int64_t> authContextId = std::nullopt) const;

	//Check the authentication a peer sent (the fields of the AARQ or AARE) against the clock nowUtc: the presence of the mechanism-name
	//(its value is not compared: a different one is tolerated and reported in peer), the structure,
	//the certificate (key type and size, chain, validity period at nowUtc, revocation, allow list), the signature and the time window.
	//On success fills peer; on failure fills failure and returns false.
	bool Verify(const acse::Authentication& received, int64_t nowUtc, PeerInfo& peer, Failure& failure) const;

private:
	Context();
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} //namespace mace
} //namespace iccp

#endif /* ICCP_MACE_H_ */
