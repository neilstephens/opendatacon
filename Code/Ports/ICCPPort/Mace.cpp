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
 * Mace.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  MACE authentication (MMS Application Certificate Exchange), see Mace.h.
 */

#include "Mace.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

#ifdef ODC_ASIO_SSL
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#endif

namespace iccp
{
namespace mace
{

acse::Bytes SignedBlock(const std::string& signingTime)
{
	//SEQUENCE { SEQUENCE { OID sha1, NULL }, OCTET STRING (20 octets) } as a header: the lengths describe a 20 octet digest,
	//but what follows is the 15 octets of the time text
	static const uint8_t header[15] = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
	acse::Bytes block(header, header + sizeof(header));
	block.insert(block.end(), signingTime.begin(), signingTime.end());
	return block;
}

std::string IsoTime(int64_t unixSeconds)
{
	try
	{
		const std::string g = acse::FormatGeneralizedTime(unixSeconds);
		return g.substr(0, 4) + "-" + g.substr(4, 2) + "-" + g.substr(6, 2) + "T" + g.substr(8, 2) + ":" + g.substr(10, 2) + ":" + g.substr(12, 2) + "Z";
	}
	catch(const std::exception&)
	{
		return std::to_string(unixSeconds);
	}
}

const char* ToString(State s)
{
	switch(s)
	{
		case State::None: return "None";
		case State::Sent: return "Sent";
		case State::Verified: return "Verified";
		case State::Failed: return "Failed";
	}
	return "?";
}

struct Context::Impl
{
	MaceConf conf;
	std::vector<std::string> warnings;
	std::string subject;
	#ifdef ODC_ASIO_SSL
	SSL_CTX* ctx = nullptr; //only for its certificate store (the trust anchors and the CRLs)
	X509* cert = nullptr;
	EVP_PKEY* key = nullptr;
	acse::Bytes certDer;
	~Impl()
	{
		if(key)
			EVP_PKEY_free(key);
		if(cert)
			X509_free(cert);
		if(ctx)
			SSL_CTX_free(ctx);
	}
	#endif
};

Context::Context() = default;
Context::~Context() = default;

const MaceConf& Context::Conf() const
{
	return impl->conf;
}

const std::vector<std::string>& Context::Warnings() const
{
	return impl->warnings;
}

const std::string& Context::OwnSubject() const
{
	return impl->subject;
}

#ifdef ODC_ASIO_SSL

namespace
{
constexpr int MaxVerifyDepth = 8;

int KeyType(const EVP_PKEY* k)
{
	#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3
	return EVP_PKEY_get_base_id(k);
	#else
	return EVP_PKEY_base_id(k);
	#endif
}

int KeyBits(const EVP_PKEY* k)
{
	#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3
	return EVP_PKEY_get_bits(k);
	#else
	return EVP_PKEY_bits(k);
	#endif
}

std::string DrainErrors()
{
	std::string out;
	unsigned long e;
	while((e = ERR_get_error()) != 0)
	{
		char buf[256];
		ERR_error_string_n(e, buf, sizeof(buf));
		if(!out.empty())
			out += "; ";
		if(out.size() < 2048)
			out += buf;
	}
	return out;
}

std::string NameString(const X509_NAME* name)
{
	if(!name)
		return "";
	BIO* bio = BIO_new(BIO_s_mem());
	if(!bio)
		return "";
	X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);
	char* data = nullptr;
	const long n = BIO_get_mem_data(bio, &data);
	std::string s(data ? data : "", n > 0 ? static_cast<size_t>(n) : 0);
	BIO_free(bio);
	return s;
}

std::string Hex(const unsigned char* d, size_t n)
{
	static const char digits[] = "0123456789abcdef";
	std::string s;
	s.reserve(n * 2);
	for(size_t i = 0; i < n; ++i)
	{
		s.push_back(digits[d[i] >> 4]);
		s.push_back(digits[d[i] & 0x0F]);
	}
	return s;
}

std::string TimeString(const ASN1_TIME* t)
{
	std::tm tm{};
	if(!t || ASN1_TIME_to_tm(t, &tm) != 1)
		return "";
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buf;
}

//seconds until the certificate expires, negative if it has
int64_t SecondsToExpiry(const X509* cert)
{
	int days = 0, secs = 0;
	if(!ASN1_TIME_diff(&days, &secs, nullptr, X509_get0_notAfter(cert)))
		return 0;
	return static_cast<int64_t>(days) * 86400 + secs;
}

std::string Fingerprint(X509* cert)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int len = 0;
	if(X509_digest(cert, EVP_sha256(), md, &len) != 1)
		return "";
	return Hex(md, len);
}

bool IsIpAddress(const std::string& s)
{
	X509_VERIFY_PARAM* param = X509_VERIFY_PARAM_new();
	if(!param)
		return false;
	const bool ok = X509_VERIFY_PARAM_set1_ip_asc(param, s.c_str()) == 1;
	X509_VERIFY_PARAM_free(param);
	ERR_clear_error();
	return ok;
}

bool MatchAllowList(X509* cert, const std::vector<tls::AllowEntry>& list, std::string& matched)
{
	for(const auto& e : list)
	{
		bool ok = false;
		switch(e.kind)
		{
			case tls::AllowEntry::Kind::CommonName:
			{
				const X509_NAME* name = X509_get_subject_name(cert);
				for(int i = X509_NAME_get_index_by_NID(name, NID_commonName, -1); i >= 0 && !ok; i = X509_NAME_get_index_by_NID(name, NID_commonName, i))
				{
					unsigned char* utf8 = nullptr;
					const int n = ASN1_STRING_to_UTF8(&utf8, X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name, i)));
					if(n >= 0 && utf8)
						ok = std::string(reinterpret_cast<char*>(utf8), static_cast<size_t>(n)) == e.value;
					OPENSSL_free(utf8);
				}
				break;
			}
			case tls::AllowEntry::Kind::San:
				if(IsIpAddress(e.value))
					ok = X509_check_ip_asc(cert, e.value.c_str(), 0) == 1;
				else
					ok = X509_check_host(cert, e.value.c_str(), e.value.size(), X509_CHECK_FLAG_NO_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT, nullptr) == 1;
				break;
			case tls::AllowEntry::Kind::Sha256:
				ok = Fingerprint(cert) == e.value;
				break;
		}
		if(ok)
		{
			matched = e.text;
			return true;
		}
	}
	return false;
}

int PasswordCallback(char* buf, int size, int /*rwflag*/, void* userdata)
{
	//never prompts: the password comes from the environment variable named in the configuration, or there is none
	const auto* password = static_cast<const std::string*>(userdata);
	if(!password || password->empty() || size <= 0)
		return 0;
	const size_t n = std::min(password->size(), static_cast<size_t>(size));
	std::memcpy(buf, password->data(), n);
	return static_cast<int>(n);
}

bool ReadableFile(const std::string& path)
{
	std::ifstream f(path, std::ios::binary);
	return f.good();
}

bool GetEnv(const std::string& name, std::string& value)
{
	#ifdef _MSC_VER
	char* buf = nullptr;
	size_t len = 0;
	if(_dupenv_s(&buf, &len, name.c_str()) != 0 || buf == nullptr)
		return false;
	value = buf;
	std::free(buf);
	return true;
	#else
	const char* v = std::getenv(name.c_str());
	if(!v)
		return false;
	value = v;
	return true;
	#endif
}

//the RSA key size rule; empty if the key is acceptable, else what is wrong
std::string CheckKey(const EVP_PKEY* key, const MaceConf& conf, const char* whose)
{
	const int type = KeyType(key);
	if(type != EVP_PKEY_RSA)
		return std::string(whose) + " key is not an RSA key: MACE supports RSA keys only (a signature of the PKCS#1 v1.5 form)";
	const int bits = KeyBits(key);
	if(bits < static_cast<int>(conf.minKeyBits) || bits > static_cast<int>(conf.maxKeyBits))
		return std::string(whose) + " RSA key has " + std::to_string(bits) + " bits, outside the accepted " + std::to_string(conf.minKeyBits) + " to " +
		       std::to_string(conf.maxKeyBits) + " (\"MinKeyBits\", \"MaxKeyBits\")";
	return "";
}
}

std::shared_ptr<Context> Context::Create(const MaceConf& conf, std::string& error)
{
	std::shared_ptr<Context> c(new Context());
	c->impl = std::make_unique<Impl>();
	Impl& I = *c->impl;
	I.conf = conf;

	ERR_clear_error();
	auto fail = [&](const std::string& what) -> std::shared_ptr<Context>
			{
				error = "\"MACE\": " + what;
				const std::string detail = DrainErrors();
				if(!detail.empty())
					error += " (" + detail + ")";
				return nullptr;
			};

	if(conf.minKeyBits > conf.maxKeyBits || conf.maxKeyBits > AbsoluteMaxKeyBits)
		return fail("\"MinKeyBits\"/\"MaxKeyBits\" must satisfy MinKeyBits <= MaxKeyBits <= " + std::to_string(AbsoluteMaxKeyBits));

	// ---- our certificate and key
	if(!ReadableFile(conf.certFile))
		return fail("\"CertFile\" \"" + conf.certFile + "\": the file cannot be read");
	{
		BIO* bio = BIO_new_file(conf.certFile.c_str(), "r");
		if(!bio)
			return fail("\"CertFile\" \"" + conf.certFile + "\": the file cannot be read");
		I.cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr); //the first certificate is ours; only it is sent
		BIO_free(bio);
		if(!I.cert)
			return fail("\"CertFile\" \"" + conf.certFile + "\": not a valid PEM certificate");
		const int len = i2d_X509(I.cert, nullptr);
		if(len <= 0)
			return fail("\"CertFile\" \"" + conf.certFile + "\": the certificate cannot be encoded");
		I.certDer.resize(static_cast<size_t>(len));
		unsigned char* p = I.certDer.data();
		if(i2d_X509(I.cert, &p) != len)
			return fail("\"CertFile\" \"" + conf.certFile + "\": the certificate cannot be encoded");
	}
	if(!ReadableFile(conf.keyFile))
		return fail("\"KeyFile\" \"" + conf.keyFile + "\": the file cannot be read");
	{
		std::string password;
		if(!conf.keyPasswordEnv.empty() && !GetEnv(conf.keyPasswordEnv, password))
			return fail("\"KeyPasswordEnv\": the environment variable \"" + conf.keyPasswordEnv + "\" is not set");
		BIO* bio = BIO_new_file(conf.keyFile.c_str(), "r");
		if(!bio)
			return fail("\"KeyFile\" \"" + conf.keyFile + "\": the file cannot be read");
		I.key = PEM_read_bio_PrivateKey(bio, nullptr, PasswordCallback, &password);
		BIO_free(bio);
		OPENSSL_cleanse(password.data(), password.size());
		if(!I.key)
			return fail("\"KeyFile\" \"" + conf.keyFile + "\": not a valid PEM private key" +
				(conf.keyPasswordEnv.empty() ? " (if it is encrypted, name the environment variable holding its password in \"KeyPasswordEnv\")" : " (or the password is wrong)"));
	}
	if(X509_check_private_key(I.cert, I.key) != 1)
		return fail("\"CertFile\" \"" + conf.certFile + "\" and \"KeyFile\" \"" + conf.keyFile + "\" do not belong together");
	if(const std::string bad = CheckKey(I.key, conf, "our"); !bad.empty())
		return fail("\"KeyFile\" \"" + conf.keyFile + "\": " + bad);
	I.subject = NameString(X509_get_subject_name(I.cert));
	{
		const int64_t left = SecondsToExpiry(I.cert);
		if(left < 0)
			I.warnings.push_back("our certificate \"" + conf.certFile + "\" (" + I.subject + ") has expired: the peer will refuse it");
		else if(left < static_cast<int64_t>(30) * 86400)
			I.warnings.push_back("our certificate \"" + conf.certFile + "\" (" + I.subject + ") expires in " + std::to_string(left / 86400) + " days");
	}

	// ---- the peer's certificate: trust anchors and CRLs
	if(conf.verifyPeer)
	{
		I.ctx = SSL_CTX_new(TLS_method());
		if(!I.ctx)
			return fail("cannot create the certificate store");
		if(!conf.caFile.empty() && !ReadableFile(conf.caFile))
			return fail("\"CAFile\" \"" + conf.caFile + "\": the file cannot be read");
		if(!conf.caPath.empty() && !std::filesystem::is_directory(conf.caPath))
			return fail("\"CAPath\" \"" + conf.caPath + "\": not a directory");
		if(SSL_CTX_load_verify_locations(I.ctx, conf.caFile.empty() ? nullptr : conf.caFile.c_str(), conf.caPath.empty() ? nullptr : conf.caPath.c_str()) != 1)
			return fail("\"CAFile\" \"" + conf.caFile + "\": no valid PEM CA certificate could be loaded");
		if(!conf.crlFile.empty())
		{
			if(!ReadableFile(conf.crlFile))
				return fail("\"CRLFile\" \"" + conf.crlFile + "\": the file cannot be read");
			BIO* bio = BIO_new_file(conf.crlFile.c_str(), "r");
			if(!bio)
				return fail("\"CRLFile\" \"" + conf.crlFile + "\": the file cannot be read");
			X509_STORE* store = SSL_CTX_get_cert_store(I.ctx);
			size_t count = 0;
			while(X509_CRL* crl = PEM_read_bio_X509_CRL(bio, nullptr, nullptr, nullptr))
			{
				if(X509_STORE_add_crl(store, crl) == 1)
					++count;
				X509_CRL_free(crl);
			}
			BIO_free(bio);
			ERR_clear_error(); //the end of the file is reported as an error
			if(count == 0)
				return fail("\"CRLFile\" \"" + conf.crlFile + "\": no valid PEM CRL found in the file");
			X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | (conf.crlCheckChain ? X509_V_FLAG_CRL_CHECK_ALL : 0));
		}
	}
	else
		I.warnings.push_back("the peer's certificate is not validated (\"VerifyPeer\": false): the signature and the time are checked, but any certificate is accepted");
	return c;
}

bool Context::Sign(int64_t nowUtc, Signed& out, std::string& error, std::optional<int64_t> authContextId) const
{
	const Impl& I = *impl;
	ERR_clear_error();
	try
	{
		const std::string time = acse::FormatGeneralizedTime(nowUtc);
		const acse::Bytes block = SignedBlock(time);

		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(I.key, nullptr);
		if(!ctx)
		{
			error = "cannot create the signing context";
			return false;
		}
		acse::Bytes signature;
		bool ok = EVP_PKEY_sign_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0; //no digest set: the block is signed as it is
		size_t length = 0;
		ok = ok && EVP_PKEY_sign(ctx, nullptr, &length, block.data(), block.size()) > 0;
		if(ok)
		{
			signature.resize(length);
			ok = EVP_PKEY_sign(ctx, signature.data(), &length, block.data(), block.size()) > 0;
			signature.resize(ok ? length : 0);
		}
		EVP_PKEY_CTX_free(ctx);
		if(!ok)
		{
			error = "the signature could not be made";
			const std::string detail = DrainErrors();
			if(!detail.empty())
				error += " (" + detail + ")";
			return false;
		}
		out.authentication = acse::MakeMaceAuthentication(I.conf.mechanismName, acse::EncodeMaceAuthValue(I.certDer, time, signature, authContextId));
		out.subject = I.subject;
		out.signingTime = nowUtc;
		return true;
	}
	catch(const std::exception& e)
	{
		error = e.what();
		return false;
	}
}

namespace
{
struct X509Free
{
	void operator()(X509* x) const
	{
		X509_free(x);
	}
};
}

bool Context::Verify(const acse::Authentication& received, int64_t nowUtc, PeerInfo& peer, Failure& failure) const
{
	const Impl& I = *impl;
	ERR_clear_error();
	auto refuse = [&](int64_t diagnostic, const std::string& text)
			  {
				  failure.diagnostic = diagnostic;
				  failure.text = "MACE: " + text;
				  ERR_clear_error();
				  return false;
			  };
	using namespace acse::UserDiagnostic;

	if(!received.authenticationValue)
		return refuse(AuthenticationRequired, "the peer sent no authentication value");
	if(!received.mechanismName)
		return refuse(AuthenticationMechanismNameRequired, "the peer sent an authentication value without a mechanism-name");
	//the value of the mechanism-name does not affect the procedure and receivers are asked to be tolerant (IEC 62351-4 11.1.4.3): a
	//different one is accepted; the caller logs it (PeerInfo)

	acse::MaceAuthValue value;
	try
	{
		value = acse::DecodeMaceAuthValue(*received.authenticationValue);
	}
	catch(const std::exception& e)
	{
		return refuse(AuthenticationFailure, std::string("the peer's authentication value is malformed (") + e.what() + ")");
	}
	if(value.certificateLength > 65536 || value.signatureLength > 2048)
		return refuse(AuthenticationFailure, "the peer's certificate or signature is implausibly large");
	int64_t signingTime = 0;
	const std::string timeText = value.SigningTimeText();
	if(!acse::ParseGeneralizedTime(timeText, signingTime))
		return refuse(AuthenticationFailure, "the peer's signing time is not a GeneralizedTime YYYYMMDDHHMMSSZ");

	const unsigned char* der = value.certificate;
	std::unique_ptr<X509, X509Free> cert(d2i_X509(nullptr, &der, static_cast<long>(value.certificateLength)));
	if(!cert || der != value.certificate + value.certificateLength)
		return refuse(AuthenticationFailure, "the peer's certificate is not a valid DER X.509 certificate");
	const std::string subject = NameString(X509_get_subject_name(cert.get()));

	EVP_PKEY* pub = X509_get0_pubkey(cert.get()); //borrowed
	if(!pub)
		return refuse(AuthenticationFailure, "the peer's certificate \"" + subject + "\" has no usable public key");
	if(const std::string bad = CheckKey(pub, I.conf, "the peer's"); !bad.empty())
		return refuse(AuthenticationFailure, bad + " (certificate \"" + subject + "\")");

	if(I.conf.verifyPeer)
	{
		X509_STORE_CTX* sctx = X509_STORE_CTX_new();
		if(!sctx)
			return refuse(AuthenticationFailure, "cannot create the certificate verification context");
		bool ok = X509_STORE_CTX_init(sctx, SSL_CTX_get_cert_store(I.ctx), cert.get(), nullptr) == 1;
		std::string why;
		if(ok)
		{
			X509_STORE_CTX_set_time(sctx, 0, static_cast<time_t>(nowUtc));
			X509_STORE_CTX_set_depth(sctx, MaxVerifyDepth);
			ok = X509_verify_cert(sctx) == 1;
			if(!ok)
				why = std::string(X509_verify_cert_error_string(X509_STORE_CTX_get_error(sctx))) + " (depth " + std::to_string(X509_STORE_CTX_get_error_depth(sctx)) + ")";
		}
		else
			why = "cannot initialise the verification";
		X509_STORE_CTX_free(sctx);
		if(!ok)
			return refuse(AuthenticationFailure, "the peer's certificate \"" + subject + "\" is not acceptable: " + why);
		if(!I.conf.peerCertAllowList.empty())
		{
			std::string matched;
			if(!MatchAllowList(cert.get(), I.conf.peerCertAllowList, matched))
				return refuse(AuthenticationFailure, "the peer's certificate \"" + subject + "\" is not in PeerCertAllowList");
		}
	}

	//the signature: the public operation must give the signed block of the time the peer sent
	{
		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pub, nullptr);
		if(!ctx)
			return refuse(AuthenticationFailure, "cannot create the verification context");
		acse::Bytes recovered(static_cast<size_t>(EVP_PKEY_size(pub)));
		size_t length = recovered.size();
		const bool ok = EVP_PKEY_verify_recover_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0 &&
		                EVP_PKEY_verify_recover(ctx, recovered.data(), &length, value.signature, value.signatureLength) > 0;
		EVP_PKEY_CTX_free(ctx);
		const acse::Bytes expected = SignedBlock(timeText);
		if(!ok || length != expected.size() || CRYPTO_memcmp(recovered.data(), expected.data(), length) != 0)
			return refuse(AuthenticationFailure, "the peer's signature is not valid for its certificate \"" + subject + "\" and signing time");
	}

	//the time window (only now that the signature proves the peer really made this time)
	const int64_t difference = nowUtc - signingTime;
	if(difference > static_cast<int64_t>(I.conf.timeWindowSeconds) || difference < -static_cast<int64_t>(I.conf.timeWindowSeconds))
		return refuse(AuthenticationFailure, "signing time outside the window (the peer's clock differs by " + std::to_string(difference) + " s; the signing time is " + IsoTime(signingTime) +
			", the window is " + std::to_string(I.conf.timeWindowSeconds) + " s)");

	peer.subject = subject;
	peer.notAfter = TimeString(X509_get0_notAfter(cert.get()));
	peer.signingTime = signingTime;
	peer.mechanismName = OidToString(*received.mechanismName);
	peer.mechanismOfficial = *received.mechanismName == OfficialMechanismName;
	peer.mechanismDiffers = *received.mechanismName != I.conf.mechanismName;
	return true;
}

#else // no OpenSSL

std::shared_ptr<Context> Context::Create(const MaceConf&, std::string& error)
{
	error = "\"MACE\": this build has no MACE support: rebuild with ODC_ASIO_SSL=ON";
	return nullptr;
}

bool Context::Sign(int64_t, Signed&, std::string& error, std::optional<int64_t>) const
{
	error = "this build has no MACE support";
	return false;
}

bool Context::Verify(const acse::Authentication&, int64_t, PeerInfo&, Failure& failure) const
{
	failure.diagnostic = acse::UserDiagnostic::AuthenticationFailure;
	failure.text = "MACE: this build has no MACE support";
	return false;
}

#endif

} //namespace mace
} //namespace iccp
