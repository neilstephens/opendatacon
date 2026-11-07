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
 * TlsTestPki.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Test support: a throw-away PKI (CA, server and client certificates, CRLs, an encrypted key) made with OpenSSL at run time,
 *  written to a temporary directory that is removed afterwards. Nothing here is a real key or certificate, and none is ever
 *  committed. Only for the TLS tests (ODC_ASIO_SSL). Shared by the tests of the ODC library (the TLS filter and the session manager); the
 *  test directories of ports that use them add this directory to their include path.
 */

#ifndef ODC_TLSTESTPKI_H_
#define ODC_TLSTESTPKI_H_

#ifdef ODC_ASIO_SSL

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace tlstest
{

struct KeyDeleter
{
	void operator()(EVP_PKEY* k) const
	{
		EVP_PKEY_free(k);
	}
};
struct CertDeleter
{
	void operator()(X509* x) const
	{
		X509_free(x);
	}
};

inline std::string BioString(BIO* bio)
{
	char* data = nullptr;
	const long n = BIO_get_mem_data(bio, &data);
	return std::string(data, n > 0 ? static_cast<size_t>(n) : 0);
}

//A temporary directory, removed with the object
class TempDir
{
public:
	TempDir()
	{
		static std::atomic<unsigned> counter{0};
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		dir = std::filesystem::temp_directory_path() / ("tls_tls_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
		std::filesystem::create_directories(dir);
	}
	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(dir, ec);
	}
	TempDir(const TempDir&) = delete;
	TempDir& operator=(const TempDir&) = delete;

	std::string Write(const std::string& name, const std::string& content) const
	{
		const auto p = dir / name;
		std::ofstream f(p, std::ios::binary);
		f << content;
		return p.string();
	}
	std::string Path(const std::string& name) const
	{
		return (dir / name).string();
	}

private:
	std::filesystem::path dir;
};

struct CertSpec
{
	std::string cn = "TEST";
	std::vector<std::string> sans; //"DNS:host", "IP:127.0.0.1"
	int64_t notBefore = -3600;     //seconds relative to now
	int64_t notAfter = 86400LL * 365;
	bool ca = false;
	long serial = 1;
	std::string keyPassword; //non-empty: the key PEM is encrypted with it
	int rsaBits = 0;         //non-zero: an RSA key of this size instead of an EC P-256 one
};

struct Cert
{
	std::unique_ptr<X509, CertDeleter> x509;
	std::unique_ptr<EVP_PKEY, KeyDeleter> key;
	std::string certPem;
	std::string keyPem;
	long serial = 0;
};

inline std::unique_ptr<EVP_PKEY, KeyDeleter> NewKey()
{
	EVP_PKEY_CTX* kc = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
	if(!kc || EVP_PKEY_keygen_init(kc) <= 0 || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(kc, NID_X9_62_prime256v1) <= 0)
		throw std::runtime_error("EC key generation setup failed");
	EVP_PKEY* k = nullptr;
	if(EVP_PKEY_keygen(kc, &k) <= 0)
		throw std::runtime_error("EC key generation failed");
	EVP_PKEY_CTX_free(kc);
	return std::unique_ptr<EVP_PKEY, KeyDeleter>(k);
}

inline std::unique_ptr<EVP_PKEY, KeyDeleter> NewRsaKey(int bits)
{
	EVP_PKEY_CTX* kc = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
	if(!kc || EVP_PKEY_keygen_init(kc) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(kc, bits) <= 0)
		throw std::runtime_error("RSA key generation setup failed");
	EVP_PKEY* k = nullptr;
	if(EVP_PKEY_keygen(kc, &k) <= 0)
		throw std::runtime_error("RSA key generation failed");
	EVP_PKEY_CTX_free(kc);
	return std::unique_ptr<EVP_PKEY, KeyDeleter>(k);
}

inline void AddExt(X509* x, X509* issuer, int nid, const std::string& value)
{
	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, issuer, x, nullptr, nullptr, 0);
	X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value.c_str());
	if(!ext)
		throw std::runtime_error("certificate extension failed: " + value);
	X509_add_ext(x, ext, -1);
	X509_EXTENSION_free(ext);
}

//issuer == nullptr: self-signed
inline Cert MakeCert(const CertSpec& spec, const Cert* issuer = nullptr)
{
	Cert c;
	c.key = spec.rsaBits ? NewRsaKey(spec.rsaBits) : NewKey();
	c.serial = spec.serial;
	c.x509.reset(X509_new());
	X509* x = c.x509.get();
	X509_set_version(x, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(x), spec.serial);
	X509_gmtime_adj(X509_getm_notBefore(x), static_cast<long>(spec.notBefore));
	X509_gmtime_adj(X509_getm_notAfter(x), static_cast<long>(spec.notAfter));
	X509_set_pubkey(x, c.key.get());
	X509_NAME* name = X509_NAME_new();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(spec.cn.c_str()), -1, -1, 0);
	X509_set_subject_name(x, name);
	X509_NAME_free(name);
	X509_set_issuer_name(x, issuer ? X509_get_subject_name(issuer->x509.get()) : X509_get_subject_name(x));
	X509* issuerX = issuer ? issuer->x509.get() : x;
	if(spec.ca)
	{
		AddExt(x, issuerX, NID_basic_constraints, "critical,CA:TRUE");
		AddExt(x, issuerX, NID_key_usage, "critical,keyCertSign,cRLSign");
	}
	else
	{
		AddExt(x, issuerX, NID_basic_constraints, "critical,CA:FALSE");
		AddExt(x, issuerX, NID_key_usage, "critical,digitalSignature");
		AddExt(x, issuerX, NID_ext_key_usage, "serverAuth,clientAuth");
	}
	if(!spec.sans.empty())
	{
		std::string list;
		for(const auto& s : spec.sans)
			list += (list.empty() ? "" : ",") + s;
		AddExt(x, issuerX, NID_subject_alt_name, list);
	}
	if(X509_sign(x, issuer ? issuer->key.get() : c.key.get(), EVP_sha256()) <= 0)
		throw std::runtime_error("certificate signing failed");

	BIO* bio = BIO_new(BIO_s_mem());
	PEM_write_bio_X509(bio, x);
	c.certPem = BioString(bio);
	BIO_free(bio);
	bio = BIO_new(BIO_s_mem());
	if(spec.keyPassword.empty())
		PEM_write_bio_PrivateKey(bio, c.key.get(), nullptr, nullptr, 0, nullptr, nullptr);
	else
		PEM_write_bio_PrivateKey(bio, c.key.get(), EVP_aes_256_cbc(), reinterpret_cast<const unsigned char*>(spec.keyPassword.data()),
			static_cast<int>(spec.keyPassword.size()), nullptr, nullptr);
	c.keyPem = BioString(bio);
	BIO_free(bio);
	return c;
}

//A CRL issued by ca revoking the serial numbers; nextUpdate is relative to now (negative = an expired CRL)
inline std::string MakeCrl(const Cert& ca, const std::vector<long>& revoked, int64_t nextUpdate = 86400)
{
	X509_CRL* crl = X509_CRL_new();
	X509_CRL_set_version(crl, 1);
	X509_CRL_set_issuer_name(crl, X509_get_subject_name(ca.x509.get()));
	const time_t now = std::time(nullptr);
	ASN1_TIME* last = ASN1_TIME_set(nullptr, now - 7200);
	ASN1_TIME* next = ASN1_TIME_set(nullptr, now + static_cast<time_t>(nextUpdate));
	X509_CRL_set1_lastUpdate(crl, last);
	X509_CRL_set1_nextUpdate(crl, next);
	for(long serial : revoked)
	{
		X509_REVOKED* r = X509_REVOKED_new();
		ASN1_INTEGER* sn = ASN1_INTEGER_new();
		ASN1_INTEGER_set(sn, serial);
		X509_REVOKED_set_serialNumber(r, sn);
		ASN1_INTEGER_free(sn);
		ASN1_TIME* when = ASN1_TIME_set(nullptr, now - 3600);
		X509_REVOKED_set_revocationDate(r, when);
		ASN1_TIME_free(when);
		X509_CRL_add0_revoked(crl, r);
	}
	ASN1_TIME_free(last);
	ASN1_TIME_free(next);
	X509_CRL_sort(crl);
	if(X509_CRL_sign(crl, ca.key.get(), EVP_sha256()) <= 0)
		throw std::runtime_error("CRL signing failed");
	BIO* bio = BIO_new(BIO_s_mem());
	PEM_write_bio_X509_CRL(bio, crl);
	std::string pem = BioString(bio);
	BIO_free(bio);
	X509_CRL_free(crl);
	return pem;
}

} //namespace tlstest

#endif // ODC_ASIO_SSL

#endif /* ODC_TLSTESTPKI_H_ */
