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
 * TCPSessionManager.h
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  A TCPSocketManager with a session on each TCP connection: plain, or TLS (as in IEC 62351-3). Connections are numbered (the serial);
 *  a write is only sent on a live session, never kept for a later connection, and Write(data, serial) and CloseConnection(serial) only
 *  act on the connection with that serial.
 *
 *  For TLS, make the context once with tls::Context::Create() (the files are read then, so a wrong one is reported at configuration time)
 *  and construct with it. The TCP listener is the TLS server, the TCP initiator the TLS client. A TLS session that fails or that the peer
 *  closes ends that connection only. A Context that is Stale() is made again for the next connection; if that fails the connection fails closed.
 *
 *  Everything runs on the strand of the TCPSocketManager, callbacks included, one at a time and never from within Open(), Close() or Write(),
 *  which can be called from any thread. The destructor waits for the handlers, so must not be called from a callback.
 */

#ifndef ODC_TCPSESSIONMANAGER_H_
#define ODC_TCPSESSIONMANAGER_H_

#include <opendatacon/TCPSocketManager.h>
#include <opendatacon/TLSFilter.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

namespace odc
{

class TCPSessionManager
{
public:
	//Plain TCP
	TCPSessionManager
		(std::shared_ptr<odc::asio_service> apIOS,
		const bool aisServer,
		const std::string& aEndPoint,
		const std::string& aPort,
		const std::function<void(buf_t&)>& aReadCallback,
		const std::function<void(bool)>& aStateCallback,
		const size_t abuffer_limit = std::numeric_limits<size_t>::max(),
		const bool aauto_reopen = false,
		const uint16_t aretry_time_ms = 0,
		const uint64_t athrottle_bitrate = 0,
		const uint64_t athrottle_chunksize = 0,
		const uint64_t athrottle_writedelay_ms = 0,
		const std::function<void(const std::string&,const std::string&)>& aLogCallback = [] (const std::string&, const std::string&){},
		const bool useKeepalives = true,
		const unsigned int KeepAliveTimeout_s = 599,
		const unsigned int KeepAliveRetry_s = 10,
		const unsigned int KeepAliveFailcount = 3);

	//TLS. The other arguments are those of TCPSocketManager (buffer_limit is not applied: a TLS stream can't lose records).
	TCPSessionManager
		(std::shared_ptr<odc::asio_service> apIOS,
		const bool aisServer,
		const std::string& aEndPoint,
		const std::string& aPort,
		std::shared_ptr<tls::Context> aContext,           //made with the role that matches aisServer; none: every connection fails
		const std::string& aPeerName,                     //client: the name the server's certificate is verified against (if the context says to) and sent as SNI; empty = the context's ServerName, or aEndPoint
		const std::function<void(buf_t&)>& aReadCallback, //the decrypted data
		const std::function<void(bool)>& aStateCallback,  //true = the handshake completed, false = that session is gone
		const size_t abuffer_limit = std::numeric_limits<size_t>::max(),
		const bool aauto_reopen = false,
		const uint16_t aretry_time_ms = 0,
		const uint64_t athrottle_bitrate = 0,
		const uint64_t athrottle_chunksize = 0,
		const uint64_t athrottle_writedelay_ms = 0,
		const std::function<void(const std::string&,const std::string&)>& aLogCallback = [] (const std::string&, const std::string&){}, //the TLS session's start with "TLS: "
		const bool useKeepalives = true,
		const unsigned int KeepAliveTimeout_s = 599,
		const unsigned int KeepAliveRetry_s = 10,
		const unsigned int KeepAliveFailcount = 3);

	TCPSessionManager(const TCPSessionManager&) = delete;
	TCPSessionManager& operator=(const TCPSessionManager&) = delete;
	~TCPSessionManager();

	//How a session ended
	struct SessionEnd
	{
		enum class Kind
		{
			Failed,         //TLS failed the session: the handshake or the certificate, the peer sent something that isn't TLS, the TLS context can't be used
			PeerClosed,     //TLS: the peer sent close_notify
			ConnectionLost, //the TCP connection ended without either of them (the peer's close or a network failure)
			ClosedLocally   //Close() or CloseConnection() ended it
		};
		Kind kind = Kind::Failed;
		uint64_t serial = 0;          //the connection it was on
		bool established = false;     //it had been established
		std::string reason;           //Failed: why (what the manager would log). ConnectionLost: a generic text. Empty otherwise
		std::string handshakeSummary; //TLS, ConnectionLost during the handshake: what had been exchanged (tls::Filter::HandshakeSummary())
	};

	//Optional, before Open(), all called on the manager's strand. An event that has a callback here isn't logged by the manager.
	struct SessionEvents
	{
		//A TCP connection came up (true; its session isn't established yet) or went away (false), with its serial: true before anything
		//else about the connection, false after the last event of its session
		std::function<void(bool up, uint64_t serial)> onConnection;
		//The session of connection `serial` is established (TLS: with the peer certificate; plain: an empty PeerInfo), just before the state callback
		std::function<void(const tls::PeerInfo&, uint64_t serial)> onEstablished;
		//The session of a connection ended, once for each (just before the state callback is called with false). A TLS connection that is refused
		//because the context can't be used reports Failed as well, though there is no session.
		std::function<void(const SessionEnd&)> onEnded;
	};
	void SetSessionEvents(const SessionEvents& events);

	//Optional, before Open(): by default the manager connects again (as TCPSocketManager does with auto_reopen) after a TLS session failed or
	//the peer closed it. With false it closes instead, as Close() does, and the owner decides when to Open() again (for a back-off, say).
	//A TCP connection that is lost is still reconnected by the manager.
	void SetReconnectAfterSessionEnd(bool reconnect);

	//Optional, before Open(): how often tls::Filter::Tick() is called while there is a TLS session (handshake timeout, renegotiation). Default 1 s.
	void SetTickPeriod(std::chrono::milliseconds period);

	void Open();
	//Ends the session (TLS: close_notify), closes the connection and stops reconnecting, as TCPSocketManager::Close()
	void Close();
	//Ends the session of connection `serial` (TLS: close_notify) and that connection, but not the manager: with auto_reopen it connects again.
	//Ignored if that isn't the live connection.
	void CloseConnection(uint64_t serial);

	//Data to send on the current session (TLS: queued until the handshake completes). Dropped if there is no session, or it has ended:
	//it is never kept for a later connection. Q must be a container with data(), size() and get_allocator() members.
	template <typename Q>
	void Write(Q&& aContainer)
	{
		Enqueue(std::forward<Q>(aContainer), false, 0);
	}
	//Same, but only if the session is the one of connection `serial`: otherwise dropped, however late it is written
	template <typename Q>
	void Write(Q&& aContainer, uint64_t serial)
	{
		Enqueue(std::forward<Q>(aContainer), true, serial);
	}

	//The serial of the latest TCP connection: 1, 2, 3 ... in the order they come up (0: none yet)
	uint64_t ConnectionSerial() const;
	//The TCP connection is up with a session, established or not (TLS: possibly still handshaking)
	bool Connected() const;
	//The session is established (the state callback was last called with true)
	bool Established() const;
	bool IsTls() const
	{
		return tls_mode;
	}

private:
	TCPSessionManager(std::shared_ptr<odc::asio_service> apIOS, bool aisServer, const std::string& aEndPoint, const std::string& aPort,
		bool aTls, std::shared_ptr<tls::Context> aContext, const std::string& aPeerName,
		const std::function<void(buf_t&)>& aReadCallback, const std::function<void(bool)>& aStateCallback, size_t abuffer_limit,
		bool aauto_reopen, uint16_t aretry_time_ms, uint64_t athrottle_bitrate, uint64_t athrottle_chunksize, uint64_t athrottle_writedelay_ms,
		const std::function<void(const std::string&,const std::string&)>& aLogCallback, bool useKeepalives, unsigned int KeepAliveTimeout_s,
		unsigned int KeepAliveRetry_s, unsigned int KeepAliveFailcount, int);

	struct Session;

	template <typename Q>
	void Enqueue(Q&& aContainer, bool check_serial, uint64_t serial)
	{
		pSockMan->Post([this,data{std::decay_t<Q>(std::forward<Q>(aContainer))},check_serial,serial]() mutable
			{
				const auto length = data.size()*sizeof(*data.data());
				if(!WriteAllowed(length,check_serial,serial))
					return;
				if(tls_mode)
					TlsWrite(reinterpret_cast<const uint8_t*>(data.data()),length);
				else
					pSockMan->Write(std::move(data));
			});
	}

	//---- on the strand
	bool WriteAllowed(size_t length, bool check_serial, uint64_t serial);
	void TlsWrite(const uint8_t* data, size_t length);
	void OnState(bool up);
	void OnRead(buf_t& buf);
	void StartSession();
	void ConnectionGone();
	void EndSession(Session& s, SessionEnd::Kind kind, const std::string& reason);
	void EndConnection();
	void ReportUp(Session& s, const tls::PeerInfo& peer);
	void ReportDown(Session& s);
	void NotifyEnded(uint64_t serial, SessionEnd::Kind kind, bool wasEstablished, const std::string& reason, const std::string& summary);
	void Failed(const std::string& why);
	void DoClose(unsigned int abort_after_ms);
	void ScheduleTick();
	void TlsLog(const std::string& level, const std::string& message) const;

	std::shared_ptr<odc::asio_service> pIOS;
	const bool tls_mode;
	const tls::Role role;
	const std::string endPoint;
	const std::string peerName;
	std::shared_ptr<tls::Context> context;
	const std::function<void(buf_t&)> readCallback;
	const std::function<void(bool)> stateCallback;
	const std::function<void(const std::string&,const std::string&)> logCallback;
	SessionEvents events;                       //set before Open()
	bool reconnectAfterEnd = true;              //set before Open()
	std::chrono::milliseconds tickPeriod{1000}; //set before Open()

	//only on the strand
	bool wanted = false; //Open() was called, Close() was not
	bool dead = false;   //being destroyed
	uint64_t serial_count = 0;
	std::unique_ptr<Session> session;
	uint64_t tickGen = 0;
	buf_t readbuf;
	std::unique_ptr<odc::steady_timer, deleter> pTickTimer;

	//readable from anywhere
	std::atomic<uint64_t> serial_now{0};
	std::atomic<bool> connected{false};
	std::atomic<bool> established{false};

	std::unique_ptr<TCPSocketManager> pSockMan; //destroyed first, explicitly
};

} //namespace odc

#endif /* ODC_TCPSESSIONMANAGER_H_ */
