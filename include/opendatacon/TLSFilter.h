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
 * TLSFilter.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TLS (as in IEC 62351-3) as a filter between a TCP transport and the protocol above it, built on OpenSSL memory BIOs.
 *
 *  It is a pure state machine without sockets or timers of its own. The owner feeds it the bytes
 *  that arrive from the transport (OnTransportData) and the application bytes to send (Write), the passage of time (Tick), and
 *  receives the TLS records to put on the transport (callbacks.send), the decrypted application bytes (callbacks.onPlain) and
 *  the events. All calls must be made from one thread at a time (the owner serialises them, for example on its strand), and the filter must not
 *  be destroyed from within one of its own callbacks (post the destruction).
 *
 *  Input is untrusted: whatever arrives (garbage, truncated or oversized records, alerts, a renegotiation) ends in onFailed or
 *  onPeerClosed, never in undefined behaviour or a hang.
 *
 *  This file compiles in every build. Without ODC_ASIO_SSL (OpenSSL) Context::Create fails with a clear message and nothing else
 *  can be used.
 */

#ifndef ODC_TLSFILTER_H_
#define ODC_TLSFILTER_H_

#include "TLSConf.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odc
{
namespace tls
{

using Bytes = std::vector<uint8_t>;
using Clock = std::chrono::steady_clock;

enum class Role : uint8_t
{
	Client, //the TCP initiator: starts the handshake
	Server  //the TCP listener
};

enum class LogLevel : uint8_t
{
	Debug,
	Info,
	Warn,
	Error
};

//The peer certificate and what was negotiated, available once the handshake completed
struct PeerInfo
{
	std::string protocol;     //"TLSv1.2"
	std::string cipher;       //"ECDHE-ECDSA-AES256-GCM-SHA384"
	bool resumed = false;     //an abbreviated handshake (session resumption)
	bool certificate = false; //the peer presented a certificate
	std::string subject;      //RFC 2253 form
	std::string issuer;
	std::string serial;    //hex
	std::string sha256;    //fingerprint, lower case hex
	std::string notBefore; //ISO 8601 UTC
	std::string notAfter;
	int64_t secondsToExpiry = 0; //negative when expired (only possible when VerifyPeer is false)
};

//The settings and the key material of one port, shared by its filters: certificate, key, CA store, CRLs, options. Create() reads the
//files, so a missing or wrong file is reported there (at configuration time as well as when the port is built).
class Context
{
public:
	//nullptr with a message in error if something is wrong (the message names the key and file)
	static std::shared_ptr<Context> Create(const TlsConf& conf, Role role, std::string& error);
	~Context();
	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;

	//true if a file it was created from changed on disk: a new Context picks up renewed certificates and CRLs
	bool Stale() const;
	//things worth a warning, found when it was created (the certificate expires soon, verification disabled, ...)
	const std::vector<std::string>& Warnings() const;
	const TlsConf& Conf() const;
	Role GetRole() const;

	struct Impl; //the OpenSSL state, private to TLSFilter.cpp
	Impl* Internal() const
	{
		return impl.get();
	}

private:
	Context();
	std::unique_ptr<Impl> impl;
};

struct FilterCallbacks
{
	std::function<void(const Bytes&)> send;              //TLS records to write to the transport (required)
	std::function<void(const uint8_t*, size_t)> onPlain; //decrypted application bytes (required)
	std::function<void(const PeerInfo&)> onEstablished;  //the handshake completed and the peer is acceptable
	std::function<void(const std::string&)> onFailed;    //the handshake failed or the session broke: close the transport. At most once.
	std::function<void()> onPeerClosed;                  //the peer sent close_notify (we answer it): close the transport
	std::function<void(LogLevel, const std::string&)> log;
};

class Filter
{
public:
	enum class State : uint8_t
	{
		Idle, //not started
		Handshaking,
		Established,
		Closed, //we or the peer closed it with close_notify
		Failed
	};

	//peerName: for a client with VerifyHostname, the name or IP address the server's certificate is verified against, and the SNI
	//(if it is not an IP address). clock: the time source for the handshake timeout and the renegotiation interval.
	Filter(std::shared_ptr<Context> context, Role role, FilterCallbacks callbacks, std::string peerName = "",
		std::function<Clock::time_point()> clock = [] { return Clock::now(); });
	~Filter();
	Filter(const Filter&) = delete;
	Filter& operator=(const Filter&) = delete;

	//Begin: the client sends its hello (through callbacks.send); the server starts its handshake timer
	void Start();
	//bytes that arrived from the transport
	void OnTransportData(const uint8_t* data, size_t length);
	//application bytes to send; queued until the handshake completes
	void Write(const uint8_t* data, size_t length);
	//send close_notify; no more data is accepted or delivered
	void Shutdown();
	//the handshake timeout and the renegotiation interval
	void Tick();

	State GetState() const;
	bool Established() const
	{
		return GetState() == State::Established;
	}
	std::optional<PeerInfo> Peer() const;
	//handshakes after the first one (renegotiations, and key updates we initiated) that completed
	uint64_t Renegotiations() const;
	//application bytes queued for the handshake or for a renegotiation
	size_t PendingPlainBytes() const;
	//the handshake as far as it got, compact: "we are the TLS client; sent: ClientHello (...); received: alert fatal X after 0 handshake messages from the peer"
	std::string HandshakeSummary() const;

	struct Impl; //the OpenSSL state, private to TLSFilter.cpp

private:
	std::unique_ptr<Impl> impl;
};

const char* ToString(Filter::State s);

} //namespace tls
} //namespace odc

#endif /* ODC_TLSFILTER_H_ */
