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
 * CBMasterPort.h
 *
 *  Created on: 15/09/2018
 *      Author: Scott Ellis <scott.ellis@novatex.com.au>
 */

#ifndef CBMASTERPORT_H_
#define CBMASTERPORT_H_

#include <queue>
#include <utility>
#include <opendatacon/ASIOScheduler.h>

#include "CB.h"
#include "CBUtility.h"
#include "CBPort.h"
#include "CBPointTableAccess.h"

// The command, and an ODC callback pointer - may be nullptr. We check for that
typedef std::pair <CBMessage_t, SharedStatusCallback_t> MasterCommandQueueItem;

// This class contains the MasterCommandQueue and management variables that all need to be protected using the MasterCommandStrand strand.
// It has a queue of commands to be processed, as well as variables to manage where we are up to in processing the current command.
// As everything is only accessed in strand protected code, we don't need to use a thread safe queue.
class MasterCommandData
{
public:
	unsigned int MaxCommandQueueSize = 20; //TODO: The maximum number of CB commands that can be in the master queue? Somewhat arbitrary??
	std::queue<MasterCommandQueueItem> MasterCommandQueue;
	MasterCommandQueueItem CurrentCommand; // Keep a copy of what has been sent to make retries easier.
	uint8_t CurrentFunctionCode = 0;       // When we send a command, make sure the response we get is one we are waiting for.
	bool ProcessingCBCommand = false;
	std::chrono::milliseconds TimerExpireTime = std::chrono::milliseconds(0);
	pTimer_t CurrentCommandTimeoutTimer = nullptr;
	uint32_t RetriesLeft = 0; // Decrementing counter for retries, if we get to zero move on to the next command.
};

class CBMasterPort: public CBPort
{
public:
	CBMasterPort(const std::string &aName, const std::string &aConfFilename, const Json::Value &aConfOverrides);
	~CBMasterPort() override;

	void Enable() override;
	void Disable() override;
	void Build() override;
	void SendCBMessage(const CBMessage_t & CompleteCBMessage) override;

	void SocketStateHandler(bool state);

	void SetAllPointsQualityToCommsLost();
	void SendAllPointEvents();

	void Event(std::shared_ptr<const EventInfo> event, const std::string & SenderName, SharedStatusCallback_t pStatusCallback) override;
	void WriteObject(const ControlRelayOutputBlock & command, const uint32_t & index, const SharedStatusCallback_t& pStatusCallback);
	void WriteObject(const int16_t & command, const uint32_t & index, const SharedStatusCallback_t& pStatusCallback);
	void WriteObject(const int32_t & command, const uint32_t & index, const SharedStatusCallback_t& pStatusCallback);
	void WriteObject(const float& command, const uint32_t & index, const SharedStatusCallback_t& pStatusCallback);
	void WriteObject(const double& command, const uint32_t & index, const SharedStatusCallback_t& pStatusCallback);

	// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
	// There is a time out lambda in UnprotectedSendNextMasterCommand which will queue the next command if we timeout.
	// If the ProcessCBMessage callback gets the command it expects, it will send the next command in the queue.
	// If the callback gets an error it will be ignored which will result in a timeout and the next command (or retry) being sent.
	// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
	// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
	void QueueCBCommand(const CBMessage_t &CompleteCBMessage, SharedStatusCallback_t pStatusCallback);
	void QueueCBCommand(const CBBlockData & SingleBlockCBMessage, SharedStatusCallback_t pStatusCallback); // Handle the many single block command messages better
	void PostCallbackCall(const odc::SharedStatusCallback_t &pStatusCallback, CommandStatus c);


	//*** PUBLIC for unit tests only
	void DoPoll(uint32_t payloadlocation);

	void ResetDigitalCommandSequenceNumber();
	uint8_t GetAndIncrementDigitalCommandSequenceNumber(); // Thread protected

	void EnablePolling(bool on); // Enabled by default

//	void SendTimeDateChangeCommand(const uint64_t &currenttime, SharedStatusCallback_t pStatusCallback);
//	void SendNewTimeDateChangeCommand(const uint64_t & currenttimeinmsec, int utcoffsetminutes, SharedStatusCallback_t pStatusCallback);
//	void SendSystemFlagScanCommand(SharedStatusCallback_t pStatusCallback);

	void SendDOMOutputCommand(const uint8_t & StationAddress, const uint8_t & Group, const uint16_t & outputbits, const SharedStatusCallback_t &pStatusCallback);

	// Testing use only
	CBPointTableAccess *GetPointTable() { return &(MyPointConf->PointTable); }
private:

	std::unique_ptr<asio::strand> MasterCommandStrand;
	MasterCommandData MasterCommandProtectedData; // Must be protected by the MasterCommandStrand.

	std::mutex DigitalCommandSequenceNumberMutex;
	uint8_t DigitalCommandSequenceNumber = 0; // Used only by the digital commands to manage resends/retries. 0 for power on - connect/reconnect. Will vary from 1 to 15 normally.

	void SendNextMasterCommand();
	void UnprotectedSendNextMasterCommand(bool timeoutoccured);
	void ClearCBCommandQueue();
	void ProcessCBMessage(CBMessage_t& CompleteCBMessage);

	std::unique_ptr<ASIOScheduler> PollScheduler;
/*	bool ProcessAnalogUnconditionalReturn(  const CBMessage_t& CompleteCBMessage);
      bool ProcessAnalogDeltaScanReturn(  const CBMessage_t& CompleteCBMessage);
      bool ProcessAnalogNoChangeReturn( const CBMessage_t& CompleteCBMessage);

      bool ProcessDigitalNoChangeReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessDigitalScan( const CBMessage_t & CompleteCBMessage); // Handles new COS and Unconditional Scan

      void GenerateODCEventsFromCBModuleWord(const uint16_t &ModuleData, const uint8_t &Group, const CBTime &eventtime);

      bool ProcessDOMReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessPOMReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessAOMReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessSetDateTimeReturn( const CBMessage_t& CompleteCBMessage);
      bool ProcessSetDateTimeNewReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessSystemSignOnReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessFreezeResetReturn( const CBMessage_t & CompleteCBMessage);
      bool ProcessFlagScanReturn( const CBMessage_t & CompleteCBMessage);
*/
	QualityFlags  CalculateBinaryQuality(bool enabled, CBTime time);
	QualityFlags  CalculateAnalogQuality(bool enabled, uint16_t meas, CBTime time);
};

#endif
