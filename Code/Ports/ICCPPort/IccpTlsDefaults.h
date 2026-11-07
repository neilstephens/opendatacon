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
 * IccpTlsDefaults.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  What is ICCP specific about the TLS of this port (secure ICCP, IEC 62351-3). The generic parts are in the ODC library (TLSFilter.h, TLSConf.h,
 *  TCPSessionManager.h); this port gives them:
 *   - the default port,
 *   - the starting values of the "TLS" section (the default cipher lists, the session id context),
 *   - the "Compatibility" presets ("Legacy12").
 *
 *  The role of TLS follows the TCP direction: the TCP initiator (ICCPClientPort) is the TLS client, the TCP listener (ICCPServerPort) is the
 *  TLS server. It is independent of the TASE.2 service role.
 */

#ifndef ICCP_ICCPTLSDEFAULTS_H_
#define ICCP_ICCPTLSDEFAULTS_H_

#include <opendatacon/TLSConf.h>
#include <opendatacon/TLSFilter.h>

namespace iccp
{
//the port's code says tls::Filter, tls::Context, ...
namespace tls = odc::tls;

//the TCP port conventionally used for secure ICCP (IEC 62351-3); the default when TLS is enabled and "Port" is not given
constexpr const char* DefaultSecurePort = "3782";

//TLS 1.2 suites with forward secrecy and AEAD (the IEC 62351-3 profile as far as it is known) [S]
constexpr const char* DefaultCiphers12 = "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:"
                                         "ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256";
constexpr const char* DefaultCipherSuites13 = "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256";
//The "Legacy12" compatibility preset: TLS 1.2 suites for an older stack, RSA-based first (ECDHE-RSA, DHE-RSA, then plain RSA key exchange),
//AES-GCM before AES-CBC with SHA-256 / SHA-1 MACs [S]
constexpr const char* LegacyCiphers12 = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:"
                                        "AES256-GCM-SHA384:AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256:"
                                        "DHE-RSA-AES128-SHA256:AES256-SHA256:AES128-SHA256:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:"
                                        "DHE-RSA-AES128-SHA:AES256-SHA:AES128-SHA";
constexpr const char* LegacyGroups = "P-256:P-384:P-521";

//the TLS session id context of this port: resumption only works between ICCP ports
constexpr const char* TlsSessionIdContext = "opendatacon-iccp";

//the starting values of the "TLS" section of an ICCP port: the library's defaults with the cipher lists above
inline tls::TlsConf IccpTlsDefaults()
{
	tls::TlsConf c;
	c.ciphers = DefaultCiphers12;
	c.cipherSuites13 = DefaultCipherSuites13;
	c.sessionIdContext = TlsSessionIdContext;
	return c;
}

//the "Compatibility" presets of an ICCP port, besides "Default"
inline void ApplyLegacy12(tls::TlsConf& c, const tls::KeyGiven& isGiven)
{
	//fills in what the configuration does not say: TLS 1.2 only, no tickets, no SNI, the NIST curves only, RSA-based suites
	if(!isGiven("MinVersion"))
		c.minVersion = 12;
	if(!isGiven("MaxVersion"))
		c.maxVersion = 12;
	if(!isGiven("SessionTickets"))
		c.sessionTickets = false;
	if(!isGiven("SendSNI"))
		c.sendSni = false;
	if(!isGiven("Groups"))
		c.groups = LegacyGroups;
	if(!isGiven("Ciphers"))
		c.ciphers = LegacyCiphers12;
}

inline const tls::PresetTable& IccpTlsPresets()
{
	static const tls::PresetTable table = {
		{"Legacy12", ApplyLegacy12}
	};
	return table;
}
} //namespace iccp

#endif /* ICCP_ICCPTLSDEFAULTS_H_ */
