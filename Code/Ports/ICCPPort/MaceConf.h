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
 * MaceConf.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The "MACE" section of the ICCP port configuration: authentication in the ACSE association (MMS Application Certificate
 *  Exchange). It is independent of TLS and of the TASE.2 service role, and valid for both port types. This only parses and
 *  checks the shape of the section, it needs no OpenSSL: the files are checked when the MACE context is created (Mace.h).
 */

#ifndef ICCP_MACECONF_H_
#define ICCP_MACECONF_H_

#include "Acse.h"
#include "IccpTlsDefaults.h"
#include <cstdint>
#include <json/json.h>
#include <string>
#include <vector>

namespace iccp
{
namespace mace
{

//the RSA key sizes accepted by default; the upper limit is the one a real-world peer documents for MACE, never exceeded
constexpr uint32_t DefaultMinKeyBits = 2048;
constexpr uint32_t DefaultMaxKeyBits = 8192;
constexpr uint32_t AbsoluteMaxKeyBits = 8192;
constexpr uint32_t DefaultTimeWindowSeconds = 600;

//The authentication mechanism-name object identifiers of IEC 62351-4 clause 11.1.4.3 [T]:
//- the official one, version1-mechanism = { auth-mechanism 1 } = 1.0.62351.4.3.1 (DER 28 83 E7 0F 04 03 01): the default
//- the one that implementations of IEC TS 62351-4:2007 are known to use, 1.0.840.0.1.0.1.1 (DER 28 86 48 00 01 00 01 01). The
//  standard calls it an illegal identifier, but allows using it to talk to such a system, and asks receivers to be tolerant of it.
inline const acse::Oid OfficialMechanismName{1, 0, 62351, 4, 3, 1};
inline const acse::Oid LegacyMechanismName{1, 0, 840, 0, 1, 0, 1, 1};
constexpr const char* OfficialMechanismText = "1.0.62351.4.3.1";
constexpr const char* LegacyMechanismText = "1.0.840.0.1.0.1.1";

//An object identifier as dotted text ("1.0.62351.4.3.1")
std::string OidToString(const acse::Oid& oid);

struct MaceConf
{
	bool enabled = false;

	//the authentication mechanism-name OID carried in the ACSE APDU (optional in the configuration: the official one by default).
	//What a peer sends is accepted whatever it is (IEC 62351-4 11.1.4.3: it does not affect the procedure), but it must be present.
	std::string mechanismText = OfficialMechanismText;
	acse::Oid mechanismName = OfficialMechanismName;

	std::string certFile;       //our certificate (PEM); the DER of the first certificate in the file is what is sent
	std::string keyFile;        //our private key (PEM, RSA)
	std::string keyPasswordEnv; //the name of an environment variable holding the key's password (never the password itself)
	std::string caFile;         //trust anchors (and intermediate CAs: only the leaf certificate is sent by the peer) for the peer's certificate
	std::string caPath;         //or a directory of hashed CA certificates (c_rehash)
	std::string crlFile;        //PEM CRL(s): enables revocation checking, fail-closed
	bool crlCheckChain = false;
	std::vector<tls::AllowEntry> peerCertAllowList; //CN:, SAN: or SHA256: as for TLS; empty = any certificate that verifies

	uint32_t timeWindowSeconds = DefaultTimeWindowSeconds; //|now - signing time| must not exceed this
	uint32_t minKeyBits = DefaultMinKeyBits;
	uint32_t maxKeyBits = DefaultMaxKeyBits;

	bool requireAuthentication = true;          //responder: refuse an association request without a valid authentication value
	bool requireResponderAuthentication = true; //initiator: abort when the response carries no (or no valid) authentication
	bool verifyPeer = true;                     //false: do not validate the chain (the signature and the time are still checked): for testing only
};

//True if this build has MACE support (ODC_ASIO_SSL, OpenSSL)
bool MaceSupported();

//Parse the "MACE" member of the port configuration root into out. An absent member leaves MACE disabled. The problems found are
//appended to problems, each naming the key. Files are not examined here.
void ParseMaceConf(const Json::Value& root, MaceConf& out, std::vector<std::string>& problems);

} //namespace mace
} //namespace iccp

#endif /* ICCP_MACECONF_H_ */
