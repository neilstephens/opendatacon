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
 * TLSFilter.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TLS filter on OpenSSL memory BIOs, see TLSFilter.h. Only APIs that exist from OpenSSL 1.1.1 to 4.0 are used (no ENGINE,
 *  no low level key structures).
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <opendatacon/TLSFilter.h>

#ifdef ODC_ASIO_SSL
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

namespace odc
{
namespace tls
{

const char* ToString(Filter::State s)
{
	switch(s)
	{
		case Filter::State::Idle: return "Idle";
		case Filter::State::Handshaking: return "Handshaking";
		case Filter::State::Established: return "Established";
		case Filter::State::Closed: return "Closed";
		case Filter::State::Failed: return "Failed";
	}
	return "?";
}

#ifdef ODC_ASIO_SSL

namespace
{
constexpr size_t RecordChunk = 16384;                //plaintext per SSL_write, ciphertext per feed
constexpr size_t MaxPendingPlain = 16 * 1024 * 1024; //application bytes queued while the handshake or a renegotiation is in progress
constexpr int MaxVerifyDepth = 8;

#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3
X509* PeerCertificate(SSL* ssl)
{
	return SSL_get1_peer_certificate(ssl);
}
#else
X509* PeerCertificate(SSL* ssl)
{
	return SSL_get_peer_certificate(ssl);
}
#endif

//the OpenSSL error queue (this thread's) as text, symbolic: "error:0A000086:SSL routines::certificate verify failed"
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

//does the certificate match the allow-list; matched = the entry
bool MatchAllowList(X509* cert, const std::vector<AllowEntry>& list, std::string& matched)
{
	for(const auto& e : list)
	{
		bool ok = false;
		switch(e.kind)
		{
			case AllowEntry::Kind::CommonName:
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
			case AllowEntry::Kind::San:
				if(IsIpAddress(e.value))
					ok = X509_check_ip_asc(cert, e.value.c_str(), 0) == 1;
				else
					ok = X509_check_host(cert, e.value.c_str(), e.value.size(), X509_CHECK_FLAG_NO_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT, nullptr) == 1;
				break;
			case AllowEntry::Kind::Sha256:
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

std::filesystem::file_time_type FileTime(const std::string& path)
{
	std::error_code ec;
	const auto t = std::filesystem::last_write_time(path, ec);
	return ec ? std::filesystem::file_time_type::min() : t;
}

//Make sure the key log file can be appended to, creating it so that only its owner can read it where the platform allows (POSIX mode 0600,
//a Windows DACL that grants the owner alone). An existing file is left as it is.
bool PrepareKeyLogFile(const std::string& path, std::string& error)
{
	std::error_code ec;
	if(std::filesystem::is_directory(path, ec))
	{
		error = "is a directory";
		return false;
	}
	if(!std::filesystem::exists(path, ec))
	{
		#ifdef _WIN32
		PSECURITY_DESCRIPTOR sd = nullptr;
		if(!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:P(A;;FA;;;OW)", SDDL_REVISION_1, &sd, nullptr))
		{
			error = "cannot make the owner-only access rights";
			return false;
		}
		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = sd;
		sa.bInheritHandle = FALSE;
		HANDLE h = CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		LocalFree(sd);
		if(h == INVALID_HANDLE_VALUE)
		{
			error = "the file cannot be created (is the directory there and writable?)";
			return false;
		}
		CloseHandle(h);
		#else
		const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
		if(fd < 0)
		{
			error = "the file cannot be created (is the directory there and writable?)";
			return false;
		}
		::close(fd);
		#endif
	}
	std::ofstream f(path, std::ios::app);
	if(!f)
	{
		error = "the file cannot be opened for appending";
		return false;
	}
	return true;
}
}

// ---------------------------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------------------------

struct Context::Impl
{
	~Impl()
	{
		SSL_SESSION_free(session);
		SSL_CTX_free(ctx);
	}
	SSL_CTX* ctx = nullptr;
	TlsConf conf;
	Role role = Role::Client;
	std::vector<std::string> warnings;
	std::vector<std::pair<std::string, std::filesystem::file_time_type>> files;
	std::mutex sessionMtx;
	std::mutex keyLogMtx;
	SSL_SESSION* session = nullptr; //client: the last session, for resumption
};

namespace
{
int ExIndex()
{
	static const int idx = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
	return idx;
}
}

Context::Context()
{}
Context::~Context() = default;

const TlsConf& Context::Conf() const
{
	return impl->conf;
}
Role Context::GetRole() const
{
	return impl->role;
}
const std::vector<std::string>& Context::Warnings() const
{
	return impl->warnings;
}
bool Context::Stale() const
{
	for(const auto& [path, time] : impl->files)
		if(FileTime(path) != time)
			return true;
	return false;
}

namespace
{
void InfoCallback(const SSL* ssl, int where, int ret);
void MsgCallback(int write_p, int version, int content_type, const void* buf, size_t len, SSL* ssl, void* arg);
void KeyLogCallback(const SSL* ssl, const char* line);
int VerifyCallback(int preverify, X509_STORE_CTX* sctx);
int NewSessionCallback(SSL* ssl, SSL_SESSION* session);
int PermissiveVerifyCallback(int, X509_STORE_CTX*)
{
	return 1;
}
}

std::shared_ptr<Context> Context::Create(const TlsConf& conf, Role role, std::string& error)
{
	std::shared_ptr<Context> c(new Context());
	c->impl = std::make_unique<Impl>();
	Impl& I = *c->impl;
	I.conf = conf;
	I.role = role;

	ERR_clear_error();
	auto fail = [&](const std::string& what) -> std::shared_ptr<Context>
			{
				error = "\"TLS\": " + what;
				const std::string detail = DrainErrors();
				if(!detail.empty())
					error += " (" + detail + ")";
				return nullptr;
			};

	I.ctx = SSL_CTX_new(TLS_method());
	if(!I.ctx)
		return fail("cannot create the TLS context");
	SSL_CTX* ctx = I.ctx;

	if(SSL_CTX_set_min_proto_version(ctx, conf.minVersion >= 13 ? TLS1_3_VERSION : TLS1_2_VERSION) != 1 ||
	   SSL_CTX_set_max_proto_version(ctx, conf.maxVersion >= 13 ? TLS1_3_VERSION : TLS1_2_VERSION) != 1)
		return fail("\"MinVersion\"/\"MaxVersion\": the TLS library does not support this protocol version range");

	uint64_t options = SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE;
	if(conf.renegotiateIntervalSeconds == 0)
		options |= SSL_OP_NO_RENEGOTIATION;
	else if(role == Role::Server)
		options |= SSL_OP_ALLOW_CLIENT_RENEGOTIATION; //off by default in servers; the TLS client is the one that starts them here
	if(!conf.sessionResumption || !conf.sessionTickets)
		options |= SSL_OP_NO_TICKET;
	if(!conf.extendedMasterSecret)
		options |= SSL_OP_NO_EXTENDED_MASTER_SECRET;
	SSL_CTX_set_options(ctx, options);
	SSL_CTX_set_mode(ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	//the security level first: it decides which suites, signatures and key sizes the lists below can use
	if(conf.securityLevel >= 0)
		SSL_CTX_set_security_level(ctx, conf.securityLevel);
	//empty lists leave OpenSSL's own defaults in place
	if(!conf.ciphers.empty() && SSL_CTX_set_cipher_list(ctx, conf.ciphers.c_str()) != 1)
		return fail("\"Ciphers\" \"" + conf.ciphers + "\": no cipher suite of this list is available (OpenSSL cipher list syntax)");
	if(!conf.cipherSuites13.empty() && SSL_CTX_set_ciphersuites(ctx, conf.cipherSuites13.c_str()) != 1)
		return fail("\"CipherSuites13\" \"" + conf.cipherSuites13 + "\": no TLS 1.3 cipher suite of this list is available (OpenSSL syntax)");
	if(!conf.groups.empty() && SSL_CTX_set1_groups_list(ctx, conf.groups.c_str()) != 1)
		return fail("\"Groups\" \"" + conf.groups + "\": not a list of groups this TLS library knows (OpenSSL syntax, for example \"P-256:P-384:X25519\")");
	if(!conf.signatureAlgorithms.empty() &&
	   (SSL_CTX_set1_sigalgs_list(ctx, conf.signatureAlgorithms.c_str()) != 1 || SSL_CTX_set1_client_sigalgs_list(ctx, conf.signatureAlgorithms.c_str()) != 1))
		return fail("\"SignatureAlgorithms\" \"" + conf.signatureAlgorithms + "\": not a list of signature algorithms this TLS library knows (OpenSSL syntax, for example \"RSA+SHA256:ECDSA+SHA256\")");
	SSL_CTX_set_dh_auto(ctx, 1);

	// ---- our certificate and key
	if(!conf.certFile.empty())
	{
		if(!ReadableFile(conf.certFile))
			return fail("\"CertFile\" \"" + conf.certFile + "\": the file cannot be read");
		if(SSL_CTX_use_certificate_chain_file(ctx, conf.certFile.c_str()) != 1)
			return fail("\"CertFile\" \"" + conf.certFile + "\": not a valid PEM certificate");
		if(!ReadableFile(conf.keyFile))
			return fail("\"KeyFile\" \"" + conf.keyFile + "\": the file cannot be read");
		std::string password;
		if(!conf.keyPasswordEnv.empty())
		{
			if(!GetEnv(conf.keyPasswordEnv, password))
				return fail("\"KeyPasswordEnv\": the environment variable \"" + conf.keyPasswordEnv + "\" is not set");
		}
		SSL_CTX_set_default_passwd_cb(ctx, PasswordCallback);
		SSL_CTX_set_default_passwd_cb_userdata(ctx, &password);
		const int keyOk = SSL_CTX_use_PrivateKey_file(ctx, conf.keyFile.c_str(), SSL_FILETYPE_PEM);
		OPENSSL_cleanse(password.data(), password.size());
		SSL_CTX_set_default_passwd_cb_userdata(ctx, nullptr);
		if(keyOk != 1)
		{
			const unsigned long last = ERR_peek_last_error();
			if(ERR_GET_LIB(last) == ERR_LIB_X509 && ERR_GET_REASON(last) == X509_R_KEY_VALUES_MISMATCH)
				return fail("\"CertFile\" \"" + conf.certFile + "\" and \"KeyFile\" \"" + conf.keyFile + "\" do not belong together");
			return fail("\"KeyFile\" \"" + conf.keyFile + "\": not a valid PEM private key" +
				(conf.keyPasswordEnv.empty() ? " (if it is encrypted, name the environment variable holding its password in \"KeyPasswordEnv\")" : " (or the password is wrong)"));
		}
		if(SSL_CTX_check_private_key(ctx) != 1)
			return fail("\"CertFile\" \"" + conf.certFile + "\" and \"KeyFile\" \"" + conf.keyFile + "\" do not belong together");
		I.files.emplace_back(conf.certFile, FileTime(conf.certFile));
		I.files.emplace_back(conf.keyFile, FileTime(conf.keyFile));
		if(X509* own = SSL_CTX_get0_certificate(ctx))
		{
			const int64_t left = SecondsToExpiry(own);
			if(left < 0)
				I.warnings.push_back("our certificate \"" + conf.certFile + "\" (" + NameString(X509_get_subject_name(own)) + ") has expired: the peer will refuse it");
			else if(left < static_cast<int64_t>(conf.certExpiryWarningDays) * 86400)
				I.warnings.push_back("our certificate \"" + conf.certFile + "\" (" + NameString(X509_get_subject_name(own)) + ") expires in " + std::to_string(left / 86400) + " days");
		}
	}

	// ---- the peer's certificate
	if(conf.verifyPeer)
	{
		if(!conf.caFile.empty() && !ReadableFile(conf.caFile))
			return fail("\"CAFile\" \"" + conf.caFile + "\": the file cannot be read");
		if(!conf.caPath.empty() && !std::filesystem::is_directory(conf.caPath))
			return fail("\"CAPath\" \"" + conf.caPath + "\": not a directory");
		if(SSL_CTX_load_verify_locations(ctx, conf.caFile.empty() ? nullptr : conf.caFile.c_str(), conf.caPath.empty() ? nullptr : conf.caPath.c_str()) != 1)
			return fail("\"CAFile\" \"" + conf.caFile + "\": no valid PEM CA certificate could be loaded");
		if(!conf.caFile.empty())
			I.files.emplace_back(conf.caFile, FileTime(conf.caFile));
		if(!conf.crlFile.empty())
		{
			if(!ReadableFile(conf.crlFile))
				return fail("\"CRLFile\" \"" + conf.crlFile + "\": the file cannot be read");
			BIO* bio = BIO_new_file(conf.crlFile.c_str(), "r");
			if(!bio)
				return fail("\"CRLFile\" \"" + conf.crlFile + "\": the file cannot be read");
			X509_STORE* store = SSL_CTX_get_cert_store(ctx);
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
			I.files.emplace_back(conf.crlFile, FileTime(conf.crlFile));
		}
		int mode = SSL_VERIFY_PEER;
		if(role == Role::Server && conf.requireClientCert)
			mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		SSL_CTX_set_verify(ctx, mode, VerifyCallback);
		SSL_CTX_set_verify_depth(ctx, MaxVerifyDepth);
	}
	else
	{
		//no verification: a certificate is still requested (and logged), whatever it is
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, PermissiveVerifyCallback);
		I.warnings.push_back("peer certificate verification is disabled (\"VerifyPeer\": false): the peer is not authenticated");
	}

	// ---- sessions
	{
		const std::string& sidContext = conf.sessionIdContext.empty() ? std::string("opendatacon") : conf.sessionIdContext;
		if(SSL_CTX_set_session_id_context(ctx, reinterpret_cast<const unsigned char*>(sidContext.data()), static_cast<unsigned int>(sidContext.size())) != 1)
			return fail("\"sessionIdContext\" \"" + sidContext + "\": too long for a TLS session id context (at most " + std::to_string(SSL_MAX_SID_CTX_LENGTH) + " bytes)");
	}
	if(conf.sessionResumption)
	{
		if(role == Role::Server)
			SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
		else
		{
			SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
			SSL_CTX_sess_set_new_cb(ctx, NewSessionCallback);
		}
	}
	else
	{
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
		SSL_CTX_set_num_tickets(ctx, 0);
	}

	SSL_CTX_set_info_callback(ctx, InfoCallback);
	SSL_CTX_set_msg_callback(ctx, MsgCallback); //the failure summary needs what was sent and received
	if(!conf.keyLogFile.empty())
	{
		std::string why;
		if(!PrepareKeyLogFile(conf.keyLogFile, why))
			return fail("\"KeyLogFile\" \"" + conf.keyLogFile + "\": " + why);
		SSL_CTX_set_keylog_callback(ctx, KeyLogCallback);
		I.warnings.push_back("KEY LOGGING IS ON (\"KeyLogFile\" \"" + conf.keyLogFile + "\"): the TLS secrets are written to that file and anyone who can read it can decrypt the traffic. For diagnostics only.");
	}
	return c;
}

// ---------------------------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------------------------

struct Filter::Impl
{
	Impl(std::shared_ptr<Context> c, Role r, FilterCallbacks callbacks, std::string name, std::function<Clock::time_point()> clk):
		context(std::move(c)),
		role(r),
		cb(std::move(callbacks)),
		peerName(std::move(name)),
		clock(std::move(clk))
	{
		const TlsConf& conf = context->Conf();
		ERR_clear_error();
		ssl = SSL_new(context->Internal()->ctx);
		rbio = BIO_new(BIO_s_mem());
		wbio = BIO_new(BIO_s_mem());
		if(!ssl || !rbio || !wbio)
		{
			if(ssl)
			{
				BIO_free(rbio); //(BIO_free accepts null)
				BIO_free(wbio);
				SSL_free(ssl);
			}
			else
			{
				BIO_free(rbio);
				BIO_free(wbio);
			}
			ssl = nullptr;
			rbio = wbio = nullptr;
			state = State::Failed;
			failure = "cannot create the TLS session: " + DrainErrors();
			return;
		}
		BIO_set_mem_eof_return(rbio, -1); //an empty input is "try again", not the end of the stream
		SSL_set_bio(ssl, rbio, wbio);
		SSL_set_ex_data(ssl, ExIndex(), this);
		if(role == Role::Client)
		{
			SSL_set_connect_state(ssl);
			if(!peerName.empty())
			{
				const bool ip = IsIpAddress(peerName);
				if(conf.verifyHostname && conf.verifyPeer)
				{
					X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
					if(ip)
						X509_VERIFY_PARAM_set1_ip_asc(param, peerName.c_str());
					else
					{
						X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
						X509_VERIFY_PARAM_set1_host(param, peerName.c_str(), 0);
					}
				}
				if(!ip && conf.sendSni)
					SSL_set_tlsext_host_name(ssl, peerName.c_str());
			}
			if(conf.sessionResumption)
			{
				auto* ci = context->Internal();
				std::lock_guard<std::mutex> lock(ci->sessionMtx);
				if(ci->session)
					SSL_set_session(ssl, ci->session);
			}
		}
		else
			SSL_set_accept_state(ssl);
		ERR_clear_error();
		if(!conf.keyLogFile.empty())
			Log(LogLevel::Warn, "KEY LOGGING IS ON for this connection: its secrets are appended to \"" + conf.keyLogFile + "\"; anyone who can read that file can decrypt the traffic");
	}
	~Impl()
	{
		if(ssl)
			SSL_free(ssl); //also frees the BIOs
	}

	std::shared_ptr<Context> context;
	const Role role;
	FilterCallbacks cb;
	const std::string peerName;
	std::function<Clock::time_point()> clock;

	SSL* ssl = nullptr;
	BIO* rbio = nullptr; //we write what arrives from the transport
	BIO* wbio = nullptr; //we read what is to be sent
	State state = State::Idle;
	std::string failure;
	bool failureReported = false;

	Bytes pending;       //application bytes not yet encrypted
	size_t inflight = 0; //the length of the SSL_write that must be repeated unchanged (it returned WANT_READ/WANT_WRITE)
	bool pumping = false;
	bool again = false;
	bool firstDataSeen = false;

	Clock::time_point startedAt;
	Clock::time_point nextRenegotiation;
	std::optional<PeerInfo> peer;
	uint64_t renegotiations = 0;
	bool localRenegotiation = false; //we started a renegotiation and its HANDSHAKE_START has not been seen yet
	Clock::time_point lastPeerRenegotiation;
	bool peerRenegotiated = false;
	std::string verifyProblem; //recorded by the verify callback
	std::string lastAlert;

	//what the handshake looked like so far, for the failure line
	std::vector<std::string> sentEvents;
	std::vector<std::string> receivedEvents;
	size_t receivedHandshakeMessages = 0;
	static constexpr size_t MaxEvents = 32;

	void AddEvent(bool sent, const std::string& text)
	{
		auto& list = sent ? sentEvents : receivedEvents;
		if(list.size() < MaxEvents)
			list.push_back(text);
		else if(list.size() == MaxEvents)
			list.push_back("...");
	}

	//the handshake as far as it got: "we are the TLS client; sent: ClientHello (...); received: alert fatal X after 0 handshake messages from the peer"
	std::string HandshakeSummary() const
	{
		auto join = [](const std::vector<std::string>& v)
				{
					std::string s;
					for(size_t i = 0; i < v.size(); ++i)
						s += (i ? ", " : "") + v[i];
					return s.empty() ? std::string("nothing") : s;
				};
		std::string s = std::string("we are the TLS ") + (role == Role::Client ? "client" : "server") + "; sent: " + join(sentEvents) + "; received: " + join(receivedEvents);
		for(const auto& e : receivedEvents)
			if(e.rfind("alert ", 0) == 0)
			{
				s += " after " + std::to_string(receivedHandshakeMessages) + " handshake messages from the peer";
				break;
			}
		return s;
	}

	static std::string HandshakeTypeName(uint8_t type)
	{
		switch(type)
		{
			case 0: return "HelloRequest";
			case 1: return "ClientHello";
			case 2: return "ServerHello";
			case 4: return "NewSessionTicket";
			case 8: return "EncryptedExtensions";
			case 11: return "Certificate";
			case 12: return "ServerKeyExchange";
			case 13: return "CertificateRequest";
			case 14: return "ServerHelloDone";
			case 15: return "CertificateVerify";
			case 16: return "ClientKeyExchange";
			case 20: return "Finished";
			case 24: return "KeyUpdate";
			default: return "handshake message " + std::to_string(type);
		}
	}

	//the message callback: the handshake messages and alerts, in both directions, while handshaking
	void OnMessage(bool sent, int contentType, const uint8_t* data, size_t length)
	{
		if(contentType == SSL3_RT_ALERT && length >= 2)
		{
			const int value = (data[0] << 8) | data[1];
			AddEvent(sent, std::string("alert ") + SSL_alert_type_string_long(value) + " " + SSL_alert_desc_string_long(value));
			return;
		}
		if(contentType != SSL3_RT_HANDSHAKE || length < 1 || state == State::Established)
			return;
		AddEvent(sent, HandshakeTypeName(data[0]) + " (" + std::to_string(length) + " bytes)");
		if(!sent)
			++receivedHandshakeMessages;
	}
	const TlsConf& Conf() const
	{
		return context->Conf();
	}
	void Log(LogLevel l, const std::string& text) const
	{
		if(cb.log)
			cb.log(l, text);
	}

	void FlushOut()
	{
		if(!wbio)
			return;
		size_t n;
		while((n = BIO_ctrl_pending(wbio)) > 0)
		{
			Bytes out(std::min<size_t>(n, 65536));
			const int r = BIO_read(wbio, out.data(), static_cast<int>(out.size()));
			if(r <= 0)
				break;
			out.resize(static_cast<size_t>(r));
			if(cb.send)
				cb.send(out);
		}
	}

	//the end: the reason as the OpenSSL error queue, the verify result and the alerts describe it
	void Fail(const std::string& head)
	{
		if(state == State::Failed)
			return;
		std::string msg = head;
		const std::string errs = DrainErrors();
		if(!errs.empty())
			msg += ": " + errs;
		if(!verifyProblem.empty())
			msg += " [" + verifyProblem + "]";
		else if(ssl)
		{
			const long vr = SSL_get_verify_result(ssl);
			if(vr != X509_V_OK)
				msg += std::string(" [peer certificate verification: ") + X509_verify_cert_error_string(vr) + "]";
		}
		if(!lastAlert.empty())
			msg += " [" + lastAlert + "]";
		msg += " [" + HandshakeSummary() + "]";
		state = State::Failed;
		failure = msg;
		failureReported = true;
		FlushOut(); //a fatal alert the library queued
		if(cb.onFailed)
			cb.onFailed(msg);
	}

	void NoteAlert(bool read, int value)
	{
		lastAlert = std::string(read ? "alert received: " : "alert sent: ") + SSL_alert_type_string_long(value) + " " + SSL_alert_desc_string_long(value);
	}

	void OnHandshakeStart()
	{
		if(state != State::Established || SSL_version(ssl) >= TLS1_3_VERSION)
			return;
		//a TLS 1.2 renegotiation. Ours is expected; one the peer starts is allowed (if renegotiation is enabled at all, else the library
		//refuses it) but not more often than every half interval.
		if(localRenegotiation)
		{
			localRenegotiation = false;
			return;
		}
		const auto now = clock();
		const auto minGap = std::chrono::milliseconds(std::max<int64_t>(500, static_cast<int64_t>(Conf().renegotiateIntervalSeconds) * 500));
		if(peerRenegotiated && now - lastPeerRenegotiation < minGap)
			peerRenegotiationAbuse = true;
		peerRenegotiated = true;
		lastPeerRenegotiation = now;
	}
	bool peerRenegotiationAbuse = false;

	void OnHandshakeDone()
	{
		if(state == State::Established && SSL_version(ssl) < TLS1_3_VERSION)
			++renegotiations;
	}

	PeerInfo BuildPeerInfo()
	{
		PeerInfo p;
		p.protocol = SSL_get_version(ssl);
		if(const SSL_CIPHER* c = SSL_get_current_cipher(ssl))
			p.cipher = SSL_CIPHER_get_name(c);
		p.resumed = SSL_session_reused(ssl) == 1;
		if(X509* cert = PeerCertificate(ssl))
		{
			p.certificate = true;
			p.subject = NameString(X509_get_subject_name(cert));
			p.issuer = NameString(X509_get_issuer_name(cert));
			if(const ASN1_INTEGER* serial = X509_get0_serialNumber(cert))
				p.serial = Hex(ASN1_STRING_get0_data(serial), static_cast<size_t>(ASN1_STRING_length(serial)));
			p.sha256 = Fingerprint(cert);
			p.notBefore = TimeString(X509_get0_notBefore(cert));
			p.notAfter = TimeString(X509_get0_notAfter(cert));
			p.secondsToExpiry = SecondsToExpiry(cert);
			X509_free(cert);
		}
		return p;
	}

	void CompleteHandshake()
	{
		PeerInfo info = BuildPeerInfo();
		const TlsConf& conf = Conf();
		if(!conf.peerCertAllowList.empty())
		{
			//the verify callback checked a certificate that was verified in this handshake; a resumed session or an optional
			//client certificate that was not presented skips it: check here as well
			X509* cert = PeerCertificate(ssl);
			std::string matched;
			const bool ok = cert && MatchAllowList(cert, conf.peerCertAllowList, matched);
			if(cert)
				X509_free(cert);
			if(!ok)
			{
				Fail(info.certificate ? "the peer certificate \"" + info.subject + "\" is not in PeerCertAllowList" : "the peer presented no certificate and PeerCertAllowList is set");
				return;
			}
		}
		state = State::Established;
		if(conf.renegotiateIntervalSeconds > 0 && role == Role::Client)
			nextRenegotiation = clock() + std::chrono::seconds(conf.renegotiateIntervalSeconds);
		peer = info;
		if(info.certificate)
		{
			const int64_t warnSeconds = static_cast<int64_t>(conf.certExpiryWarningDays) * 86400;
			if(info.secondsToExpiry < 0)
				Log(LogLevel::Warn, "the peer certificate \"" + info.subject + "\" has expired (" + info.notAfter + ")");
			else if(info.secondsToExpiry < warnSeconds)
				Log(LogLevel::Warn, "the peer certificate \"" + info.subject + "\" expires in " + std::to_string(info.secondsToExpiry / 86400) + " days (" + info.notAfter + ")");
		}
		if(cb.onEstablished)
			cb.onEstablished(info);
	}

	//encrypt what is queued; false if the session failed
	bool WritePending()
	{
		while(!pending.empty() && state == State::Established)
		{
			const size_t n = inflight ? inflight : std::min(pending.size(), RecordChunk);
			ERR_clear_error();
			const int r = SSL_write(ssl, pending.data(), static_cast<int>(n));
			if(r > 0)
			{
				pending.erase(pending.begin(), pending.begin() + r);
				inflight = 0;
				continue;
			}
			const int e = SSL_get_error(ssl, r);
			inflight = n; //repeat with the same length
			FlushOut();
			if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
				return true;
			Fail("TLS write failed");
			return false;
		}
		FlushOut();
		return true;
	}

	void PumpOnce()
	{
		if(!ssl)
			return;
		if(state == State::Handshaking)
		{
			ERR_clear_error();
			const int r = SSL_do_handshake(ssl);
			FlushOut();
			if(r == 1)
				CompleteHandshake();
			else
			{
				const int e = SSL_get_error(ssl, r);
				if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
					return;
				Fail(e == SSL_ERROR_ZERO_RETURN ? "the peer closed the TLS connection during the handshake" : "TLS handshake failed");
				return;
			}
		}
		if(state != State::Established)
			return;
		if(!WritePending())
			return;
		uint8_t buf[RecordChunk];
		while(state == State::Established)
		{
			ERR_clear_error();
			const int n = SSL_read(ssl, buf, static_cast<int>(sizeof(buf)));
			if(n > 0)
			{
				FlushOut();
				if(cb.onPlain)
					cb.onPlain(buf, static_cast<size_t>(n));
				continue;
			}
			const int e = SSL_get_error(ssl, n);
			FlushOut();
			if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
				break;
			if(e == SSL_ERROR_ZERO_RETURN)
			{
				//close_notify: answer it, then the owner closes the transport
				WritePending();
				ERR_clear_error();
				SSL_shutdown(ssl);
				FlushOut();
				state = State::Closed;
				if(cb.onPeerClosed)
					cb.onPeerClosed();
				return;
			}
			Fail(e == SSL_ERROR_SYSCALL ? "the TLS connection ended unexpectedly" : "TLS error");
			return;
		}
		if(state == State::Established && peerRenegotiationAbuse)
		{
			Fail("the peer renegotiates too often");
			return;
		}
		if(state == State::Established && !pending.empty())
			WritePending();
	}

	void Pump()
	{
		if(pumping)
		{
			again = true;
			return;
		}
		pumping = true;
		do
		{
			again = false;
			PumpOnce();
		} while(again && state != State::Failed && state != State::Closed);
		pumping = false;
	}

	void Renegotiate()
	{
		const auto interval = std::chrono::seconds(Conf().renegotiateIntervalSeconds);
		nextRenegotiation = clock() + interval;
		ERR_clear_error();
		if(SSL_version(ssl) >= TLS1_3_VERSION)
		{
			if(SSL_key_update(ssl, SSL_KEY_UPDATE_REQUESTED) != 1)
			{
				Fail("TLS key update failed");
				return;
			}
			++renegotiations;
			Log(LogLevel::Debug, "TLS key update requested");
		}
		else
		{
			if(SSL_renegotiate_pending(ssl))
				return;
			if(SSL_renegotiate(ssl) != 1)
			{
				Fail("TLS renegotiation could not be started");
				return;
			}
			localRenegotiation = true;
			Log(LogLevel::Debug, "TLS renegotiation requested");
		}
		const int r = SSL_do_handshake(ssl);
		FlushOut();
		if(r <= 0)
		{
			const int e = SSL_get_error(ssl, r);
			if(e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE)
			{
				Fail("TLS renegotiation failed");
				return;
			}
		}
		Pump();
	}
};

namespace
{
Filter::Impl* FromSsl(const SSL* ssl)
{
	return ssl ? static_cast<Filter::Impl*>(SSL_get_ex_data(ssl, ExIndex())) : nullptr;
}

void InfoCallback(const SSL* ssl, int where, int ret)
{
	Filter::Impl* impl = FromSsl(ssl);
	if(!impl)
		return;
	if(where & SSL_CB_ALERT)
		impl->NoteAlert((where & SSL_CB_READ) != 0, ret);
	else if(where & SSL_CB_HANDSHAKE_START)
		impl->OnHandshakeStart();
	else if(where & SSL_CB_HANDSHAKE_DONE)
		impl->OnHandshakeDone();
}

void MsgCallback(int write_p, int /*version*/, int content_type, const void* buf, size_t len, SSL* ssl, void*)
{
	Filter::Impl* impl = FromSsl(ssl);
	if(!impl || !buf)
		return;
	impl->OnMessage(write_p != 0, content_type, static_cast<const uint8_t*>(buf), len);
}

//NSS key log lines, only when KeyLogFile is configured (the callback is not even installed otherwise); never to the log
void KeyLogCallback(const SSL* ssl, const char* line)
{
	Filter::Impl* impl = FromSsl(ssl);
	if(!impl || !line)
		return;
	auto* ci = impl->context->Internal();
	if(ci->conf.keyLogFile.empty())
		return;
	std::lock_guard<std::mutex> lock(ci->keyLogMtx);
	std::ofstream f(ci->conf.keyLogFile, std::ios::app);
	if(f)
		f << line << "\n";
}

int VerifyCallback(int preverify, X509_STORE_CTX* sctx)
{
	SSL* ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(sctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
	Filter::Impl* impl = FromSsl(ssl);
	const int depth = X509_STORE_CTX_get_error_depth(sctx);
	X509* cert = X509_STORE_CTX_get_current_cert(sctx);
	if(!preverify)
	{
		if(impl)
		{
			const int err = X509_STORE_CTX_get_error(sctx);
			impl->verifyProblem = std::string("certificate verification failed: ") + X509_verify_cert_error_string(err) + " (depth " + std::to_string(depth) +
			                      ", subject " + (cert ? NameString(X509_get_subject_name(cert)) : std::string("?")) + ")";
		}
		return 0;
	}
	if(depth == 0 && impl && cert && !impl->Conf().peerCertAllowList.empty())
	{
		std::string matched;
		if(!MatchAllowList(cert, impl->Conf().peerCertAllowList, matched))
		{
			impl->verifyProblem = "the peer certificate \"" + NameString(X509_get_subject_name(cert)) + "\" is not in PeerCertAllowList";
			X509_STORE_CTX_set_error(sctx, X509_V_ERR_APPLICATION_VERIFICATION);
			return 0;
		}
	}
	return 1;
}

int NewSessionCallback(SSL* ssl, SSL_SESSION* session)
{
	Filter::Impl* impl = FromSsl(ssl);
	if(!impl || impl->role != Role::Client)
		return 0;
	auto* ci = impl->context->Internal();
	std::lock_guard<std::mutex> lock(ci->sessionMtx);
	SSL_SESSION_free(ci->session);
	ci->session = session; //ownership taken
	return 1;
}
}

Filter::Filter(std::shared_ptr<Context> context, Role role, FilterCallbacks callbacks, std::string peerName, std::function<Clock::time_point()> clock):
	impl(std::make_unique<Impl>(std::move(context), role, std::move(callbacks), std::move(peerName), std::move(clock)))
{}
Filter::~Filter() = default;

void Filter::Start()
{
	if(impl->state == State::Failed && !impl->failureReported)
	{
		impl->failureReported = true;
		if(impl->cb.onFailed)
			impl->cb.onFailed(impl->failure);
		return;
	}
	if(impl->state != State::Idle)
		return;
	impl->state = State::Handshaking;
	impl->startedAt = impl->clock();
	impl->Pump();
}

void Filter::OnTransportData(const uint8_t* data, size_t length)
{
	if(impl->state == State::Idle)
		Start();
	if(length > 0 && impl->role == Role::Server && !impl->firstDataSeen && impl->state == State::Handshaking)
	{
		//a TLS client starts with a handshake record (0x16). Anything else is not TLS: say so at once, instead of waiting for the
		//rest of a record that will never come (typically a plain RFC 1006 client, whose TPKT starts 0x03 0x00)
		impl->firstDataSeen = true;
		if(data[0] != 0x16)
		{
			char hex[8];
			std::snprintf(hex, sizeof(hex), "0x%02X", data[0]);
			impl->Fail(std::string("the first byte received (") + hex + ") is not a TLS handshake record: the peer is not using TLS" +
				(data[0] == 0x03 && length > 1 && data[1] == 0x00 ? " (it looks like a plain RFC 1006 / TPKT connection)" : ""));
			return;
		}
	}
	while(length > 0 && (impl->state == State::Handshaking || impl->state == State::Established))
	{
		const size_t n = std::min(length, RecordChunk);
		if(BIO_write(impl->rbio, data, static_cast<int>(n)) != static_cast<int>(n))
		{
			impl->Fail("cannot buffer the received data");
			return;
		}
		data += n;
		length -= n;
		impl->Pump();
	}
}

void Filter::Write(const uint8_t* data, size_t length)
{
	if(length == 0 || impl->state == State::Failed || impl->state == State::Closed)
		return;
	if(impl->pending.size() + length > MaxPendingPlain)
	{
		impl->Fail("too much data queued to send");
		return;
	}
	impl->pending.insert(impl->pending.end(), data, data + length);
	if(impl->state == State::Established)
		impl->Pump();
}

void Filter::Shutdown()
{
	if(impl->state == State::Established)
	{
		impl->WritePending();
		ERR_clear_error();
		SSL_shutdown(impl->ssl);
		impl->FlushOut();
		ERR_clear_error();
	}
	if(impl->state != State::Failed)
		impl->state = State::Closed;
}

void Filter::Tick()
{
	Impl& I = *impl;
	const auto now = I.clock();
	if(I.state == State::Handshaking)
	{
		const auto limit = std::chrono::milliseconds(I.Conf().handshakeTimeoutMs);
		if(now - I.startedAt >= limit)
			I.Fail("the TLS handshake did not complete within " + std::to_string(I.Conf().handshakeTimeoutMs) + " ms");
	}
	else if(I.state == State::Established && I.role == Role::Client && I.Conf().renegotiateIntervalSeconds > 0 && now >= I.nextRenegotiation)
		I.Renegotiate();
}

Filter::State Filter::GetState() const
{
	return impl->state;
}
std::optional<PeerInfo> Filter::Peer() const
{
	return impl->peer;
}
uint64_t Filter::Renegotiations() const
{
	return impl->renegotiations;
}
size_t Filter::PendingPlainBytes() const
{
	return impl->pending.size();
}
std::string Filter::HandshakeSummary() const
{
	return impl->HandshakeSummary();
}

#else // ODC_ASIO_SSL

// ---------------------------------------------------------------------------------------------
// without OpenSSL: nothing can be created
// ---------------------------------------------------------------------------------------------

struct Context::Impl
{
	TlsConf conf;
	std::vector<std::string> warnings;
};
struct Filter::Impl
{
	State state = State::Failed;
};

Context::Context()
{}
Context::~Context() = default;
std::shared_ptr<Context> Context::Create(const TlsConf&, Role, std::string& error)
{
	error = "\"TLS\": this build has no TLS support: rebuild with ODC_ASIO_SSL=ON";
	return nullptr;
}
bool Context::Stale() const
{
	return false;
}
const std::vector<std::string>& Context::Warnings() const
{
	return impl->warnings;
}
const TlsConf& Context::Conf() const
{
	return impl->conf;
}
Role Context::GetRole() const
{
	return Role::Client;
}

Filter::Filter(std::shared_ptr<Context>, Role, FilterCallbacks, std::string, std::function<Clock::time_point()>):
	impl(std::make_unique<Impl>())
{}
Filter::~Filter() = default;
void Filter::Start()
{}
void Filter::OnTransportData(const uint8_t*, size_t)
{}
void Filter::Write(const uint8_t*, size_t)
{}
void Filter::Shutdown()
{}
void Filter::Tick()
{}
Filter::State Filter::GetState() const
{
	return impl->state;
}
std::optional<PeerInfo> Filter::Peer() const
{
	return std::nullopt;
}
uint64_t Filter::Renegotiations() const
{
	return 0;
}
std::string Filter::HandshakeSummary() const
{
	return "";
}
size_t Filter::PendingPlainBytes() const
{
	return 0;
}

#endif

} //namespace tls
} //namespace odc
