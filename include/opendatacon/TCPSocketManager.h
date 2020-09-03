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
 * TCPSocketManager.h
 *
 *  Created on: 2018-01-20
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

//TODO: probably move the code into a cpp in ODC library

//Helper class for managing access to a TCP socket, either as server or client
//Usage:
//	-- Construct
//	-- Call Open()
//	-- Wait for a connection (state callback)
//	-- Data will continuously be read from socket if available and passed to the read callback, unitl the socket is closed
//	-- Optionally Write() to the socket. Data will be buffered if the connection isn't open.
//	-- If the socket closes for any reason you'll get a state callback
//	-- Call Close() to intentionally close the socket 8-)

#ifndef TCPSOCKETMANAGER
#define TCPSOCKETMANAGER

#include <opendatacon/asio.h>
#include <opendatacon/Platform.h>
#include <opendatacon/util.h>
#include <string>
#include <functional>


namespace odc
{

typedef asio::basic_streambuf<std::allocator<char>> buf_t;

//buffer to track a data container
//T must be a container with a data(), size() and get_allocator() members
template <typename T>
class shared_const_buffer: public asio::const_buffer
{
public:
	shared_const_buffer(std::shared_ptr<T> pCon):
		asio::const_buffer(pCon->data(),pCon->size()*sizeof(pCon->get_allocator())),
		con(pCon)
	{}
	//Implement the ConstBufferSequence requirements
	//(so I don't have to put a single one in a container)
	typedef const shared_const_buffer* const_iterator;
	const_iterator begin() const
	{
		return this;
	}
	const_iterator end() const
	{
		return this + 1;
	}

private:
	std::shared_ptr<T> con;
};

struct TCPKeepaliveOpts
{
	TCPKeepaliveOpts(bool enabled, unsigned int idle_timeout_s, unsigned int retry_interval_s, unsigned int fail_count):
		enabled(enabled),
		idle_timeout_s(idle_timeout_s),
		retry_interval_s(retry_interval_s),
		fail_count(fail_count)
	{}
	bool enabled;
	unsigned int idle_timeout_s;
	unsigned int retry_interval_s;
	unsigned int fail_count;
};

template <typename Q>
class TCPSocketManager
{
public:
	TCPSocketManager
		(std::shared_ptr<odc::asio_service> apIOS,        //pointer to an asio io_service
		const bool aisServer,                             //Whether to act as a server or client
		const std::string& aEndPoint,                     //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                         //Port to connect to if client, or listen on if server
		const std::function<void(buf_t&)>& aReadCallback, //Handler for data read off socket
		const std::function<void(bool)>& aStateCallback,  //Handler for communicating the connection state of the socket
		const size_t abuffer_limit                        //
		      = std::numeric_limits<size_t>::max(),       //maximum number of writes to buffer
		const bool aauto_reopen = false,                  //Keeps the socket open (retry on error), unless you explicitly Close() it
		const uint16_t aretry_time_ms = 0,                //You can specify a fixed retry time if auto_open is enabled, zero means exponential backoff
		const std::function<void(const std::string&)>&    //
		aLogCallback = [](const std::string& m){},        //Handler for log messages
		const bool useKeepalives = true,                  //Set TCP keepalive socket option
		const unsigned int KeepAliveTimeout_s = 599,      //TCP keepalive idle timeout (seconds)
		const unsigned int KeepAliveRetry_s = 10,         //TCP keepalive retry interval (seconds)
		const unsigned int KeepAliveFailcount = 3):       //TCP keepalive fail count
		handler_tracker(std::make_shared<char>()),
		isConnected(false),
		pending_connections(0),
		manuallyClosed(true),
		pIOS(apIOS),
		isServer(aisServer),
		ReadCallback(aReadCallback),
		StateCallback(aStateCallback),
		LogCallback(aLogCallback),
		Keepalives(useKeepalives,KeepAliveTimeout_s,KeepAliveRetry_s,KeepAliveFailcount),
		pSock(nullptr),
		pReadStrand(pIOS->make_strand()),
		pWriteStrand(pIOS->make_strand()),
		pSockStrand(pIOS->make_strand()),
		pRetryTimer(pIOS->make_steady_timer()),
		buffer_limit(abuffer_limit),
		auto_reopen(aauto_reopen),
		retry_time_ms(aretry_time_ms),
		ramp_time_ms(0),
		host_name(aEndPoint),
		service_name(aPort),
		EndpointIterator(),
		resolve_time(msSinceEpoch()),
		pAcceptor(nullptr)
	{}

	void Open()
	{
		auto tracker = handler_tracker;
		pSockStrand->post([this,tracker]()
			{
				manuallyClosed = false;
				if(isConnected)
					return;

				asio::ip::tcp::resolver::iterator end;

				//resolve between connections or every 15mins
				if(EndpointIterator == end || (msSinceEpoch()-resolve_time) > 900000)
				{
				//need to resolve endpoint(s)
				      std::shared_ptr<asio::ip::tcp::resolver> res = pIOS->make_tcp_resolver();
				      res->async_resolve(host_name,service_name,pSockStrand->wrap(
						[this,res,tracker](asio::error_code err_code, asio::ip::tcp::resolver::iterator endpoint_it)
						{
							if(manuallyClosed)
								return;
							if(err_code)
							{
							      LogCallback("Resolution error: "+err_code.message());
							      AutoOpen();
							      return;
							}
							LogCallback("Resolved endpoint(s).");
							EndpointIterator = endpoint_it;
							resolve_time = msSinceEpoch();
							AutoOpen();
						}));
				      return;
				}

				for(auto endpoint_it = EndpointIterator; endpoint_it != end; endpoint_it++)
				{
				      auto addr_str = endpoint_it->endpoint().address().to_string()+":"+std::to_string(endpoint_it->endpoint().port());
				      std::shared_ptr<asio::ip::tcp::socket> pCandidateSock = pIOS->make_tcp_socket();
				      if(isServer)
				      {
				            try
				            {
				                  pAcceptor = pIOS->make_tcp_acceptor(endpoint_it);
				                  auto msg = "Listening on "+addr_str;
				                  LogCallback(msg);
						}
				            catch(std::exception& e)
				            {
				                  auto msg = "Failed to start server on "+addr_str+". Exception: "+e.what();
				                  LogCallback(msg);
				                  if(pending_connections == 0)
								AutoOpen();
				                  return;
						}
				            pending_connections++;
				            pAcceptor->async_accept(*pCandidateSock,pSockStrand->wrap([this,pCandidateSock,addr_str,tracker](asio::error_code err_code)
							{
								pending_connections--;
								if(manuallyClosed || isConnected)
								{
								      LogCallback("Connection dropped on "+addr_str);
								      return;
								}
								if(!err_code)
									LogCallback("Connection accepted on "+addr_str);
								ConnectCompletionHandler(err_code,pCandidateSock,addr_str);
								pAcceptor.reset();
							}));
					}
				      else
				      {
				            auto msg = "Connecting to "+addr_str;
				            LogCallback(msg);
				            pending_connections++;
				            pCandidateSock->async_connect(*endpoint_it,pSockStrand->wrap([this,pCandidateSock,addr_str,tracker](asio::error_code err_code)
							{
								pending_connections--;
								if(manuallyClosed || isConnected)
								{
								      LogCallback("Connection dropped on "+addr_str);
								      return;
								}
								if(!err_code)
									LogCallback("Connection established on "+addr_str);
								ConnectCompletionHandler(err_code,pCandidateSock,addr_str);
							}));
					}
				}
			});
	}
	void Close()
	{
		auto tracker = handler_tracker;
		pSockStrand->post([this,tracker]()
			{
				LogCallback("Connection manual close.");
				manuallyClosed = true;
				ramp_time_ms = 0;
				pRetryTimer->cancel();
				pAcceptor.reset();
				AutoClose();
			});
	}

	template <typename T>
	void Write(T&& aContainer)
	{
		//shared_const_buffer is a ref counted wraper that will delete the data in good time
		auto buf = shared_const_buffer<T>(std::make_shared<T>(std::move(aContainer)));

		auto tracker = handler_tracker;
		pSockStrand->post([this,tracker,buf]()
			{
				if(!isConnected)
				{
				      pWriteStrand->post([this,tracker,buf]()
						{
							writebufs.push_back(buf);
							if(writebufs.size() > buffer_limit)
								writebufs.erase(writebufs.begin());
						});
				      return;
				}

				asio::async_write(*pSock,buf,asio::transfer_all(),pWriteStrand->wrap([this,tracker,buf](asio::error_code err_code, std::size_t n)
					{
						if(err_code)
						{
						      writebufs.push_back(buf);
						      if(writebufs.size() > buffer_limit)
								writebufs.erase(writebufs.begin());
						      LogCallback("Connection async write ("+std::to_string(n)+" bytes) error: "+err_code.message());
						      AutoClose();
						      AutoOpen();
						      return;
						}
					}));
			});
	}

	~TCPSocketManager()
	{
		//There may be some outstanding handlers if we're destroyed right after Close()
		std::weak_ptr<void> tracker = handler_tracker;
		handler_tracker.reset();

		while(!tracker.expired() && !pIOS->stopped())
			pIOS->poll_one();
	}

private:
	std::shared_ptr<void> handler_tracker;
	bool isConnected;
	size_t pending_connections;
	bool manuallyClosed;
	std::shared_ptr<odc::asio_service> pIOS;
	const bool isServer;
	const std::function<void(buf_t&)> ReadCallback;
	const std::function<void(bool)> StateCallback;
	const std::function<void(const std::string&)> LogCallback;
	TCPKeepaliveOpts Keepalives;

	buf_t readbuf;
	std::vector<shared_const_buffer<Q>> writebufs;
	std::shared_ptr<asio::ip::tcp::socket> pSock;

	//Strand to sync access to read buffer
	std::unique_ptr<asio::io_service::strand> pReadStrand;
	//Strand to sync access to write buffers
	std::unique_ptr<asio::io_service::strand> pWriteStrand;
	//Strand to sync access to socket state
	std::unique_ptr<asio::io_service::strand> pSockStrand;

	//for timing open-retries
	std::unique_ptr<asio::steady_timer> pRetryTimer;

	size_t buffer_limit;

	//Auto open funtionality - see constructor for description
	bool auto_reopen;
	uint16_t retry_time_ms;
	uint16_t ramp_time_ms; //keep track of exponential backoff

	//Host/IP and Port are resolved into these:
	const std::string host_name;
	const std::string service_name;
	asio::ip::tcp::resolver::iterator EndpointIterator;
	msSinceEpoch_t resolve_time;
	std::unique_ptr<asio::ip::tcp::acceptor> pAcceptor;

	void ConnectCompletionHandler(asio::error_code err_code, std::shared_ptr<asio::ip::tcp::socket> pCandidateSock, std::string addr_str)
	{
		if(err_code)
		{
			LogCallback("Connection error on "+addr_str+" : "+err_code.message());
			if(pending_connections == 0)
				AutoOpen();
			return;
		}
		pSock = pCandidateSock;
		SetTCPKeepalives(*pSock,Keepalives.enabled,Keepalives.idle_timeout_s,Keepalives.retry_interval_s,Keepalives.fail_count);
		isConnected = true;
		StateCallback(isConnected);
		ramp_time_ms = 0;

		//if there's anything in the buffer sequence, write it
		auto tracker = handler_tracker;
		pWriteStrand->post([this,tracker]()
			{
				if(writebufs.size() > 0)
				{
				      auto n = asio::write(*pSock,writebufs,asio::transfer_all());
				      if(n == 0)
				      {
				            LogCallback("Connection sync write error: Zero bytes written");
				            AutoClose();
				            AutoOpen();
				            return;
					}
				      writebufs.clear();
				}
			});
		Read();
	}
	void Read()
	{
		auto tracker = handler_tracker;
		asio::async_read(*pSock, readbuf, asio::transfer_at_least(1), pReadStrand->wrap([this,tracker](asio::error_code err_code, std::size_t n)
			{
				if(err_code)
				{
				      LogCallback("Connection async read ("+std::to_string(n)+" bytes) error: "+err_code.message());
				      AutoClose();
				      AutoOpen();
				}
				else
				{
				      ReadCallback(readbuf);
				      Read();
				}
			}));
	}
	void AutoOpen()
	{
		auto tracker = handler_tracker;
		pSockStrand->post([this,tracker]()
			{
				if(!auto_reopen || manuallyClosed)
					return;

				if(retry_time_ms != 0)
				{
				      ramp_time_ms = retry_time_ms;
				}

				if(ramp_time_ms == 0)
				{
				      ramp_time_ms = 125;
				      LogCallback("Trying to open socket...");
				      Open();
				}
				else
				{
				      pRetryTimer->expires_from_now(std::chrono::milliseconds(ramp_time_ms));
				      pRetryTimer->async_wait(pSockStrand->wrap([this,tracker](asio::error_code err_code)
						{
							if(manuallyClosed || err_code)
								return;
							ramp_time_ms *= 2;
							LogCallback("Socket open retry.");
							Open();
						}));
				}
			});
	}
	void AutoClose()
	{
		auto tracker = handler_tracker;
		pSockStrand->post([this,tracker]()
			{
				if(!isConnected)
				{
				      return;
				}
				LogCallback("Connection auto close.");
				pSock->close();
				isConnected = false;
				StateCallback(isConnected);
				//Force endpoint(s) resolution between connections
				EndpointIterator = asio::ip::tcp::resolver::iterator();
			});
	}
};

} //namespace odc

#endif // TCPSOCKETMANAGER

