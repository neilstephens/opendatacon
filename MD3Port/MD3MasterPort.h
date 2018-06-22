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
 * MD3MasterPort.h
 *
 *  Created on: 01/04/2018
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef MD3MASTERPORT_H_
#define MD3MASTERPORT_H_

#include <queue>
#include <utility>
#include <opendnp3/master/ISOEHandler.h>
#include <opendatacon/ASIOScheduler.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3Port.h"


class MD3MasterPort: public MD3Port
{
public:
	MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~MD3MasterPort() override;

	void Enable() override;
	void Disable() override;

	void BuildOrRebuild() override;

	void SocketStateHandler(bool state);

	void SetAllPointsQualityToCommsLost();
	void SendAllPointEvents();

	uint8_t CalculateBinaryQuality(bool enabled, MD3Time time);
	uint8_t CalculateAnalogQuality(bool enabled, uint16_t meas, MD3Time time);

	// Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
	void Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	template<typename T> void EventT(T& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback);

	//*** PUBLIC for unit tests only
	//TODO: Add timeout callback and timeout period, plus command succeeded callback to MasterCommandQueueItem to add extra functionality.
	typedef std::vector<MD3BlockData> MasterCommandQueueItem;

	// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
	// The type of the queue will contain a function pointer to the completion function and the timeout function.
	// There is a fixed timeout function (below) which will queue the next command and call any timeout function pointer
	// If the ProcessMD3Message callback gets the command it expects, it will send the next command in the queue.
	// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
	// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
	void QueueMD3Command(MasterCommandQueueItem &CompleteMD3Message);
	// Handle the many single block command messages better
	void QueueMD3Command(MD3BlockFormatted &SingleBlockMD3Message);
	void SendNextMasterCommand();
	void ClearMD3CommandQueue();

private:
//	template<class T>
//	CommandStatus WriteObject(const T& command, uint16_t index);

	void DoPoll(uint32_t pollgroup);
	void ProcessMD3Message(std::vector<MD3BlockData>& CompleteMD3Message);

	void HandleError(int errnum, const std::string& source);
	CommandStatus HandleWriteError(int errnum, const std::string& source);

	std::unique_ptr<ASIOScheduler> PollScheduler;
	void ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message);
	void ProcessAnalogDeltaScanReturn(MD3BlockFormatted & Header, std::vector<MD3BlockData>& CompleteMD3Message);

	std::shared_ptr<StrandProtectedQueue<MasterCommandQueueItem>> pMasterCommandQueue;
	uint8_t CurrentFunctionCode = 0;   // When we send a command, make sure the response we get is one we are waiting for.
	bool ProcessingMD3Command = false; //TODO ProcessingMD3Command Will need to be atomic/and/or mutex protected or become part of the commandqueue class when refactored.

	// Check that what we got is one that is expected for the current Function we are processing.
	bool AllowableResponseToFunctionCode(uint8_t CurrentFunctionCode, uint8_t FunctionCode);
};

#endif /* MD3MASTERPORT_H_ */
