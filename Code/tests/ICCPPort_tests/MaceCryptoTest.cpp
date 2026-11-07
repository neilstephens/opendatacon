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
 * MaceCryptoTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE: the signature scheme and the verification rules, with certificates made at run time (ODC_ASIO_SSL only), and the optional
 *  local check against a real peer's capture (ICCP_MACE_CAPTURE_DIR, see MaceTest.cpp). Synthetic values only.
 */

#ifdef ODC_ASIO_SSL

#include "Acse.h"
#include "Mace.h"
#include "MaceTestPki.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <openssl/rsa.h>

#define SUITE(name) "ICCP MACE crypto - " name

using namespace iccp;
using ber::Bytes;
using macetest::Conf;
using macetest::GetPki;
using macetest::Make;
using macetest::RealNow;

namespace
{
const acse::Oid MechanismOid{1, 3, 9999, 7, 1};

Bytes CertDer(const tlstest::Cert& c)
{
	Bytes der(static_cast<size_t>(i2d_X509(c.x509.get(), nullptr)));
	unsigned char* p = der.data();
	i2d_X509(c.x509.get(), &p);
	return der;
}

//the DER of an object identifier, tag and length included
Bytes Der(const acse::Oid& oid)
{
	ber::Writer w;
	w.ObjectIdentifier(oid);
	return w.Release();
}

//what a signed authentication is made of
struct Parts
{
	Bytes cert;
	std::string time;
	Bytes signature;
};

Parts PartsOf(const acse::Authentication& a)
{
	const acse::MaceAuthValue v = acse::DecodeMaceAuthValue(*a.authenticationValue);
	return {Bytes(v.certificate, v.certificate + v.certificateLength), v.SigningTimeText(), Bytes(v.signature, v.signature + v.signatureLength)};
}

acse::Authentication Build(const Parts& p, const acse::Oid& mechanism = MechanismOid)
{
	return acse::MakeMaceAuthentication(mechanism, acse::EncodeMaceAuthValue(p.cert, p.time, p.signature));
}

acse::Authentication SignAt(const mace::Context& ctx, int64_t now)
{
	mace::Signed s;
	std::string error;
	REQUIRE(ctx.Sign(now, s, error));
	return s.authentication;
}

struct Result
{
	bool ok;
	mace::PeerInfo peer;
	mace::Failure failure;
};

Result Check(const mace::Context& verifier, const acse::Authentication& a, int64_t now)
{
	Result r;
	r.ok = verifier.Verify(a, now, r.peer, r.failure);
	return r;
}

//expect a refusal with the diagnostic and the text
void RequireRefused(const Result& r, int64_t diagnostic, const std::string& part)
{
	INFO(r.failure.text);
	REQUIRE_FALSE(r.ok);
	REQUIRE(r.failure.diagnostic == diagnostic);
	REQUIRE(r.failure.text.find(part) != std::string::npos);
	REQUIRE(r.failure.text.rfind("MACE: ", 0) == 0);
}

std::string FingerprintOf(const tlstest::Cert& c)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int n = 0;
	X509_digest(c.x509.get(), EVP_sha256(), md, &n);
	static const char d[] = "0123456789abcdef";
	std::string s;
	for(unsigned i = 0; i < n; ++i)
	{
		s.push_back(d[md[i] >> 4]);
		s.push_back(d[md[i] & 15]);
	}
	return s;
}

//the RSA public operation of the certificate's key on a signature, PKCS#1 type 1: the payload
Bytes Recover(const tlstest::Cert& c, const Bytes& signature)
{
	EVP_PKEY* pub = X509_get0_pubkey(c.x509.get());
	EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pub, nullptr);
	REQUIRE(ctx != nullptr);
	REQUIRE(EVP_PKEY_verify_recover_init(ctx) > 0);
	REQUIRE(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0);
	Bytes out(static_cast<size_t>(EVP_PKEY_size(pub)));
	size_t n = out.size();
	REQUIRE(EVP_PKEY_verify_recover(ctx, out.data(), &n, signature.data(), signature.size()) > 0);
	EVP_PKEY_CTX_free(ctx);
	out.resize(n);
	return out;
}
}

TEST_CASE(SUITE("Sign and verify round trip; the signed block is the DigestInfo header and the time"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();

	mace::Signed s;
	std::string error;
	REQUIRE(a->Sign(now, s, error));
	REQUIRE(s.subject.find("mace-a.test") != std::string::npos);
	REQUIRE(s.signingTime == now);
	REQUIRE(s.authentication.acseRequirements == acse::AcseRequirement::Authentication);
	REQUIRE(s.authentication.mechanismName == MechanismOid);
	REQUIRE(a->OwnSubject() == s.subject);

	const Result r = Check(*b, s.authentication, now);
	REQUIRE(r.ok);
	REQUIRE(r.peer.subject == s.subject);
	REQUIRE(r.peer.signingTime == now);
	REQUIRE_FALSE(r.peer.notAfter.empty());

	//what is on the wire: our certificate (DER, leaf only), the time, a signature of the key's size
	const Parts parts = PartsOf(s.authentication);
	REQUIRE(parts.cert == CertDer(pki.nodeA));
	REQUIRE(parts.time == acse::FormatGeneralizedTime(now));
	REQUIRE(parts.signature.size() == 256);

	//the public operation gives the PKCS#1 type 1 payload: 30 octets, the SHA-1 DigestInfo header then the time text itself
	const Bytes block = Recover(pki.nodeA, parts.signature);
	REQUIRE(block.size() == 30);
	REQUIRE(Bytes(block.begin(), block.begin() + 15) == Bytes({0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14}));
	REQUIRE(Bytes(block.begin() + 15, block.end()) == Bytes(parts.time.begin(), parts.time.end()));
	REQUIRE(block == mace::SignedBlock(parts.time));

	//a new time is a new signature; the same time gives the same one (PKCS#1 v1.5 is deterministic)
	REQUIRE(PartsOf(SignAt(*a, now + 1)).signature != parts.signature);
	REQUIRE(PartsOf(SignAt(*a, now)).signature == parts.signature);

	//both directions
	REQUIRE(Check(*a, SignAt(*b, now), now).ok);

	//a 3072 bit key is inside the default limits
	const auto big = Make(Conf(pki.rsa3072F, pki.caFile));
	const Parts bigParts = PartsOf(SignAt(*big, now));
	REQUIRE(bigParts.signature.size() == 384);
	REQUIRE(Check(*b, SignAt(*big, now), now).ok);
}

TEST_CASE(SUITE("Wrong key, tampered time, tampered signature, a signature made for another time"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();
	const Parts good = PartsOf(SignAt(*a, now));
	REQUIRE(Check(*b, Build(good), now).ok);

	{ //A's signature under B's certificate
		Parts p = good;
		p.cert = CertDer(pki.nodeB);
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	}
	{ //the time changed, the signature not
		Parts p = good;
		p.time = acse::FormatGeneralizedTime(now + 1);
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	}
	for(size_t i : {size_t(0), size_t(100), size_t(255)})
	{
		Parts p = good;
		p.signature[i] ^= 0x01;
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	}
	{ //a signature made for another (older) time, presented with the current time
		Parts p = good;
		p.signature = PartsOf(SignAt(*a, now - 10)).signature;
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	}
	{ //a shorter or longer signature
		Parts p = good;
		p.signature.pop_back();
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
		p.signature = good.signature;
		p.signature.push_back(0);
		RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	}
	{ //the time text is not a time
		acse::Authentication auth;
		auth.mechanismName = MechanismOid;
		auth.authenticationValue = acse::EncodeMaceAuthValue(good.cert, "20260102030405Z", good.signature);
		//(the encoder refuses a bad time, so a hand-made value: change a time octet in place)
		Bytes& v = *auth.authenticationValue;
		const std::string wanted = "20260102030405Z";
		auto pos = std::search(v.begin(), v.end(), wanted.begin(), wanted.end());
		REQUIRE(pos != v.end());
		*pos = 'x';
		RequireRefused(Check(*b, auth, now), acse::UserDiagnostic::AuthenticationFailure, "not a GeneralizedTime");
	}
}

TEST_CASE(SUITE("The time window: replay, future, edges, and the clock of the check"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();
	const acse::Authentication auth = SignAt(*a, now);

	RequireRefused(Check(*b, auth, now + 700), acse::UserDiagnostic::AuthenticationFailure, "signing time outside the window");
	const Result late = Check(*b, auth, now + 700);
	REQUIRE(late.failure.text.find("the peer's clock differs by 700 s") != std::string::npos);
	const Result early = Check(*b, auth, now - 700);
	RequireRefused(early, acse::UserDiagnostic::AuthenticationFailure, "signing time outside the window");
	REQUIRE(early.failure.text.find("differs by -700 s") != std::string::npos);

	REQUIRE(Check(*b, auth, now + 600).ok);
	REQUIRE_FALSE(Check(*b, auth, now + 601).ok);
	REQUIRE(Check(*b, auth, now - 600).ok);
	REQUIRE_FALSE(Check(*b, auth, now - 601).ok);

	//a different window
	auto narrow = Conf(pki.b, pki.caFile);
	narrow.timeWindowSeconds = 5;
	const auto n = Make(narrow);
	REQUIRE(Check(*n, auth, now + 5).ok);
	REQUIRE_FALSE(Check(*n, auth, now + 6).ok);

	//the certificate's validity is judged at the clock given, not at the real time: signed and checked two years on, it has expired
	const int64_t later = now + 2 * 365 * 86400;
	RequireRefused(Check(*b, SignAt(*a, later), later), acse::UserDiagnostic::AuthenticationFailure, "certificate has expired");
}

TEST_CASE(SUITE("The mechanism-name and the presence of the value"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();
	const acse::Authentication good = SignAt(*a, now);

	//a different mechanism-name is tolerated (IEC 62351-4 11.1.4.3), and reported
	acse::Authentication other = good;
	other.mechanismName = acse::Oid{1, 3, 9999, 7, 2};
	{
		const Result r = Check(*b, other, now);
		REQUIRE(r.ok);
		REQUIRE(r.peer.mechanismDiffers);
		REQUIRE(r.peer.mechanismName == "1.3.9999.7.2");
		const Result same = Check(*b, good, now);
		REQUIRE(same.ok);
		REQUIRE_FALSE(same.peer.mechanismDiffers);
		REQUIRE(same.peer.mechanismName == "1.3.9999.7.1");
	}
	acse::Authentication none = good;
	none.mechanismName.reset();
	RequireRefused(Check(*b, none, now), acse::UserDiagnostic::AuthenticationMechanismNameRequired, "without a mechanism-name");
	acse::Authentication empty;
	RequireRefused(Check(*b, empty, now), acse::UserDiagnostic::AuthenticationRequired, "no authentication value");

	//the verifier has another mechanism configured: still accepted
	auto conf = Conf(pki.b, pki.caFile);
	conf.mechanismName = {1, 3, 9999, 7, 2};
	conf.mechanismText = "1.3.9999.7.2";
	{
		const Result r = Check(*Make(conf), good, now);
		REQUIRE(r.ok);
		REQUIRE(r.peer.mechanismDiffers);
		REQUIRE(r.peer.mechanismName == "1.3.9999.7.1");
	}
}

TEST_CASE(SUITE("The official and the legacy mechanism-names, in both directions"))
{
	auto& pki = GetPki();
	const int64_t now = RealNow();

	//the two object identifiers of IEC 62351-4 11.1.4.3, in DER
	REQUIRE(Der(mace::OfficialMechanismName) == Bytes({0x06, 0x07, 0x28, 0x83, 0xE7, 0x0F, 0x04, 0x03, 0x01}));
	REQUIRE(Der(mace::LegacyMechanismName) == Bytes({0x06, 0x08, 0x28, 0x86, 0x48, 0x00, 0x01, 0x00, 0x01, 0x01}));
	REQUIRE(mace::OidToString(mace::OfficialMechanismName) == mace::OfficialMechanismText);
	REQUIRE(mace::OidToString(mace::LegacyMechanismName) == mace::LegacyMechanismText);

	auto official = Conf(pki.a, pki.caFile);
	official.mechanismName = mace::OfficialMechanismName;
	official.mechanismText = mace::OfficialMechanismText;
	auto legacy = Conf(pki.b, pki.caFile);
	legacy.mechanismName = mace::LegacyMechanismName;
	legacy.mechanismText = mace::LegacyMechanismText;
	const auto o = Make(official);
	const auto l = Make(legacy);

	const acse::Authentication fromOfficial = SignAt(*o, now);
	const acse::Authentication fromLegacy = SignAt(*l, now);
	REQUIRE(fromOfficial.mechanismName == mace::OfficialMechanismName);
	REQUIRE(fromLegacy.mechanismName == mace::LegacyMechanismName);

	const Result a = Check(*o, fromLegacy, now);
	REQUIRE(a.ok);
	REQUIRE(a.peer.mechanismDiffers);
	REQUIRE_FALSE(a.peer.mechanismOfficial);
	REQUIRE(a.peer.mechanismName == mace::LegacyMechanismText);
	const Result b = Check(*l, fromOfficial, now);
	REQUIRE(b.ok);
	REQUIRE(b.peer.mechanismDiffers);
	REQUIRE(b.peer.mechanismOfficial);
	REQUIRE(b.peer.mechanismName == mace::OfficialMechanismText);
	REQUIRE_FALSE(Check(*o, fromOfficial, now).peer.mechanismDiffers);
	REQUIRE(Check(*o, fromOfficial, now).peer.mechanismOfficial);
}

TEST_CASE(SUITE("The A-security identification component of the authentication value"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();

	mace::Signed s;
	std::string error;
	REQUIRE(a->Sign(now, s, error, 5));
	const Bytes& v = *s.authentication.authenticationValue;
	//[2] { INTEGER 5, [0] { [0] { ... } } }
	const ber::Element outer = ber::ParseElement(v.data(), v.size());
	REQUIRE(outer.tag.Is(ber::TagClass::ContextSpecific, 2));
	REQUIRE(Bytes(outer.content, outer.content + 3) == Bytes({0x02, 0x01, 0x05}));
	REQUIRE(Check(*b, s.authentication, now).ok);

	//the form without it is unchanged
	const acse::Authentication plain = SignAt(*a, now);
	REQUIRE(plain.authenticationValue->size() + 3 == v.size());
	REQUIRE(Check(*b, plain, now).ok);
}

TEST_CASE(SUITE("Malformed authentication values are refused"))
{
	auto& pki = GetPki();
	const auto a = Make(Conf(pki.a, pki.caFile));
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();
	const Parts good = PartsOf(SignAt(*a, now));

	acse::Authentication junk;
	junk.mechanismName = MechanismOid;
	junk.authenticationValue = Bytes{0x01, 0x02, 0x03};
	RequireRefused(Check(*b, junk, now), acse::UserDiagnostic::AuthenticationFailure, "malformed");

	Parts p = good;
	p.cert = Bytes(200, 0x5A);
	RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "not a valid DER X.509");
	p.cert = good.cert;
	p.cert.push_back(0); //trailing octets after the certificate
	RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "not a valid DER X.509");
	p.cert = Bytes(good.cert.begin(), good.cert.begin() + 100);
	RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "not a valid DER X.509");
	p.cert = Bytes(70000, 1);
	RequireRefused(Check(*b, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "implausibly large");
}

TEST_CASE(SUITE("The certificate is validated: expiry, trust, revocation"))
{
	auto& pki = GetPki();
	const auto b = Make(Conf(pki.b, pki.caFile));
	const int64_t now = RealNow();

	RequireRefused(Check(*b, SignAt(*Make(Conf(pki.expiredF, pki.caFile)), now), now), acse::UserDiagnostic::AuthenticationFailure, "certificate has expired");
	RequireRefused(Check(*b, SignAt(*Make(Conf(pki.notYetF, pki.caFile)), now), now), acse::UserDiagnostic::AuthenticationFailure, "not yet valid");
	{
		const Result r = Check(*b, SignAt(*Make(Conf(pki.untrustedF, pki.otherCaFile)), now), now);
		RequireRefused(r, acse::UserDiagnostic::AuthenticationFailure, "is not acceptable");
		REQUIRE(r.failure.text.find("mace-untrusted.test") != std::string::npos);
	}
	//a certificate that chains to a CA we do not trust, with that CA's certificate not even given
	RequireRefused(Check(*b, SignAt(*Make(Conf(pki.untrustedF, pki.otherCaFile)), now), now), acse::UserDiagnostic::AuthenticationFailure, "unable to get local issuer certificate");

	//the CRL: the revoked certificate is refused, the others are still accepted
	auto crl = Conf(pki.b, pki.caFile);
	crl.crlFile = pki.crlFile;
	const auto withCrl = Make(crl);
	RequireRefused(Check(*withCrl, SignAt(*Make(Conf(pki.revokedF, pki.caFile)), now), now), acse::UserDiagnostic::AuthenticationFailure, "certificate revoked");
	REQUIRE(Check(*withCrl, SignAt(*Make(Conf(pki.a, pki.caFile)), now), now).ok);
	REQUIRE(Check(*b, SignAt(*Make(Conf(pki.revokedF, pki.caFile)), now), now).ok); //(no CRL configured)
}

TEST_CASE(SUITE("VerifyPeer false skips the chain but not the signature or the time"))
{
	auto& pki = GetPki();
	auto conf = Conf(pki.b, "");
	conf.verifyPeer = false;
	const auto lax = Make(conf);
	REQUIRE_FALSE(lax->Warnings().empty());
	REQUIRE(lax->Warnings()[0].find("not validated") != std::string::npos);
	const int64_t now = RealNow();

	for(const macetest::Files* f : {&pki.untrustedF, &pki.expiredF, &pki.notYetF, &pki.revokedF})
		REQUIRE(Check(*lax, SignAt(*Make(Conf(*f, pki.caFile)), now), now).ok);

	const auto a = Make(Conf(pki.untrustedF, pki.caFile));
	Parts p = PartsOf(SignAt(*a, now));
	p.signature[3] ^= 0x10;
	RequireRefused(Check(*lax, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "signature is not valid");
	RequireRefused(Check(*lax, SignAt(*a, now), now + 1000), acse::UserDiagnostic::AuthenticationFailure, "outside the window");
}

TEST_CASE(SUITE("The allow list"))
{
	auto& pki = GetPki();
	const int64_t now = RealNow();
	const auto signerA = Make(Conf(pki.a, pki.caFile));
	const acse::Authentication fromA = SignAt(*signerA, now);
	auto verifier = [&](const std::string& entry)
			    {
				    auto conf = Conf(pki.b, pki.caFile);
				    tls::AllowEntry e;
				    e.text = entry;
				    const std::string value = entry.substr(entry.find(':') + 1);
				    e.kind = entry.rfind("CN:", 0) == 0 ? tls::AllowEntry::Kind::CommonName :
				             (entry.rfind("SAN:", 0) == 0 ? tls::AllowEntry::Kind::San : tls::AllowEntry::Kind::Sha256);
				    e.value = value;
				    conf.peerCertAllowList.push_back(e);
				    return Make(conf);
			    };
	REQUIRE(Check(*verifier("CN:mace-a.test"), fromA, now).ok);
	REQUIRE(Check(*verifier("SAN:mace-a.test"), fromA, now).ok);
	REQUIRE(Check(*verifier("SHA256:" + FingerprintOf(pki.nodeA)), fromA, now).ok);
	RequireRefused(Check(*verifier("CN:mace-b.test"), fromA, now), acse::UserDiagnostic::AuthenticationFailure, "not in PeerCertAllowList");
	RequireRefused(Check(*verifier("SAN:other.test"), fromA, now), acse::UserDiagnostic::AuthenticationFailure, "not in PeerCertAllowList");
	RequireRefused(Check(*verifier("SHA256:" + FingerprintOf(pki.nodeB)), fromA, now), acse::UserDiagnostic::AuthenticationFailure, "not in PeerCertAllowList");
}

TEST_CASE(SUITE("Key size limits and key types of the peer"))
{
	auto& pki = GetPki();
	const int64_t now = RealNow();
	const acse::Authentication from2048 = SignAt(*Make(Conf(pki.a, pki.caFile)), now);
	const acse::Authentication from3072 = SignAt(*Make(Conf(pki.rsa3072F, pki.caFile)), now);

	{ //MaxKeyBits lowered to 2048: a 3072 bit peer key is refused (no 8192 bit key is needed to test the bound)
		auto conf = Conf(pki.b, pki.caFile);
		conf.maxKeyBits = 2048;
		const auto v = Make(conf);
		REQUIRE(Check(*v, from2048, now).ok);
		RequireRefused(Check(*v, from3072, now), acse::UserDiagnostic::AuthenticationFailure, "3072 bits, outside the accepted 2048 to 2048");
	}
	{ //MinKeyBits raised to 3072: a 2048 bit peer key is refused
		auto conf = Conf(pki.rsa3072F, pki.caFile);
		conf.minKeyBits = 3072;
		const auto v = Make(conf);
		REQUIRE(Check(*v, from3072, now).ok);
		RequireRefused(Check(*v, from2048, now), acse::UserDiagnostic::AuthenticationFailure, "2048 bits, outside the accepted 3072 to 8192");
	}
	{ //an EC certificate is refused clearly (a signature with the right layout cannot exist)
		const auto v = Make(Conf(pki.b, pki.caFile));
		Parts p;
		p.cert = CertDer(pki.ec);
		p.time = acse::FormatGeneralizedTime(now);
		p.signature = Bytes(64, 0x11);
		RequireRefused(Check(*v, Build(p), now), acse::UserDiagnostic::AuthenticationFailure, "not an RSA key");
	}
}

TEST_CASE(SUITE("A local check against a real peer's capture"))
{
	//Off unless ICCP_MACE_CAPTURE_DIR names a directory with aarq.bin and aare.bin (the ACSE APDUs of one association). Prints counts only.
	std::string dir;
	{
		#ifdef _MSC_VER
		char* buf = nullptr;
		size_t len = 0;
		if(_dupenv_s(&buf, &len, "ICCP_MACE_CAPTURE_DIR") == 0 && buf != nullptr)
		{
			dir = buf;
			std::free(buf);
		}
		#else
		if(const char* e = std::getenv("ICCP_MACE_CAPTURE_DIR"))
			dir = e;
		#endif
	}
	if(dir.empty())
		return;
	auto read = [&](const char* name)
			{
				std::ifstream f(dir + "/" + name, std::ios::binary);
				REQUIRE(f.good());
				return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			};
	auto& pki = GetPki();

	int decoded = 0, verified = 0, reencoded = 0, skeletons = 0;
	for(bool isAarq : {true, false})
	{
		const Bytes apdu = read(isAarq ? "aarq.bin" : "aare.bin");
		const acse::Authentication auth = isAarq ? acse::DecodeAarq(apdu).authentication : acse::DecodeAare(apdu).authentication;
		REQUIRE(auth.authenticationValue.has_value());
		REQUIRE(auth.mechanismName.has_value());
		REQUIRE(auth.acseRequirements == acse::AcseRequirement::Authentication);
		const acse::MaceAuthValue value = acse::DecodeMaceAuthValue(*auth.authenticationValue);
		++decoded;

		//byte exact re-encoding of the whole APDU
		const bool same = isAarq ? acse::Encode(acse::DecodeAarq(apdu)) == apdu : acse::Encode(acse::DecodeAare(apdu)) == apdu;
		REQUIRE(same);
		++reencoded;

		//our encoder given synthetic values of the same sizes gives the same nesting and lengths
		const Bytes cert(value.certificateLength, 0x42), sig(value.signatureLength, 0x43);
		acse::Authentication mine = acse::MakeMaceAuthentication(*auth.mechanismName, acse::EncodeMaceAuthValue(cert, value.SigningTimeText(), sig));
		Bytes myApdu;
		if(isAarq)
		{
			acse::Aarq x = acse::DecodeAarq(apdu);
			x.authentication = mine;
			myApdu = acse::Encode(x);
		}
		else
		{
			acse::Aare x = acse::DecodeAare(apdu);
			x.authentication = mine;
			myApdu = acse::Encode(x);
		}
		std::vector<std::string> a, b;
		{
			static const char classes[] = {'U', 'A', 'C', 'P'};
			struct Walk
			{
				static void Run(const uint8_t* d, size_t n, int depth, std::vector<std::string>& out)
				{
					ber::Cursor c(d, n);
					while(!c.Empty())
					{
						const ber::Element e = c.Next();
						out.push_back(std::string(static_cast<size_t>(depth), ' ') + classes[static_cast<int>(e.tag.cls)] + std::to_string(e.tag.number) +
							(e.tag.constructed ? "{ " : " ") + std::to_string(e.contentLength));
						if(e.tag.constructed)
							Run(e.content, e.contentLength, depth + 1, out);
					}
				}
			};
			Walk::Run(apdu.data(), apdu.size(), 0, a);
			Walk::Run(myApdu.data(), myApdu.size(), 0, b);
		}
		REQUIRE(a == b);
		++skeletons;

		//the signature, with the captured certificate, with the clock at the captured signing time and the chain not validated
		auto conf = Conf(pki.a, "");
		conf.verifyPeer = false;
		conf.mechanismName = *auth.mechanismName;
		const auto ctx = Make(conf);
		int64_t t = 0;
		REQUIRE(acse::ParseGeneralizedTime(value.SigningTimeText(), t));
		mace::PeerInfo peer;
		mace::Failure failure;
		const bool ok = ctx->Verify(auth, t, peer, failure);
		REQUIRE(ok);
		++verified;
	}
	std::printf("MACE capture check: APDUs decoded %d of 2, re-encoded byte exact %d of 2, skeletons equal %d of 2, signatures verified %d of 2\n", decoded, reencoded, skeletons, verified);
}

#endif // ODC_ASIO_SSL
