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
class shared_const_buffer: public asio::const_buffer
{
public:
	template <typename T> //T must be a container with a data(), size() and get_allocator() members
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
	std::shared_ptr<void> con;
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

struct throttle_data_t
{
	bool is_active;
	std::chrono::microseconds needed_delay;
	std::chrono::system_clock::time_point time_before;
	size_t transfer_size;
};

class TCPSocketManager
{
public:
	TCPSocketManager
		(std::shared_ptr<odc::asio_service> apIOS,                        //pointer to an asio io_service
		const bool aisServer,                                             //Whether to act as a server or client
		const std::string& aEndPoint,                                     //IP addr or hostname (to connect to if client, or bind to if server)
		const std::string& aPort,                                         //Port to connect to if client, or listen on if server
		const std::function<void(buf_t&)>& aReadCallback,                 //Handler for data read off socket
		const std::function<void(bool)>& aStateCallback,                  //Handler for communicating the connection state of the socket
		const size_t abuffer_limit                                        //
		      = std::numeric_limits<size_t>::max(),                       //maximum number of writes to buffer
		const bool aauto_reopen = false,                                  //Keeps the socket open (retry on error), unless you explicitly Close() it
		const uint16_t aretry_time_ms = 0,                                //You can specify a fixed retry time if auto_open is enabled, zero means exponential backoff
		const uint64_t athrottle_bitrate = 0,                             //You can throttle the throughput, zero means don't throttle
		const uint64_t athrottle_chunksize = 0,                           //You can define the max chunksize in bytes to write in one go when throttling, zero means dont chunk writes
		const std::function<void(const std::string&,const std::string&)>& //
		aLogCallback = [](const std::string&, const std::string&){},      //Handler for log messages
		const bool useKeepalives = true,                                  //Set TCP keepalive socket option
		const unsigned int KeepAliveTimeout_s = 599,                      //TCP keepalive idle timeout (seconds)
		const unsigned int KeepAliveRetry_s = 10,                         //TCP keepalive retry interval (seconds)
		const unsigned int KeepAliveFailcount = 3);                       //TCP keepalive fail count

	void Open();
	void Close();

	template <typename Q> //Q must be a container with a data(), size() and get_allocator() members
	void Write(Q&& aContainer)
	{
		//shared_const_buffer is a ref counted wraper that will delete the data in good time
		shared_const_buffer buf(std::make_shared<Q>(std::move(aContainer)));
		Write(buf);
	}

	~TCPSocketManager();

private:
	std::shared_ptr<void> handler_tracker;
	bool isConnected;
	size_t pending_connections;
	bool pending_write;
	bool pending_read;
	bool manuallyClosed;
	std::shared_ptr<odc::asio_service> pIOS;
	const bool isServer;
	const std::function<void(buf_t&)> ReadCallback;
	const std::function<void(bool)> StateCallback;
	const std::function<void(const std::string& level,const std::string& message)> LogCallback;
	TCPKeepaliveOpts Keepalives;

	buf_t readbuf;
	//dual buffers for alternate queuing and passing to asio
	std::vector<shared_const_buffer> writebufs1;
	std::vector<shared_const_buffer> writebufs2;
	std::vector<shared_const_buffer>* queue_writebufs;
	std::vector<shared_const_buffer>* dispatch_writebufs;

	std::shared_ptr<asio::ip::tcp::socket> pSock;
	//Strand to sync access to socket
	std::unique_ptr<asio::io_service::strand> pSockStrand;

	//for timing open-retries
	std::unique_ptr<asio::steady_timer> pRetryTimer;

	//not a size limit, but number of Write() limit
	size_t buffer_limit;

	//Auto open funtionality - see constructor for description
	bool auto_reopen;
	uint16_t retry_time_ms;
	uint16_t ramp_time_ms; //keep track of exponential backoff

	//throttling
	const uint64_t throttle_bitrate;
	const uint64_t throttle_chunksize;

	//Host/IP and Port resolution:
	const std::string host_name;
	const std::string service_name;
	asio::ip::tcp::resolver::iterator EndpointIterator;
	msSinceEpoch_t resolve_time;
	std::unique_ptr<asio::ip::tcp::acceptor> pAcceptor;

	//some stats
	uint64_t write_count = 0;
	uint64_t read_count = 0;

	void Write(shared_const_buffer buf);
	bool EndPointResolved(std::shared_ptr<void> tracker);
	void ServerOpen(std::shared_ptr<asio::ip::tcp::socket> pCandidateSock, asio::ip::tcp::resolver::iterator endpoint_it, std::string addr_str, std::shared_ptr<void> tracker);
	void ClientOpen(std::shared_ptr<asio::ip::tcp::socket> pCandidateSock, asio::ip::tcp::resolver::iterator endpoint_it, std::string addr_str, std::shared_ptr<void> tracker);
	void CheckLastWrite(std::shared_ptr<asio::ip::tcp::socket> pWriteSock, std::string remote_addr_str, std::shared_ptr<void> tracker);
	void ThrottleCheckLastWrite(std::shared_ptr<asio::ip::tcp::socket> pWriteSock, std::string remote_addr_str, throttle_data_t throttle_data, std::shared_ptr<void> tracker);
	void WriteBuffer(std::shared_ptr<asio::ip::tcp::socket> pWriteSock, std::string remote_addr_str, std::shared_ptr<void> tracker);
	void ConnectCompletionHandler(std::shared_ptr<void> tracker, asio::error_code err_code, std::shared_ptr<asio::ip::tcp::socket> pCandidateSock, std::string addr_str, std::string remote_addr_str);
	void Read(std::string remote_addr_str, std::shared_ptr<void> tracker);
	void Open(std::shared_ptr<void> tracker);
	void AutoOpen(std::shared_ptr<void> tracker);
	void AutoClose(std::shared_ptr<void> tracker);
	inline throttle_data_t CheckThrottle(const size_t data_size);
};

} //namespace odc

#endif // TCPSOCKETMANAGER

