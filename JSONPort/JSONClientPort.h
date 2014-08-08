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
	JSONClientPort(std::string aName, std::string aConfFilename, std::string aConfOverrides);

	void Enable();
	void Disable();

	void BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL);

private:
	asio::basic_streambuf<std::allocator<char>> buf;
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	std::unique_ptr<Timer_t> pTCPRetryTimer;
	void ConnectCompletionHandler(asio::error_code err_code);
	void ReadCompletionHandler(asio::error_code err_code);
	void ProcessBraced(std::string braced);
	template<typename T> void LoadT(T meas, uint16_t index);
	void Read();
};

#endif /* JSONCLIENTDATAPORT_H_ */
