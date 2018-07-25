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

// The command, and an ODC callback pointer - may be nullptr. We check for that
typedef std::pair <MD3Message_t, SharedStatusCallback_t> MasterCommandQueueItem;

// This class contains the MasterCommandQueue and management variables that all need to be protected using the MasterCommandStrand strand.
// It has a queue of commands to be processed, as well as variables to manage where we are up to in processing the current command.
// As everything is only accessed in strand protected code, we don't need to use a thread safe queue.
class MasterCommandData
{
public:
	unsigned int MaxCommandQueueSize = 20; //TODO: The maximum number of MD3 commands that can be in the master queue? Somewhat arbitrary??
	std::queue<MasterCommandQueueItem> MasterCommandQueue;
	MasterCommandQueueItem CurrentCommand; // Keep a copy of what has been sent to make retries easier.
	uint8_t CurrentFunctionCode = 0;       // When we send a command, make sure the response we get is one we are waiting for.
	bool ProcessingMD3Command = false;
	std::chrono::milliseconds TimerExpireTime;
	pTimer_t CurrentCommandTimeoutTimer = nullptr;
	uint32_t RetriesLeft = 0; // Decrementing counter for retries, if we get to zero move on to the next command.
};

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

	template<class T> void WriteObject(const T& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback);

	// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
	// There is a time out lambda in UnprotectedSendNextMasterCommand which will queue the next command if we timeout.
	// If the ProcessMD3Message callback gets the command it expects, it will send the next command in the queue.
	// If the callback gets an error it will be ignored which will result in a timeout and the next command (or retry) being sent.
	// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
	// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
	void QueueMD3Command(const MD3Message_t &CompleteMD3Message, SharedStatusCallback_t pStatusCallback);
	void PostCallbackCall(const odc::SharedStatusCallback_t &pStatusCallback, CommandStatus c);
	void QueueMD3Command(const MD3BlockFormatted &SingleBlockMD3Message, SharedStatusCallback_t pStatusCallback); // Handle the many single block command messages better

	//*** PUBLIC for unit tests only
	void DoPoll(uint32_t pollgroup);

	void ResetDigitalCommandSequenceNumber();
	int GetAndIncrementDigitalCommandSequenceNumber(); // Thread protected

	void EnablePolling(bool on); // Enabled by default

	void SendTimeDateChangeCommand(const uint64_t &currenttime, SharedStatusCallback_t pStatusCallback);
	void SendDOMOutputCommand(const uint8_t & StationAddress, const uint8_t & ModuleAddress, const uint16_t & outputbits, const SharedStatusCallback_t &pStatusCallback);
	void SendPOMOutputCommand(const uint8_t & StationAddress, const uint8_t & ModuleAddress, const uint8_t & outputselection, const SharedStatusCallback_t &pStatusCallback);

private:

	std::unique_ptr<asio::strand> MasterCommandStrand;
	MasterCommandData MasterCommandProtectedData; // Must be protected by the MasterCommandStrand.

	std::mutex DigitalCommandSequenceNumberMutex;
	int DigitalCommandSequenceNumber = 0; // Used only by the digital commands to manage resends/retries. 0 for power on - connect/reconnect. Will vary from 1 to 15 normally.

	void SendNextMasterCommand();
	void UnprotectedSendNextMasterCommand(bool timeoutoccured);
	void ClearMD3CommandQueue();
	void ProcessMD3Message(MD3Message_t& CompleteMD3Message);

	std::unique_ptr<ASIOScheduler> PollScheduler;
	bool ProcessAnalogUnconditionalReturn( MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message);
	bool ProcessAnalogDeltaScanReturn( MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message);
	bool ProcessAnalogNoChangeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message);

	bool ProcessDigitalNoChangeReturn(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);

	// Handles new COS and Unconditional Scan
	bool ProcessDigitalScan(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);

	bool ProcessDOMReturn(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);
	bool ProcessPOMReturn(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);
	bool ProcessSetDateTimeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message);
	bool ProcessSystemSignOnReturn(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);
	bool ProcessFreezeResetReturn(MD3BlockFormatted & Header, const MD3Message_t & CompleteMD3Message);

	// Check that what we got is one that is expected for the current Function we are processing.
	bool AllowableResponseToFunctionCode(uint8_t CurrentFunctionCode, uint8_t FunctionCode);
};

#endif /* MD3MASTERPORT_H_ */
