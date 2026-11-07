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
 * MaceTestPki.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Test support for the MACE tests: a throw-away PKI made at run time (RSA 2048 and 3072 node certificates, an EC one, expired, not yet
 *  valid, untrusted, revoked), written to a temporary directory. Synthetic names; nothing here is a real key or certificate.
 *  Needs OpenSSL (ODC_ASIO_SSL).
 */

#ifndef ICCP_MACETESTPKI_H_
#define ICCP_MACETESTPKI_H_

#ifdef ODC_ASIO_SSL

#include "Mace.h"
#include "TlsTestPki.h"
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>

namespace macetest
{

constexpr const char* Mechanism = "1.3.9999.7.1"; //synthetic

inline int64_t RealNow()
{
	return static_cast<int64_t>(std::time(nullptr));
}

struct Files
{
	std::string cert, key;
};

struct Pki
{
	tlstest::TempDir dir;
	tlstest::Cert ca, otherCa;
	tlstest::Cert nodeA, nodeB, expired, notYet, untrusted, revoked, rsa3072, ec;
	Files a, b, expiredF, notYetF, untrustedF, revokedF, rsa3072F, ecF;
	std::string caFile, otherCaFile, crlFile;

	static tlstest::CertSpec Node(const std::string& cn, long serial, int bits = 2048)
	{
		tlstest::CertSpec s;
		s.cn = cn;
		s.sans = {"DNS:" + cn};
		s.serial = serial;
		s.rsaBits = bits;
		return s;
	}
	Files Save(const std::string& name, const tlstest::Cert& c)
	{
		return {dir.Write(name + ".crt", c.certPem), dir.Write(name + ".key", c.keyPem)};
	}
	Pki()
	{
		{
			tlstest::CertSpec s;
			s.cn = "MACE Test CA";
			s.ca = true;
			s.notAfter = 86400LL * 3650;
			ca = tlstest::MakeCert(s);
			s.cn = "Other MACE Test CA";
			otherCa = tlstest::MakeCert(s);
		}
		nodeA = tlstest::MakeCert(Node("mace-a.test", 10), &ca);
		nodeB = tlstest::MakeCert(Node("mace-b.test", 11), &ca);
		{
			auto s = Node("mace-expired.test", 12);
			s.notBefore = -86400 * 30;
			s.notAfter = -3600;
			expired = tlstest::MakeCert(s, &ca);
			s = Node("mace-notyet.test", 13);
			s.notBefore = 86400;
			notYet = tlstest::MakeCert(s, &ca);
		}
		untrusted = tlstest::MakeCert(Node("mace-untrusted.test", 14), &otherCa);
		revoked = tlstest::MakeCert(Node("mace-revoked.test", 15), &ca);
		rsa3072 = tlstest::MakeCert(Node("mace-3072.test", 16, 3072), &ca);
		{
			auto s = Node("mace-ec.test", 17, 0);
			ec = tlstest::MakeCert(s, &ca);
		}
		a = Save("a", nodeA);
		b = Save("b", nodeB);
		expiredF = Save("expired", expired);
		notYetF = Save("notyet", notYet);
		untrustedF = Save("untrusted", untrusted);
		revokedF = Save("revoked", revoked);
		rsa3072F = Save("rsa3072", rsa3072);
		ecF = Save("ec", ec);
		caFile = dir.Write("ca.crt", ca.certPem);
		otherCaFile = dir.Write("otherca.crt", otherCa.certPem);
		crlFile = dir.Write("ca.crl", tlstest::MakeCrl(ca, {15}));
	}
};

inline Pki& GetPki()
{
	static Pki pki;
	return pki;
}

//a MACE configuration for a node that trusts caFile
inline iccp::mace::MaceConf Conf(const Files& id, const std::string& caFile)
{
	iccp::mace::MaceConf c;
	c.enabled = true;
	c.mechanismText = Mechanism;
	c.mechanismName = {1, 3, 9999, 7, 1};
	c.certFile = id.cert;
	c.keyFile = id.key;
	c.caFile = caFile;
	return c;
}

inline std::shared_ptr<iccp::mace::Context> Make(const iccp::mace::MaceConf& conf)
{
	std::string error;
	auto c = iccp::mace::Context::Create(conf, error);
	if(!c)
		throw std::runtime_error("test MACE context: " + error);
	return c;
}

inline void SetEnv(const std::string& name, const std::string& value)
{
	#ifdef _WIN32
	_putenv_s(name.c_str(), value.c_str());
	#else
	setenv(name.c_str(), value.c_str(), 1);
	#endif
}

} //namespace macetest

#endif // ODC_ASIO_SSL

#endif /* ICCP_MACETESTPKI_H_ */
