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
 * TLSConf.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  The "TLS" section of a port configuration (TLS on the TCP transport, as in IEC 62351-3). This only parses and checks the shape of the
 *  section, it needs no OpenSSL: the files themselves are checked when the TLS context is created (TLSFilter.h).
 *
 *  The TLS role follows the TCP direction: the TCP initiator is the TLS client, the TCP listener is the TLS server.
 *
 *  The library has no opinion on cipher lists: a TlsConf with empty "ciphers" and "cipherSuites13" leaves OpenSSL's own defaults in
 *  place. A port that wants its own starting values, or named sets of values ("Compatibility" presets), passes them to ParseTlsConf.
 */

#ifndef ODC_TLSCONF_H_
#define ODC_TLSCONF_H_

#include <cstdint>
#include <functional>
#include <json/json.h>
#include <string>
#include <vector>

namespace odc
{
namespace tls
{

//an entry of "PeerCertAllowList": the peer certificate must match at least one
struct AllowEntry
{
	enum class Kind : uint8_t
	{
		CommonName, //"CN:name": a subject common name, exact match
		San,        //"SAN:name": a subject alternative name, a DNS name or an IP address, exact match (no wildcards)
		Sha256      //"SHA256:hex": the SHA-256 fingerprint of the certificate (colons optional)
	};
	Kind kind = Kind::CommonName;
	std::string value; //SHA256: lower case hex without colons
	std::string text;  //as configured, for messages
};

struct TlsConf
{
	bool enabled = false;

	std::string certFile;       //our certificate (PEM, the chain may follow the certificate)
	std::string keyFile;        //our private key (PEM)
	std::string keyPasswordEnv; //the name of an environment variable holding the key's password (never the password itself)
	std::string caFile;         //the CA certificates (PEM bundle) the peer's certificate must chain to
	std::string caPath;         //or a directory of hashed CA certificates (c_rehash)
	std::string crlFile;        //PEM CRL(s): enables revocation checking, fail-closed (no CRL = verification fails)
	bool crlCheckChain = false; //check the CRLs of the intermediate CAs as well, not only the peer certificate's

	bool verifyPeer = true;        //verify the peer certificate (false = accept any: not for production)
	bool requireClientCert = true; //TLS server: a client without a (valid) certificate is refused

	int minVersion = 12; //12 = TLS 1.2, 13 = TLS 1.3
	int maxVersion = 13;
	std::string ciphers;        //TLS 1.2 and below, OpenSSL cipher list syntax; empty = the library's default (OpenSSL's)
	std::string cipherSuites13; //TLS 1.3, OpenSSL syntax; empty = the library's default (OpenSSL's)

	std::vector<AllowEntry> peerCertAllowList; //empty = any certificate that verifies
	bool verifyHostname = false;               //TLS client: the server's certificate must carry serverName (or the connect address) as a SAN
	std::string serverName;                    //TLS client: the name to verify, and to send as SNI if it is a host name; default the connect address

	uint32_t handshakeTimeoutMs = 10000;
	uint32_t renegotiateIntervalSeconds = 0; //0 = no renegotiation at all (a peer's request is refused). The TLS client starts one (TLS 1.2) or a key update (TLS 1.3) this often; the TLS server accepts them from the peer, at most every half interval
	//off by default: for a long lived connection a reconnect is rare and an abbreviated handshake saves nothing worth having, while a resumed
	//session is not checked again against the CA, the CRL or the expiry, and a stack that cannot resume fails only on the reconnect
	bool sessionResumption = false;
	uint32_t certExpiryWarningDays = 30;
	//the TLS session id context (SSL_CTX_set_session_id_context): resumption only works between filters that share it. A port names itself here.
	std::string sessionIdContext = "opendatacon";

	//compatibility with picky or older stacks; the defaults are what the library always did
	std::string groups;                    //SSL_CTX_set1_groups_list syntax ("P-256:P-384"); empty = the library's default (which includes a hybrid key share)
	std::string signatureAlgorithms;       //SSL_CTX_set1_sigalgs_list syntax ("RSA+SHA256:ECDSA+SHA256"), also for client authentication; empty = the library's default
	bool sendSni = true;                   //TLS client: false = never send server_name (it is only sent for a host name anyway, not for an IP address)
	bool extendedMasterSecret = true;      //false: do not offer or accept extended_master_secret (TLS 1.2)
	bool sessionTickets = true;            //false: no session_ticket extension (session id resumption stays, if SessionResumption)
	int securityLevel = -1;                //OpenSSL security level 0 to 5; -1 = the library's default
	std::string compatibility = "Default"; //the preset: "Default" or a name of the table given to ParseTlsConf (explicit keys override it)

	//Diagnostics only: append the TLS secrets (NSS key log format) to this file, for Wireshark to decrypt a capture. Empty = never. Anyone who can
	//read the file can decrypt the traffic: a warning is logged at start-up and on every connection.
	std::string keyLogFile;
};

//True if this build has TLS support (ODC_ASIO_SSL, OpenSSL)
bool TlsSupported();

//A named set of values for the "Compatibility" key. apply adjusts the configuration under construction (which holds the defaults and every
//explicit key already). isGiven(key) tells whether the configuration names that key: a preset must leave alone what the user gave explicitly.
using KeyGiven = std::function<bool (const char*)>;
struct Preset
{
	std::string name; //"Default" is always valid and needs no entry (it changes nothing)
	std::function<void(TlsConf& conf, const KeyGiven& isGiven)> apply;
};
using PresetTable = std::vector<Preset>;

//Parse the "TLS" member of the port configuration root into out. An absent member leaves TLS disabled.
//The problems found are appended to problems, each naming the key. Files are not examined here.
//listener: the port is the TCP listener (so the TLS server).
//defaults: the starting values, copied into out before anything is parsed (the library's own defaults if not given).
//presets: the names "Compatibility" may take besides "Default", in the order they are listed in the message for an unknown name.
void ParseTlsConf(const Json::Value& root, bool listener, TlsConf& out, std::vector<std::string>& problems,
	const TlsConf& defaults = TlsConf(), const PresetTable& presets = PresetTable());

} //namespace tls
} //namespace odc

#endif /* ODC_TLSCONF_H_ */
