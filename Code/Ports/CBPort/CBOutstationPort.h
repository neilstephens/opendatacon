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
* CBOutStationPort.h
*
*  Created on: 12/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef CBOUTSTATIONPORT_H_
#define CBOUTSTATIONPORT_H_
#include "CB.h"
#include "CBPort.h"
#include "CBUtility.h"
#include "CBConnection.h"
#include "CBPointTableAccess.h"
#include <unordered_map>
#include <vector>
#include <functional>


class OutstationSystemFlags
{
	// CB can support 12 bits of status flags, which are reported by Fn 9 Group 8, system flag scan.
public:
	// This is calculated by checking the digital bit changed flag, using a method registered with us
	bool GetSOEAvailableFlag()
	{
		if (SOEAvailableFn != nullptr)
			return SOEAvailableFn();

		LOGERROR("GetSOEAvailableFlag called without a handler being registered");
		return false;
	}

	// This is calculated by checking the timetagged data queues, using a method registered with us
	bool GetSOEOverflowFlag()
	{
		if (SOEOverflowFn != nullptr)
			return SOEOverflowFn();

		LOGERROR("GetSOEOverflowFlag called without a handler being registered");
		return false;
	}
	uint16_t GetStatusPayload()
	{
		// We cant do this in the above lambda as we might getandset the overflow flag multiple times...
		// Bit Definitions:
		// 0-3 Error Code
		// 4 - Controls Isolated
		// 5 - Reset Occurred
		// 6 - Field Supply Low
		// 7 - Internal Supply Low
		// 8 - Accumulator Overflow Change
		// 9 - Accumulator Overflow Status
		// 10 - SOE Overflow
		// 11 - SOE Data Available
		// We only implement 5, 10, 11 as hard coded points. The rest can be defined as RST points which will be treated like digitals (so you can set their values in the Simulator)
		uint16_t Payload = 0;
		if (ControlIsolate)
		{
			// Bit 4 Controls isolated
			// TODO: Prevent an execute from doing anything when this is set?
			Payload |= (0x1 << 4);
		}
		if (StartUp)
		{
			// Bit 5 Power Up
			Payload |= (0x1 << 5);
			StartUp = false;
		}
		if (GetSOEOverflowFlag())
		{
			// Bit 11 SOE Buffer Full
			Payload |= (0x1 << 10);
		}
		if (GetSOEAvailableFlag())
		{
			// Bit 12 SOE Data available
			Payload |= (0x1 << 11);
		}
		return Payload;
	}

	void SetSOEAvailableFn(std::function<bool(void)> Fn) { SOEAvailableFn = Fn; }
	void SetSOEOverflowFn(std::function<bool(void)> Fn) { SOEOverflowFn = Fn; }
	void SetStartupFlag(bool on) { StartUp = on;  }
	void SetControlIsolateFlag(bool on) { ControlIsolate = on; }

private:
	std::function<bool(void)> SOEAvailableFn = nullptr; // SOE Events Pending
	std::function<bool(void)> SOEOverflowFn = nullptr;  // SOE Overflow

	bool StartUp = true;
	bool ControlIsolate = false;
};



class PendingCommandType
{
public:
	PendingCommandType() {}

	enum CommandType { None, Trip, Close, SetA, SetB };

	const CBTime CommandValidTimemsec = 30000; // PendingCommand valid for 30 seconds..
	CommandType Command = None;
	uint16_t Data = 0;
	CBTime ExpiryTime = 0; // If we dont receive the execute before this time, it will not be executed
};

class CBOutstationPortCollection;

class CBOutstationPort: public CBPort
{
	enum AnalogChangeType { NoChange, DeltaChange, AllChange };
	enum AnalogCounterModuleType { CounterModule, AnalogModule };

public:
	CBOutstationPort(const std::string & aName, const std::string & aConfFilename, const Json::Value & aConfOverrides);
	void UpdateOutstationPortCollection();
	~CBOutstationPort() override;

	void Enable() override;
	void Disable() override final;
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	CommandStatus Perform(const std::shared_ptr<EventInfo>& event, bool waitforresult);

	void SendCBMessage(const CBMessage_t & CompleteCBMessage) override;
	CBMessage_t CorruptCBMessage(const CBMessage_t& CompleteCBMessage);
	void ResendLastCBMessage()
	{
		SendCBMessage(LastSentCBMessage);
	}
	void ProcessCBMessage(CBMessage_t &&CompleteCBMessage);

	// Response to PendingCommand Methods
	void ScanRequest(CBBlockData &Header);
	void FuncTripClose(CBBlockData & Header, PendingCommandType::CommandType pCommand);
	void FuncSetAB(CBBlockData & Header, PendingCommandType::CommandType pCommand);
	void ExecuteCommand(CBBlockData & Header);
	bool ExecuteCommandOnGroup(const PendingCommandType &PendingCommand, uint8_t Group, bool singlecommand);
	bool ExecuteBinaryControl(uint8_t group, uint8_t Channel, bool point_on);
	bool ExecuteAnalogControl(uint8_t group, uint8_t Channel, uint16_t data);
	void FuncMasterStationRequest(CBBlockData &Header, CBMessage_t &CompleteCBMessage);
	void RemoteStatusWordResponse(CBBlockData& Header);
	void FuncReSendSOEResponse(CBBlockData & Header);
	void FuncSendSOEResponse(CBBlockData & Header);

	void ConvertPayloadWordsToCBMessage(CBBlockData & Header, std::vector<uint16_t> &PayloadWords, CBMessage_t &ResponseCBMessage);

	void ConvertBitArrayToPayloadWords(const uint32_t UsedBits, std::array<bool, MaxSOEBits> &BitArray, std::vector<uint16_t> &PayloadWords);

	void BuildPackedEventBitArray(std::array<bool, MaxSOEBits> &BitArray, uint32_t &UsedBits);

	void ProcessUpdateTimeRequest(CBMessage_t & CompleteCBMessage);
	void EchoReceivedHeaderToMaster(CBBlockData & Header);

	void BuildScanRequestResponseData(uint8_t Group, std::vector<uint16_t>& BlockValues);
	uint16_t GetPayload(uint8_t &Group, PayloadLocationType &payloadlocation);

	void MarkAllBinaryPointsAsChanged();
	uint8_t CountBinaryBlocksWithChanges();

	// UI Interactions
	std::pair<std::string, const IUIResponder*> GetUIResponder() final;
	bool UIFailControl(const std::string& active);                // Shift the control response channel from the correct set channel to an alternative channel.
	bool UIRandomReponseBitFlips(const std::string& probability); // Zero probability = does not happen. 1 = there is a bit flip in every response packet.
	bool UIRandomReponseDrops(const std::string& probability);    // Zero probability = does not happen. 1 = there is a drop every response packet.
	bool UISetRTUReStartFlag(const std::string& active);		  // Set the Watchdog Timer bit in the RSW
	bool UISetRTUControlIsolateFlag(const std::string& active);	  // Set the Control Isolate bit in the RSW


	// Testing use only
	PendingCommandType GetPendingCommand(uint8_t group) { return PendingCommands[group & 0x0F]; } // Return a copy, cannot be changed
	int GetSOEOffsetMinutes() { return SOETimeOffsetMinutes; }
private:
	std::shared_ptr<CBOutstationPortCollection> CBOutstationCollection;

	//Unsynchronised version of their public counterpart
	//The public interface synchronises access to these using the strand below
	void Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback);
	void ProcessCBMessage_(CBMessage_t &&CompleteCBMessage);
	//Strand to sync access to the above functions
	std::unique_ptr<asio::io_service::strand> EventSyncExecutor = odc::asio_service::Get()->make_strand();

	// UI Testing flags to cause misbehavior
	bool FailControlResponse = false;
	double BitFlipProbability = 0.0;
	double ResponseDropProbability = 0.0;

	bool SOEOverflowFn(void);
	bool SOEAvailableFn(void);
	int SOETimeOffsetMinutes = 0;

	OutstationSystemFlags SystemFlags;

	void SocketStateHandler(bool state);

	CBMessage_t LastSentCBMessage;
	CBMessage_t LastSentSOEMessage; // For SOE specific resend commands.

	PendingCommandType PendingCommands[16+1]; // Store a potential pending command for each group.
};

#endif
