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
 * TCPSessionManager.cpp
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  See TCPSessionManager.h. Everything here runs on the strand of the TCPSocketManager (TCPSocketManager::Post()).
 */

#include <opendatacon/TCPSessionManager.h>
#include <thread>

namespace odc
{

namespace
{
const unsigned int AbortAfterCloseMs = 1000;  //how long the peer has to close its side of a connection that we ended
const unsigned int AbortAfterDestroyMs = 250; //the same when the manager is being destroyed

const char* LevelName(tls::LogLevel l)
{
	switch(l)
	{
		case tls::LogLevel::Error: return "error";
		case tls::LogLevel::Warn: return "warning";
		case tls::LogLevel::Info: return "info";
		default: return "debug";
	}
}
}

//the session of one TCP connection
struct TCPSessionManager::Session
{
	uint64_t serial = 0;
	std::unique_ptr<tls::Filter> filter; //TLS only
	bool ended = false;                  //it failed, the peer closed it, or we did: nothing more is passed to or from it
	bool stateUp = false;                //the state callback was called with true for it, and not yet with false
	bool everEstablished = false;
	bool localClose = false; //Close() or CloseConnection() ended it
};

TCPSessionManager::TCPSessionManager(std::shared_ptr<odc::asio_service> apIOS, const bool aisServer, const std::string& aEndPoint,
	const std::string& aPort, const std::function<void(buf_t&)>& aReadCallback, const std::function<void(bool)>& aStateCallback,
	const size_t abuffer_limit, const bool aauto_reopen, const uint16_t aretry_time_ms, const uint64_t athrottle_bitrate,
	const uint64_t athrottle_chunksize, const uint64_t athrottle_writedelay_ms,
	const std::function<void(const std::string&,const std::string&)>& aLogCallback, const bool useKeepalives,
	const unsigned int KeepAliveTimeout_s, const unsigned int KeepAliveRetry_s, const unsigned int KeepAliveFailcount):
	TCPSessionManager(apIOS, aisServer, aEndPoint, aPort, false, nullptr, "", aReadCallback, aStateCallback, abuffer_limit, aauto_reopen,
		aretry_time_ms, athrottle_bitrate, athrottle_chunksize, athrottle_writedelay_ms, aLogCallback, useKeepalives, KeepAliveTimeout_s,
		KeepAliveRetry_s, KeepAliveFailcount, 0)
{}

TCPSessionManager::TCPSessionManager(std::shared_ptr<odc::asio_service> apIOS, const bool aisServer, const std::string& aEndPoint,
	const std::string& aPort, std::shared_ptr<tls::Context> aContext, const std::string& aPeerName,
	const std::function<void(buf_t&)>& aReadCallback, const std::function<void(bool)>& aStateCallback,
	const size_t abuffer_limit, const bool aauto_reopen, const uint16_t aretry_time_ms, const uint64_t athrottle_bitrate,
	const uint64_t athrottle_chunksize, const uint64_t athrottle_writedelay_ms,
	const std::function<void(const std::string&,const std::string&)>& aLogCallback, const bool useKeepalives,
	const unsigned int KeepAliveTimeout_s, const unsigned int KeepAliveRetry_s, const unsigned int KeepAliveFailcount):
	TCPSessionManager(apIOS, aisServer, aEndPoint, aPort, true, std::move(aContext), aPeerName, aReadCallback, aStateCallback, abuffer_limit,
		aauto_reopen, aretry_time_ms, athrottle_bitrate, athrottle_chunksize, athrottle_writedelay_ms, aLogCallback, useKeepalives,
		KeepAliveTimeout_s, KeepAliveRetry_s, KeepAliveFailcount, 0)
{}

TCPSessionManager::TCPSessionManager(std::shared_ptr<odc::asio_service> apIOS, bool aisServer, const std::string& aEndPoint,
	const std::string& aPort, bool aTls, std::shared_ptr<tls::Context> aContext, const std::string& aPeerName,
	const std::function<void(buf_t&)>& aReadCallback, const std::function<void(bool)>& aStateCallback, size_t abuffer_limit,
	bool aauto_reopen, uint16_t aretry_time_ms, uint64_t athrottle_bitrate, uint64_t athrottle_chunksize, uint64_t athrottle_writedelay_ms,
	const std::function<void(const std::string&,const std::string&)>& aLogCallback, bool useKeepalives, unsigned int KeepAliveTimeout_s,
	unsigned int KeepAliveRetry_s, unsigned int KeepAliveFailcount, int):
	pIOS(apIOS),
	tls_mode(aTls),
	role(aisServer ? tls::Role::Server : tls::Role::Client),
	endPoint(aEndPoint),
	peerName(aPeerName),
	context(std::move(aContext)),
	readCallback(aReadCallback),
	stateCallback(aStateCallback),
	logCallback(aLogCallback),
	pTickTimer(pIOS->make_steady_timer())
{
	pSockMan = std::make_unique<TCPSocketManager>(pIOS, aisServer, aEndPoint, aPort,
		[this](buf_t& buf){ OnRead(buf); },
		[this](bool up){ OnState(up); },
		tls_mode ? std::numeric_limits<size_t>::max() : abuffer_limit,
		aauto_reopen, aretry_time_ms, athrottle_bitrate, athrottle_chunksize, athrottle_writedelay_ms, aLogCallback,
		useKeepalives, KeepAliveTimeout_s, KeepAliveRetry_s, KeepAliveFailcount, true /*nothing of one connection is sent on the next*/);
}

TCPSessionManager::~TCPSessionManager()
{
	//close on the strand (TLS: close_notify) and wait for that, running the io service's handlers here if there is nobody else to
	auto done = std::make_shared<std::atomic_bool>(false);
	pSockMan->Post([this,done]()
		{
			DoClose(AbortAfterDestroyMs);
			dead = true;
			done->store(true);
		});
	while(!done->load() && !pIOS->stopped())
		if(!pIOS->poll_one())
			std::this_thread::yield();
	if(!done->load())
		dead = true; //the io service isn't running: nothing else is touching this
	//waits for the handlers (its own and those made with Post() and Wrap()): what they report finds this dead
	pSockMan.reset();
}

void TCPSessionManager::SetSessionEvents(const SessionEvents& e)
{
	events = e;
}

void TCPSessionManager::SetReconnectAfterSessionEnd(bool reconnect)
{
	reconnectAfterEnd = reconnect;
}

void TCPSessionManager::SetTickPeriod(std::chrono::milliseconds period)
{
	if(period.count() > 0)
		tickPeriod = period;
}

void TCPSessionManager::Open()
{
	pSockMan->Post([this]()
		{
			if(dead)
				return;
			wanted = true;
			pSockMan->Open();
		});
}

void TCPSessionManager::Close()
{
	pSockMan->Post([this]()
		{
			if(!dead)
				DoClose(AbortAfterCloseMs);
		});
}

void TCPSessionManager::CloseConnection(uint64_t serial)
{
	pSockMan->Post([this,serial]()
		{
			if(dead || !session || session->ended || session->serial != serial)
				return;
			if(session->filter)
				session->filter->Shutdown(); //close_notify, on this connection
			session->ended = true;
			session->localClose = true;
			//the connection only: Close() ends it (what was written is sent first), Open() connects again once it has ended
			pSockMan->Close(AbortAfterCloseMs);
			pSockMan->Open();
		});
}

uint64_t TCPSessionManager::ConnectionSerial() const
{
	return serial_now.load();
}

bool TCPSessionManager::Connected() const
{
	return connected.load();
}

bool TCPSessionManager::Established() const
{
	return established.load();
}

//---------------------------------------------- on the strand

void TCPSessionManager::TlsLog(const std::string& level, const std::string& message) const
{
	if(logCallback)
		logCallback(level, "TLS: "+message);
}

bool TCPSessionManager::WriteAllowed(size_t length, bool check_serial, uint64_t serial)
{
	if(dead)
		return false;
	if(!session || session->ended || (check_serial && session->serial != serial))
	{
		if(logCallback)
			logCallback("debug","Dropping "+std::to_string(length)+" bytes written when there was no session for them.");
		return false;
	}
	return true;
}

void TCPSessionManager::TlsWrite(const uint8_t* data, size_t length)
{
	session->filter->Write(data, length);
}

void TCPSessionManager::DoClose(unsigned int abort_after_ms)
{
	wanted = false;
	++tickGen;
	pTickTimer->cancel();
	if(session && !session->ended)
	{
		if(session->filter)
			session->filter->Shutdown(); //close_notify, on this connection
		session->ended = true;
		session->localClose = true;
	}
	pSockMan->Close(abort_after_ms); //(what was written is sent first)
}

void TCPSessionManager::OnRead(buf_t& buf)
{
	if(dead || !session || session->ended)
	{
		buf.consume(buf.size());
		return;
	}
	if(!tls_mode)
	{
		if(readCallback)
			readCallback(buf);
		else
			buf.consume(buf.size());
		return;
	}
	auto data = buf.data();
	const size_t n = asio::buffer_size(data);
	for(auto it = asio::buffer_sequence_begin(data); it != asio::buffer_sequence_end(data) && !session->ended; ++it)
		session->filter->OnTransportData(static_cast<const uint8_t*>(it->data()), it->size());
	buf.consume(n);
}

void TCPSessionManager::OnState(bool up)
{
	if(dead)
		return;
	if(!up)
	{
		if(session)
			ConnectionGone();
		if(events.onConnection)
			events.onConnection(false, serial_count);
		return;
	}
	serial_now = ++serial_count;
	readbuf.consume(readbuf.size());
	if(events.onConnection)
		events.onConnection(true, serial_count);
	if(!wanted)
		return; //closed while it was coming up: the socket manager closes it
	StartSession();
}

void TCPSessionManager::StartSession()
{
	const uint64_t serial = serial_count;
	if(!tls_mode)
	{
		session = std::make_unique<Session>();
		session->serial = serial;
		connected = true;
		ReportUp(*session, tls::PeerInfo());
		return;
	}

	std::string refused;
	if(!context)
		refused = "no TLS context";
	else if(context->GetRole() != role)
		refused = "the TLS context was made for the other role than the one of this connection";
	else if(context->Stale())
	{
		//a certificate, key, CA or CRL file changed: use the new ones. A file that can't be loaded now fails the connection (fail closed).
		std::string error;
		auto fresh = tls::Context::Create(context->Conf(), role, error);
		if(!fresh)
			refused = "cannot load the changed certificate files: "+error;
		else
		{
			context = fresh;
			TlsLog("info", "the certificate files changed on disk: reloaded");
			for(const auto& w : context->Warnings())
				TlsLog("warning", w);
		}
	}
	if(!refused.empty())
	{
		Failed(refused);
		NotifyEnded(serial, SessionEnd::Kind::Failed, false, refused, "");
		EndConnection();
		return;
	}

	session = std::make_unique<Session>();
	session->serial = serial;
	Session* s = session.get(); //the filter only calls back while we call it, and the session outlives those calls
	tls::FilterCallbacks cb;
	cb.send = [this](const tls::Bytes& b){ pSockMan->Write(tls::Bytes(b)); }; //on the strand: done now, on this connection
	cb.onPlain = [this,s](const uint8_t* data, size_t length)
			 {
				 if(s->ended || !readCallback || !length)
					 return;
				 auto m = readbuf.prepare(length);
				 asio::buffer_copy(m, asio::buffer(data, length));
				 readbuf.commit(length);
				 readCallback(readbuf);
			 };
	cb.onEstablished = [this,s](const tls::PeerInfo& peer)
				 {
					 if(s->ended)
						 return;
					 if(!events.onEstablished)
					 {
						 std::string text = "established: "+peer.protocol+" "+peer.cipher+(peer.resumed ? " (resumed session)" : "");
						 text += peer.certificate ? ", peer certificate subject \""+peer.subject+"\" valid until "+peer.notAfter : ", the peer presented no certificate";
						 TlsLog("info", text);
					 }
					 ReportUp(*s, peer);
				 };
	cb.onFailed = [this,s](const std::string& why)
			  {
				  if(s->ended)
					  return;
				  Failed(why);
				  EndSession(*s, SessionEnd::Kind::Failed, why);
			  };
	cb.onPeerClosed = [this,s]()
				{
					if(s->ended)
						return;
					if(!events.onEnded)
						TlsLog("info", "the peer closed the TLS connection (close_notify)");
					EndSession(*s, SessionEnd::Kind::PeerClosed, "");
				};
	cb.log = [this](tls::LogLevel l, const std::string& text){ TlsLog(LevelName(l), text); };
	std::string name;
	if(role == tls::Role::Client)
		name = !peerName.empty() ? peerName : (!context->Conf().serverName.empty() ? context->Conf().serverName : endPoint);
	s->filter = std::make_unique<tls::Filter>(context, role, std::move(cb), name);
	connected = true;
	s->filter->Start();
	ScheduleTick();
}

void TCPSessionManager::ReportUp(Session& s, const tls::PeerInfo& peer)
{
	s.stateUp = true;
	s.everEstablished = true;
	established = true;
	if(events.onEstablished)
		events.onEstablished(peer, s.serial);
	if(stateCallback)
		stateCallback(true);
}

void TCPSessionManager::ReportDown(Session& s)
{
	if(!s.stateUp)
		return;
	s.stateUp = false;
	established = false;
	if(stateCallback)
		stateCallback(false);
}

void TCPSessionManager::NotifyEnded(uint64_t serial, SessionEnd::Kind kind, bool wasEstablished, const std::string& reason, const std::string& summary)
{
	if(!events.onEnded)
		return;
	SessionEnd e;
	e.kind = kind;
	e.serial = serial;
	e.established = wasEstablished;
	e.reason = reason;
	e.handshakeSummary = summary;
	events.onEnded(e);
}

void TCPSessionManager::Failed(const std::string& why)
{
	if(!events.onEnded) //(with an onEnded callback the owner reports it)
		TlsLog("error", why);
}

//a TLS session failed or the peer closed it: the connection is ended (the manager reconnects if it is to). The session itself is destroyed
//when the connection is gone, never from within one of its own callbacks.
void TCPSessionManager::EndSession(Session& s, SessionEnd::Kind kind, const std::string& reason)
{
	s.ended = true;
	++tickGen;
	NotifyEnded(s.serial, kind, s.everEstablished, reason, "");
	ReportDown(s);
	EndConnection();
}

void TCPSessionManager::EndConnection()
{
	//Close() ends it (the alert or close_notify is sent first), Open() connects again once it has ended
	pSockMan->Close(AbortAfterCloseMs);
	if(reconnectAfterEnd)
		pSockMan->Open();
	else
		wanted = false;
}

void TCPSessionManager::ConnectionGone()
{
	auto s = std::move(session);
	connected = false;
	++tickGen;
	pTickTimer->cancel();
	if(s->localClose)
		NotifyEnded(s->serial, SessionEnd::Kind::ClosedLocally, s->everEstablished, "", "");
	else if(!s->ended)
	{
		std::string reason = "the TCP connection ended";
		std::string summary;
		if(s->filter && s->filter->GetState() == tls::Filter::State::Handshaking)
		{
			summary = s->filter->HandshakeSummary();
			reason = "the peer closed the TCP connection during the TLS handshake"+
			         std::string(role == tls::Role::Client ? " (is the peer using TLS on this port?)" : "")+" ["+summary+"]";
			Failed(reason);
		}
		else if(s->filter && !events.onEnded)
			TlsLog("info", reason);
		s->ended = true;
		NotifyEnded(s->serial, SessionEnd::Kind::ConnectionLost, s->everEstablished, reason, summary);
	}
	ReportDown(*s);
}

void TCPSessionManager::ScheduleTick()
{
	const auto gen = ++tickGen;
	pTickTimer->expires_from_now(tickPeriod);
	pTickTimer->async_wait(pSockMan->Wrap([this,gen](std::error_code ec)
		{
			if(ec || dead || gen != tickGen || !session || session->ended)
				return;
			session->filter->Tick(); //the handshake timeout and the renegotiation interval
			if(session && !session->ended)
				ScheduleTick();
		}));
}

} //namespace odc
