/*	opendatacon
 *
 *	Copyright (c) 2018:
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
 * MD3Port.h
 *
 *  Created on: 1/2/2018
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - derived from DNP3 version
 */

#ifndef MD3PORT_H_
#define MD3PORT_H_
#include "MD3.h"
#include "MD3PortConf.h"
#include "MD3Utility.h"
#include "MD3Connection.h"
#include "StrandProtectedQueue.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <opendatacon/DataPort.h>

using namespace odc;

class MD3Port: public DataPort
{
public:
	MD3Port(const std::string& aName, const std::string & aConfFilename, const Json::Value &aConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) final;

	void Enable() override = 0;
	void Disable() override = 0;
	void Build() override = 0;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override = 0;

	// Public only for UnitTesting
	virtual void SendMD3Message(const MD3Message_t& CompleteMD3Message);
	void SetSendTCPDataFn(std::function<void(std::string)> Send);
	void InjectSimulatedTCPMessage(buf_t & readbuf); // Equivalent of the callback handler in the MD3Connection.

protected:

	TCPClientServer ClientOrServer();
	bool  IsServer();

	bool IsOutStation = true;

	// Worker functions to try and clean up the code...
	MD3PortConf *MyConf;
	std::shared_ptr<MD3PointConf> MyPointConf;

	int Limit(int val, int max);
	uint8_t Limit(uint8_t val, uint8_t max);

	// We need to support multi-drop in both the OutStation and the Master.
	// We have a separate OutStation or Master for each OutStation, but they could be sharing a TCP connection, then routing the traffic based on MD3 Station Address.
	ConnectionTokenType pConnection;
};

#endif
