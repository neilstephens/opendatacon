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
 * JSONDataPort.h
 *
 *  Created on: 22/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONDATAPORT_H_
#define JSONDATAPORT_H_
#include "JSONPortConf.h"
#include <unordered_map>
#include <opendatacon/DataPort.h>
#include <opendatacon/TCPSocketManager.h>

using namespace odc;

class JSONPort: public DataPort
{
public:
	JSONPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides, bool aisServer);

	void ProcessElements(const Json::Value& JSONRoot) override;

	void Enable() override;
	void Disable() override;

	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

private:
	bool isServer;
	std::unique_ptr<TCPSocketManager<std::string>> pSockMan;
	void SocketStateHandler(bool state);
	void ReadCompletionHandler(buf_t& readbuf);
	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
	void ProcessBraced(const std::string& braced);
};

#endif /* JSONDATAPORT_H_ */
