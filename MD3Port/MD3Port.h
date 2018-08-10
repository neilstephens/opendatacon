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
 *      Author: Scott Ellis <scott.ellis@novatex.com.au> - derived from MD3 version
 */

#ifndef MD3PORT_H_
#define MD3PORT_H_

#include <unordered_map>
#include <vector>
#include <functional>

#include <opendatacon/DataPort.h>

#include "MD3.h"
#include "MD3PortConf.h"
#include "MD3Engine.h"
#include "MD3Connection.h"
#include "StrandProtectedQueue.h"

using namespace odc;

class MD3Connection;

class MD3Port: public DataPort
{
public:
	MD3Port(const std::string& aName, const std::string & aConfFilename, const Json::Value &aConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) final;

	void Enable() override =0;
	void Disable() override =0;
	void Build() override =0;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); }

	// Public only for UnitTesting

	// Methods to access the outstation point table
	//TODO: Point container access extract to separate class..
	bool GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, bool &hasbeenset);
	bool GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool &hasbeenset);
	bool SetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetCounterODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & res);
	bool SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, bool & hasbeenset);
	bool GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool & hasbeenset);
	bool SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetAnalogValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);
	bool SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool GetAnalogODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res);
	bool GetBinaryODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res);

	bool GetBinaryQualityUsingMD3Index(const uint16_t module, const uint8_t channel, bool & hasbeenset); //TODO: Clean up quality on master for bits

	bool GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed);
	bool GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t & res);
	bool GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed);
	bool SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas, bool & valuechanged);
	bool GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed);
	bool SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, MD3Time eventtime);

	bool GetBinaryControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & index);
	bool GetBinaryControlMD3IndexUsingODCIndex(const size_t index, uint8_t & module, uint8_t & channel, BinaryPointType & pointtype);

	bool GetAnalogControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & index);

	void AddToDigitalEvents(MD3BinaryPoint & pt);
	uint16_t CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool & ModuleFailed);
	uint16_t CollectModuleBitsIntoWord(const uint8_t ModuleAddress, bool & ModuleFailed);

	virtual void SendMD3Message(const MD3Message_t& CompleteMD3Message);
	void SetSendTCPDataFn(std::function<void(std::string)> Send);
	void InjectSimulatedTCPMessage(buf_t & readbuf); // Equivalent of the callback handler in the MD3Connection.

protected:

	TCPClientServer ClientOrServer();
	bool  IsServer();

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr; // nullptr normally. Set to hook function for testing

	// Worker functions to try and clean up the code...
	MD3PortConf* MyConf();
	std::shared_ptr<MD3PointConf> MyPointConf();
	int Limit(int val, int max);

	// We need to support multi-drop in both the OutStation and the Master.
	// We have a separate OutStation or Master for each OutStation, but they could be sharing a TCP connection, then routing the traffic based on MD3 Station Address.
	std::shared_ptr<MD3Connection> pConnection;

	// Only used in outstation
	std::shared_ptr<StrandProtectedQueue<MD3BinaryPoint>> pBinaryTimeTaggedEventQueue;       // Separate queue for time tagged binary events. Used for OLD Binary functions
	std::shared_ptr<StrandProtectedQueue<MD3BinaryPoint>> pBinaryModuleTimeTaggedEventQueue; // Used for new Binary. This queue needs to snapshot all 16 bits in the module at the time any one bit  is set. Really wierd
};

#endif /* MD3PORT_H_ */
