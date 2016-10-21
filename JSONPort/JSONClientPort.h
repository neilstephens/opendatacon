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
 * JSONClientDataPort.h
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONCLIENTDATAPORT_H_
#define JSONCLIENTDATAPORT_H_

#include <asio.hpp>
#include "JSONPort.h"

class JSONClientPort: public JSONPort
{
public:
	JSONClientPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	void Enable();
	void Disable();

	template<typename T> std::future<opendnp3::CommandStatus> EventT(const T& meas, uint16_t index, const std::string& SenderName);
	template<typename T> std::future<opendnp3::CommandStatus> EventQ(const T& qual, uint16_t index, const std::string& SenderName);

	std::future<opendnp3::CommandStatus> Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName) ;
	std::future<opendnp3::CommandStatus> Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName) ;

	std::future<opendnp3::CommandStatus> Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName) ;

	std::future<opendnp3::CommandStatus> Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName) ;
	std::future<opendnp3::CommandStatus> Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName) ;

	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

private:
	asio::basic_streambuf<std::allocator<char> > readbuf;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTCPRetryTimer;
	void ConnectCompletionHandler(asio::error_code err_code);
	void ReadCompletionHandler(asio::error_code err_code);
	void ProcessBraced(std::string braced);
	template<typename T> void LoadT(T meas, uint16_t index, Json::Value timestamp_val);
	void Read();

	std::deque<std::string> write_queue;
	std::unique_ptr<asio::strand> pWriteQueueStrand;
	void QueueWrite(const std::string& message);
	void Write();
	void WriteCompletionHandler(asio::error_code err_code, size_t bytes_written);
};

#endif /* JSONCLIENTDATAPORT_H_ */
