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
* CBOutStationPortHandlers.cpp
*
*  Created on: 12/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

/* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
 This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/CB) will turn back into scada commands and send out to the "real" Outstation.
 So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
 So an Event to an outstation will be data that needs to be sent up to the scada master.
 An event from an outstation will be a master control signal to turn something on or off.
*/
#include "CB.h"
#include "Log.h"
#include "CBUtility.h"
#include "CBOutstationPort.h"
#include "CBOutStationPortCollection.h"
#include <chrono>
#include <future>
#include <iostream>
#include <regex>

//Synchronise processing the messages with the internal events
void CBOutstationPort::ProcessCBMessage(CBMessage_t &&CompleteCBMessage)
{
	EventSyncExecutor->post([this,msg{std::move(CompleteCBMessage)}]() mutable
		{
			ProcessCBMessage_(std::move(msg));
		});
}

//this should only be called by sync'd function above
void CBOutstationPort::ProcessCBMessage_(CBMessage_t &&CompleteCBMessage)
{
	if (!enabled.load()) return; // Port Disabled so dont process

	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteCBMessage.size() != 0);

	CBBlockData &Header = CompleteCBMessage[0];
	// Now based on the PendingCommand Function, take action. Some of these are responses from - not commands to an OutStation.

	bool NotImplemented = false;

	// All are included to allow better error reporting.
	// The Python simulator only implements the commands below:
	//  '0': 'scan data',
	//	'1' : 'execute',
	//	'2' : 'trip/close',
	//	'4' : 'trip/close',
	//	'9' : 'master station request'
	switch (Header.GetFunctionCode())
	{
		case FUNC_SCAN_DATA:
			ScanRequest(Header); // Fn - 0
			break;

		case FUNC_EXECUTE_COMMAND:
			// The other functions below will setup what needs to be executed, has to be executed within a certain time...
			ExecuteCommand(Header);
			break;

		case FUNC_TRIP:
			// Binary Control
			FuncTripClose(Header, PendingCommandType::CommandType::Trip);
			break;

		case FUNC_SETPOINT_A:
			FuncSetAB(Header, PendingCommandType::CommandType::SetA);
			break;

		case FUNC_CLOSE:
			// Binary Control
			FuncTripClose(Header, PendingCommandType::CommandType::Close);
			break;

		case FUNC_SETPOINT_B:
			FuncSetAB(Header, PendingCommandType::CommandType::SetB);
			break;

		case FUNC_RESET: // Not sure....
			NotImplemented = true;
			break;

		case FUNC_MASTER_STATION_REQUEST:
			// Has many sub messages, determined by Group Number
			// This is the only code that has extra Master to RTU blocks
			FuncMasterStationRequest(Header, CompleteCBMessage);
			break;

		case FUNC_SEND_NEW_SOE:
			FuncSendSOEResponse(Header);
			break;

		case FUNC_REPEAT_SOE:
			// Resend the last SOE message
			FuncReSendSOEResponse(Header);
			break;
		case FUNC_UNIT_RAISE_LOWER:
			NotImplemented = true;
			break;
		case FUNC_FREEZE_AND_SCAN_ACC:
			// Warm Restart (from python sim)??
			NotImplemented = true;
			break;
		case FUNC_FREEZE_SCAN_AND_RESET_ACC:
			// Cold restart (from python sim)??
			NotImplemented = true;
			break;

		default:
			Log.Error("{} Unknown PendingCommand Function - {} On Station Address - {}",Name, Header.GetFunctionCode(), Header.GetStationAddress());
			break;
	}
	if (NotImplemented == true)
	{
		Log.Error("{} PendingCommand Function NOT Implemented - {} On Station Address - {}",Name,Header.GetFunctionCode(),Header.GetStationAddress());
	}
}

#ifdef _MSC_VER
#pragma region RESPONSE METHODS
#endif

void CBOutstationPort::ScanRequest(CBBlockData &Header)
{
	Log.Debug("{} - ScanRequest - Fn0 - Group {}",Name, Header.GetGroup());

	// Assemble the block values A and B in order ready to be placed into the response message.
	std::vector<uint16_t> BlockValues;

	// Use the group definition to assemble the scan data
	BuildScanRequestResponseData(Header.GetGroup(), BlockValues);

	// Now assemble and return the required response..
	CBMessage_t ResponseCBMessage;
	uint32_t index = 0;
	uint16_t FirstBlockBValue = 0x000; // Default

	if (BlockValues.size() > 0)
	{
		FirstBlockBValue = BlockValues[index++];
	}

	// The first block is mostly an echo of the request, except that the B field contains data.
	auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), FirstBlockBValue, BlockValues.size() < 2);
	ResponseCBMessage.push_back(firstblock);

	// index will be 1 when we get to here..
	while (index < BlockValues.size())
	{
		uint16_t A = BlockValues[index++];

		// If there is no B value, default to zero.
		uint16_t B = 0x00;
		if (index < BlockValues.size())
		{
			B = BlockValues[index++]; // This should always be triggered as our BuildScanResponseData should give us a full message. We handle the missing case anyway!
		}
		auto block = CBBlockData(A, B, index >= BlockValues.size()); // Just some data, end of message is true if we have no more data.
		ResponseCBMessage.push_back(block);
	}

	SendCBMessage(ResponseCBMessage);
}

void CBOutstationPort::BuildScanRequestResponseData(uint8_t Group, std::vector<uint16_t>& BlockValues)
{
	// We now have to collect all the current values for this group.
	// Search for the group and payload location, and if we have data process it. We have to search 3 lists and the RST table to get what we need
	// This will always return a complete set of blocks, i.e. there will always be a last B value.

	uint8_t MaxBlockNum = 1; // If the following fails, we just respond with an empty single block.

	if (!MyPointConf->PointTable.GetMaxPayload(Group, MaxBlockNum))
	{
		// The DNMS has a habit of scanning groups with no data - why - who knows. So not an error worth reporting in this case.
		Log.Debug("{}- Tried to get the payload count for a group ({}) that has no payload defined - check the configuration", Name, Group);
	}

	// Block 1B will always need a value. 1A is always emtpy - used for group/station/function
	PayloadLocationType payloadlocationB(1, PayloadABType::PositionB);
	BlockValues.push_back(GetPayload(Group, payloadlocationB)); // Returns 0 if payload value cannot be found and logs a message

	for (uint8_t blocknumber = 2; blocknumber <= MaxBlockNum; blocknumber++)
	{
		PayloadLocationType payloadlocationA(blocknumber, PayloadABType::PositionA );
		BlockValues.push_back(GetPayload(Group, payloadlocationA));

		PayloadLocationType payloadlocationB(blocknumber, PayloadABType::PositionB);
		BlockValues.push_back(GetPayload(Group, payloadlocationB));
	}
}

uint16_t CBOutstationPort::GetPayload(uint8_t &Group, PayloadLocationType &payloadlocation)
{
	bool FoundMatch = false;
	uint16_t Payload = 0;

	MyPointConf->PointTable.ForEachMatchingAnalogPoint(Group, payloadlocation, [&Payload,&FoundMatch](CBAnalogCounterPoint &pt)
		{
			// We have a matching point - there may be 2, set a flag to indicate we have a match, and set our bits in the output.
			uint8_t ch = pt.GetChannel();
			if (pt.GetPointType() == ANA6)
			{
				uint16_t value = (63 - pt.GetAnalog()) & 0x03f; // ANA6 Are Inverted, 6 bit result only

				// Shift only if ch == 1 (it is in the top bits!)
				if (ch == 1)
					Payload |= ShiftLeftResult16Bits(value, 6);
				else
					Payload |= value;
			}
			else
				Payload |= pt.GetAnalog();

			FoundMatch = true;
		});

	if (!FoundMatch)
	{
		MyPointConf->PointTable.ForEachMatchingCounterPoint(Group, payloadlocation, [&Payload,&FoundMatch](CBAnalogCounterPoint &pt)
			{
				// We have a matching point - there will be only 1!!, set a flag to indicate we have a match, and set our bit in the output.
				Payload = pt.GetAnalog();
				FoundMatch = true;
			});
	}
	if (!FoundMatch)
	{
		MyPointConf->PointTable.ForEachMatchingBinaryPoint(Group, payloadlocation, [this,&Payload,&FoundMatch](CBBinaryPoint &pt)
			{
				// We have a matching point, set a flag to indicate we have a match, and set our bit in the output.
				uint8_t ch = pt.GetChannel();
				if (pt.GetPointType() == DIG)
				{
					if (pt.GetBinary() == 1)
					{
						if (MyPointConf->IsBakerDevice)
							Payload |= ShiftLeftResult16Bits(1, ch - 1); // ch 12 TO 1,
						else
							Payload |= ShiftLeftResult16Bits(1, 12 - ch); // ch 1 TO 12, Just have to OR, we know it was zero initially.
					}
				}
				else if ((pt.GetPointType() == MCA) || (pt.GetPointType() == MCB) || (pt.GetPointType() == MCC))
				{
					// Up to 6 x 2 bit blocks, channels 1 to 6.
					// MCA - The change bit is set when the input changes from open to closed (1-->0). The status bit is 0 when the contact is CLOSED.
					// MCB - The change bit is set when the input changes from closed to open (0-->1). The status bit is 0 when the contact is OPEN.
					// MCC - The change bit is set when the input has gone through more than one change of state. The status bit is 0 when the contact is OPEN.
					// We dont think about open or closed, we will just be getting a value of 1 or 0 from the real outstation, or a simulator. So we dont do inversion or anything like that.
					// We do need to track the types of transision, and the point has a special field to do this.

					// Set our bit and MC changed flag in the output. Data bit is first (low) then change bit (higher) So bit 11 = changea, bit 10 = data in 12 bit word - 11 highest bit.
					uint8_t result;
					bool MCS;
					pt.GetBinaryAndMCFlagWithFlagReset(result, MCS);

					if (pt.GetPointType() == MCA)
						result = !result; // MCA point on the wire is inverted. Closed == 0

					if (result == 1) // Status
					{
						if (MyPointConf->IsBakerDevice)
							Payload |= ShiftLeftResult16Bits(1, 1 + (ch - 1) * 2); // ch 1 value is Bit 1
						else
							Payload |= ShiftLeftResult16Bits(1, 10 - (ch - 1) * 2); // ch 1 value is bit 10
					}
					if (MCS) // Change Flag
					{
						if (MyPointConf->IsBakerDevice)
							Payload |= ShiftLeftResult16Bits(1, (ch - 1) * 2); // ch 1 status is Bit 0
						else
							Payload |= ShiftLeftResult16Bits(1, 11 - (ch - 1) * 2); // ch 1 status is bit 11
					}
				}
				else
				{
					Log.Error("{} Unhandled Binary Point type {} - no valid value returned",Name, pt.GetPointType());
				}
				FoundMatch = true;
			});
	}
	// See if it is a StatusByte we need to provide - there is only one status byte
	// We could have RST converted to DIG to allow setting some status bits - we clear the important ones below if there is a clash..
	if (MyPointConf->PointTable.IsStatusByteLocation(Group, payloadlocation))
	{
		FoundMatch = true;
		// We cant do this in the above lambda as we might getandset the overflow flag multiple times...
		// Bit Definitions: 0 is MSB!!!!
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
		Payload = SystemFlags.GetStatusPayload(Payload);

		Log.Debug("{} Combining RST bits and Status Word Flags :{} - {} - {}", Name, Group, payloadlocation.to_string(), to_hexstring(Payload));
	}
	if (!FoundMatch)
	{
		Log.Debug("{} Failed to find a payload for: {} Setting to zero",Name, payloadlocation.to_string() );
	}
	return Payload;
}


void CBOutstationPort::FuncTripClose(CBBlockData &Header, PendingCommandType::CommandType pCommand)
{
	// Sets up data to be executed when we get the execute command...
	// So we dont trigger any ODC events at this point, only when the execute occurs.
	// If there is an error - we do not respond!
	std::string cmd = "Trip";
	if (pCommand == PendingCommandType::CommandType::Close) cmd = "Close";

	Log.Debug("{} - {} PendingCommand - Fn2/4, Group {}",Name, cmd, Header.GetGroup());

	uint8_t group = Header.GetGroup();
	PendingCommands[group].Data = Header.GetB();

	// Can/must only be 1 bit set, check this.
	if (GetBitsSet(PendingCommands[group].Data,12) == 1)
	{
		// Check that this is actually a valid CONTROL point.
		uint8_t Channel = 1 + numeric_cast<uint8_t>(GetSetBit(PendingCommands[group].Data, 12));

		if (MyPointConf->IsBakerDevice)
			Channel = 13 - Channel; // 1 to 12, Baker reverse control bit order - only used to check if valid here

		size_t ODCIndex;
		if (!MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(group, Channel, ODCIndex))
		{
			PendingCommands[group].Command = PendingCommandType::CommandType::None;
			Log.Debug("{} FuncTripClose - Could not find an ODC BinaryControl to match Group {}, channel {} - Not responding to Master",Name, Header.GetGroup(), Channel);
			return;
		}

		PendingCommands[group].Command = pCommand;
		PendingCommands[group].ExpiryTime = CBNowUTC() + PendingCommands[group].CommandValidTimemsec;

		Log.Debug("{} Got a valid {} PendingCommand, Data {} DeliberatelyFailControlResponse {}",Name, cmd, PendingCommands[group].Data, FailControlResponse);

		if (FailControlResponse)
		{
			// We have to corrupt the response by shifting right by one bit. Channel is 1 to 12
			uint8_t bit = 1 + numeric_cast<uint8_t>(GetSetBit(PendingCommands[group].Data, 12));
			if (bit > 11) bit = 0;
			PendingCommands[group].Data = 1 << (11 - bit);
		}
		auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), PendingCommands[group].Data, true);

		// Now assemble and return the required response..
		CBMessage_t ResponseCBMessage;
		ResponseCBMessage.push_back(firstblock);

		SendCBMessage(ResponseCBMessage);
	}
	else
	{
		PendingCommands[group].Command = PendingCommandType::CommandType::None;

		// Error - dont reply..
		Log.Error("{} More than one or no bit set in a {} PendingCommand - Not responding to master",Name, cmd);
	}
}

void CBOutstationPort::FuncSetAB(CBBlockData &Header, PendingCommandType::CommandType pCommand)
{
	// Sets up data to be executed when we get the execute command...
	// So we dont trigger any ODC events at this point, only when the execute occurs.
	// If there is an error - we do not respond!
	std::string cmd = "SetA";
	uint8_t Channel = 1;

	if (pCommand == PendingCommandType::CommandType::SetB)
	{
		Channel = 2;
		cmd = "SetB";
	}

	Log.Debug("{} -{} PendingCommand - Fn3 / 5", Name, cmd);
	uint8_t group = Header.GetGroup();

	// Also now check that this is actually a valid CONTROL point.
	size_t ODCIndex;
	if (!MyPointConf->PointTable.GetAnalogControlODCIndexUsingCBIndex(group, Channel, ODCIndex))
	{
		PendingCommands[group].Command = PendingCommandType::CommandType::None;
		Log.Debug("{} FuncSetAB - Could not find an ODC AnalogControl to match Group {}, channel {} - Not responding to Master",Name, group, Channel);
		return;
	}

	PendingCommands[group].Command = pCommand;
	PendingCommands[group].ExpiryTime = CBNowUTC() + PendingCommands[group].CommandValidTimemsec;
	PendingCommands[group].Data = Header.GetB();

	Log.Debug("{} - Got a valid {} PendingCommand, Data {}", Name, cmd, PendingCommands[group].Data);

	auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), PendingCommands[group].Data, true);

	// Now assemble and return the required response..
	CBMessage_t ResponseCBMessage;
	ResponseCBMessage.push_back(firstblock);

	SendCBMessage(ResponseCBMessage);
}

void CBOutstationPort::ExecuteCommand(CBBlockData &Header)
{
	auto pStatusCallback = std::make_shared<std::function<void(CommandStatus status)>>([this,Header](CommandStatus status)
		{
			// Don't respond if we failed.
			if(status == CommandStatus::SUCCESS)
			{
				auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), 0, true);
				CBMessage_t ResponseCBMessage;
				ResponseCBMessage.push_back(firstblock);
				SendCBMessage(ResponseCBMessage);
			}
			else
			{
				Log.Debug("{} We failed ({}) in the execute command, but have no way to send back this error ", Name, ToString(status));
			}
		});

	// Now if there is a command to be executed - do so
	if (MyPointConf->IsBakerDevice && (Header.GetGroup() == 0))
	{
		Log.Debug("{} ExecuteCommand - Fn1 - Doing Baker Global Execute",Name);
		// Baker (well DNMS) seems to use group 0 to execute any Command regardless of the group.
		for (uint8_t i = 0; i < 16; i++)
		{
			ExecuteCommandOnGroup(PendingCommands[i], i, false, pStatusCallback);
			PendingCommands[i].Command = PendingCommandType::CommandType::None;
		}
	}
	else
	{
		Log.Debug("{} ExecuteCommand - Fn1, Group {}", Name, Header.GetGroup());
		ExecuteCommandOnGroup(PendingCommands[Header.GetGroup()], Header.GetGroup(), true, pStatusCallback);
		PendingCommands[Header.GetGroup()].Command = PendingCommandType::CommandType::None;
	}
}

void CBOutstationPort::ExecuteCommandOnGroup(const PendingCommandType& PendingCommand, uint8_t Group, bool singlecommand, SharedStatusCallback_t pStatusCallback)
{
	if ((CBNowUTC() > PendingCommand.ExpiryTime) && (PendingCommand.Command != PendingCommandType::CommandType::None))
	{
		CBTime TimeDelta = CBNowUTC() - PendingCommand.ExpiryTime;
		Log.Debug("{} Received an Execute Command, but the current command had expired - time delta {} msec", Name, TimeDelta);
		(*pStatusCallback)(CommandStatus::TIMEOUT);
		return;
	}

	switch (PendingCommand.Command)
	{
		case PendingCommandType::CommandType::None:
		{
			if (singlecommand)
			{
				Log.Debug("{} Received an Execute Command, but there is no current command",Name);
				(*pStatusCallback)(CommandStatus::NO_SELECT);
			}
			else
				(*pStatusCallback)(CommandStatus::UNDEFINED);
		}
		break;

		case PendingCommandType::CommandType::Trip:
		{
			Log.Debug("{} Received an Execute Command, Trip",Name);
			int Channel = 1 + GetSetBit(PendingCommand.Data, 12);
			if (Channel == 0)
				Log.Error("{} Recevied a Trip command, but no bit was set in the data {}", Name, PendingCommand.Data);

			if (MyPointConf->IsBakerDevice)
				Channel = 13 - Channel; // 1 to 12, Baker reverse control bit order

			bool point_on = false; // Trip is OFF - causes OPEN = 0
			ExecuteBinaryControl(Group, numeric_cast<uint8_t>(Channel), point_on, pStatusCallback);
		}
		break;

		case PendingCommandType::CommandType::Close:
		{
			Log.Debug("{} Received an Execute Command, Close",Name);
			int Channel = 1 + GetSetBit(PendingCommand.Data, 12);
			if (Channel == 0)
				Log.Error("{} Recevied a Close command, but no bit was set in the data {}", Name, PendingCommand.Data);

			if (MyPointConf->IsBakerDevice)
				Channel = 13 - Channel; // 1 to 12, Baker reverse control bit order

			bool point_on = true; // CLOSE is ON, causes CLOSED = 1
			ExecuteBinaryControl(Group, numeric_cast<uint8_t>(Channel), point_on, pStatusCallback);
		}
		break;

		case PendingCommandType::CommandType::SetA:
		{
			Log.Debug("{} Received an Execute Command, SetA",Name);
			ExecuteAnalogControl(Group, 1, PendingCommand.Data, pStatusCallback);
		}
		break;
		case PendingCommandType::CommandType::SetB:
		{
			Log.Debug("{} Received an Execute Command, SetB",Name);
			ExecuteAnalogControl(Group, 2, PendingCommand.Data, pStatusCallback);
		}
		break;
	}
}

// Called when we get a Conitel Execute Command on an already setup trip/close command.
void CBOutstationPort::ExecuteBinaryControl(uint8_t group, uint8_t channel, bool point_on, SharedStatusCallback_t pStatusCallback)
{
	size_t ODCIndex = 0;

	if (!MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(group, channel, ODCIndex))
	{
		Log.Debug("{} Could not find an ODC BinaryControl to match Group {}, channel {}", Name, group, channel);
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	// Set our output value. Only really used for testing
	MyPointConf->PointTable.SetBinaryControlValueUsingODCIndex(ODCIndex, point_on, CBNowUTC());

	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	bool waitforresult = !MyPointConf->StandAloneOutstation;

	Perform(event, waitforresult, pStatusCallback);
}
void CBOutstationPort::ExecuteAnalogControl(uint8_t group, uint8_t channel, uint16_t data, SharedStatusCallback_t pStatusCallback)
{
	// The Setbit+1 == channel
	size_t ODCIndex = 0;

	if (!MyPointConf->PointTable.GetAnalogControlODCIndexUsingCBIndex(group, channel, ODCIndex))
	{
		Log.Debug("{} Could not find an ODC AnalogControl to match Group {}, channel {}", Name, group, channel);
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	// Set our output value. Only really used for testing
	MyPointConf->PointTable.SetAnalogControlValueUsingODCIndex(ODCIndex, data, CBNowUTC());

	EventTypePayload<EventType::AnalogOutputInt16>::type val;
	val.first = numeric_cast<short>(data);

	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ODCIndex, Name);
	event->SetPayload<EventType::AnalogOutputInt16>(std::move(val));

	bool waitforresult = !MyPointConf->StandAloneOutstation;

	Perform(event, waitforresult, pStatusCallback);
}
void CBOutstationPort::FuncMasterStationRequest(CBBlockData & Header, CBMessage_t & CompleteCBMessage)
{
	Log.Debug("{} - MasterStationRequest - Fn9 - Code {}", Name, Header.GetGroup());
	// The purpose of this function is determined by Group Number
	switch (Header.GetGroup())
	{
		case MASTER_SUB_FUNC_0_NOTUSED:
		case MASTER_SUB_FUNC_SPARE1:
		case MASTER_SUB_FUNC_SPARE2:
		case MASTER_SUB_FUNC_SPARE3:
			// Not used, we do not respond;
			Log.Error("{} Received Unused Master Command Function - {} ",Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_TESTRAM:
		case MASTER_SUB_FUNC_TESTPROM:
		case MASTER_SUB_FUNC_TESTEPROM:
		case MASTER_SUB_FUNC_TESTIO:

			// We respond as if everything is OK - we just send back what we got. One block only.
			EchoReceivedHeaderToMaster(Header);
			Log.Debug("{} Received TEST Master Command Function - {}, no action, but we reply", Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_SEND_TIME_UPDATES:
		{
			// This is the only code that has extra Master to RTU blocks
			uint8_t DataIndex = Header.GetB() >> 8;
			uint8_t NumberOfBlocksInMessage = (Header.GetB() >> 4) & 0x00f;
			//If we needed the Fn9 Data
			//uint8_t Fn9Data = (Header.GetB() & 0x00f);
			if ((DataIndex == 0) && (NumberOfBlocksInMessage == 2) && (CompleteCBMessage.size() == 2))
			{
				ProcessUpdateTimeRequest(CompleteCBMessage);
			}
			else
			{
				Log.Error("{} Received Illegal MASTER_SUB_FUNC_SEND_TIME_UPDATES Command - Index {} (must be 0), Blocks {} (must be 2), Message Size {} (must be 2) ", Name, DataIndex, NumberOfBlocksInMessage, CompleteCBMessage.size());
			}
		}
		break;

		case MASTER_SUB_FUNC_RETRIEVE_REMOTE_STATUS_WORD:
			RemoteStatusWordResponse(Header);
			break;

		case MASTER_SUB_FUNC_RETREIVE_INPUT_CIRCUIT_DATA:
			Log.Error("{} Received Input Circuit Data Master Command Function - {}, not implemented", Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_TIME_CORRECTION_FACTOR_ESTABLISHMENT: // Also send Comms Stats, 2nd option
			EchoReceivedHeaderToMaster(Header);
			Log.Debug("{} Received Time Correction Factor Master Command Function - {}, no action, but we reply", Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION:
			Log.Debug("{} Resending Last Message as a Repeat Last Transmission - {}", CBMessageAsString(LastSentCBMessage));
			ResendLastCBMessage();
			break;

		case MASTER_SUB_FUNC_SET_LOOPBACKS:
			Log.Error("{} Received Input Circuit Data Master Command Function - {}, not implemented", Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_RESET_RTU_WARM:
			Log.Error("{} Received Warm Restart Master Command Function - {}, not implemented", Name, Header.GetGroup());
			break;

		case MASTER_SUB_FUNC_RESET_RTU_COLD:
			Log.Error("{} Received Cold Restart Master Command Function - {}, not implemented", Name, Header.GetGroup());
			break;

		default:
			Log.Error("{} Unknown PendingCommand Function - {} On Station Address - {}", Name, Header.GetFunctionCode(),Header.GetStationAddress());
			break;
	}
}

void CBOutstationPort::RemoteStatusWordResponse(CBBlockData& Header)
{
	// We only implement 4, 5, 10, 11 as hard coded points. The rest can be defined as RST points which will be treated like digitals (so you can set their values in the Simulator)
	uint16_t Payload = 0;

	// TODO: Need to scan any digitals in this Group/Location..

	Payload = SystemFlags.GetStatusPayload(Payload);
	auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), Payload, true);
	CBMessage_t ResponseCBMessage;
	ResponseCBMessage.push_back(firstblock);
	SendCBMessage(ResponseCBMessage);

	Log.Debug("{} Received Get Remote Status Master Command Function - {}, replied with Status Bits {}", Name, Header.GetGroup(), to_hexstring(Payload));
}

void CBOutstationPort::FuncReSendSOEResponse(CBBlockData & Header)
{
	Log.Debug("{} - ReSendSOEResponse - FnB - Code {}",Name, Header.GetGroup());

	SendCBMessage(LastSentSOEMessage);
}

void CBOutstationPort::FuncSendSOEResponse(CBBlockData & Header)
{
	Log.Debug("{} - SendSOEResponse - FnA - Code {}", Name, Header.GetGroup());

	CBMessage_t ResponseCBMessage;

	if (!MyPointConf->PointTable.TimeTaggedDataAvailable())
	{
		// Format empty response
		// Not clear in the spec what an empty SOE response is, however I am going to assume that a full echo of the inbound packet is the empty response.
		// The Master can work out that there should be another block for a real response, also Group 0, Point 0 is unlikely to be used?
		ResponseCBMessage.push_back(Header);
	}
	else
	{
		// The maximum number of bits we can send is 12 * 31 = 372.
		uint32_t UsedBits = 0;
		std::array<bool, MaxSOEBits> BitArray{};

		BuildPackedEventBitArray(BitArray, UsedBits);

		// Using an vector of uint16_t to store up to 31 x 12 bit blocks of data.
		// Store the data in the bottom 12 bits of the 16 bit word.
		std::vector<uint16_t> PayloadWords;
		PayloadWords.reserve(32); // To stop reallocations when we know max size.

		ConvertBitArrayToPayloadWords(UsedBits, BitArray, PayloadWords);

		// We now have the payloads ready to load into Conitel packets.
		ConvertPayloadWordsToCBMessage(Header, PayloadWords, ResponseCBMessage);
	}
	if (ResponseCBMessage.size() > 16)
		Log.Error("{} Too many packets in ResponseCBMessage in Outstation SOE Response - fatal error",Name);

	//Log.Debug("Station F10 Response Packet {}", BuildASCIIHexStringfromCBMessage(ResponseCBMessage));

	LastSentSOEMessage = ResponseCBMessage;
	SendCBMessage(ResponseCBMessage);
}
void CBOutstationPort::ConvertPayloadWordsToCBMessage(CBBlockData & Header, std::vector<uint16_t> &PayloadWords, CBMessage_t &ResponseCBMessage)
{
	// We must have at least one payload! If, not it would be zero, which is what we would send back if there was none, so we get there either way.
	uint16_t BPayload = 0;
	if (PayloadWords.size()!= 0) // Might need a zero filled padding block...
		BPayload = PayloadWords[0];

	CBBlockData ReturnHeader(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), BPayload);

	ResponseCBMessage.push_back(ReturnHeader);

	uint8_t block = 1;
	while (block < PayloadWords.size())
	{
		uint16_t APayload = PayloadWords[block++];
		uint16_t BPayload = 0;

		if (block < PayloadWords.size()) // Only Set B Block value if it exists
			BPayload = PayloadWords[block++];

		CBBlockData Block(APayload, BPayload);

		ResponseCBMessage.push_back(Block);
	}
	ResponseCBMessage.back().MarkAsEndOfMessageBlock();
}
void CBOutstationPort::ConvertBitArrayToPayloadWords(const uint32_t UsedBits, std::array<bool, MaxSOEBits> &BitArray, std::vector<uint16_t> &PayloadWords)
{
	uint8_t BlockCount = UsedBits / 12 + ((UsedBits % 12 == 0) ? 0 : 1);
	if (BlockCount > 31)
	{
		Log.Error("{} ConvertBitArrayToPayloadWords - blockcount exceeded 31!! {}",Name, BlockCount);
		BlockCount = 31;
	}
	for (uint8_t block = 0; block < BlockCount; block++)
	{
		uint16_t payload = 0;
		for (size_t i = 0; i < 12; i++)
		{
			size_t bitindex = (size_t)block * 12 + i;
			if (bitindex >= MaxSOEBits)
			{
				Log.Error("{} ConvertBitArrayToPayloadWords - bit index exceeded {}!! {}", Name, MaxSOEBits, bitindex);
				break; // Dont do any more bits!
			}
			payload |= ShiftLeftResult16Bits(BitArray[bitindex] ? 1 : 0, 11 - i);
		}
		PayloadWords.push_back(payload);
	}
}
void CBOutstationPort::BuildPackedEventBitArray(std::array<bool, MaxSOEBits> &BitArray, uint32_t &UsedBits)
{
	// The SOE data is built into a stream of bits (that may not be block aligned) and then it is stuffed 12 bits at a time into the available Payload locations - up to 31.
	// First section format:
	// 1 - 3 bits group #, 7 bits point number, Status(value) bit (Note MCA Status is inverted??), Quality Bit (unused), Time Bit - changes what comes next
	// T==1 Time - 27 bits, Hour (0-23, 5 bits), Minute (0-59, 6 bits), Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// T==0 Hours and Minutes same as previous event, 16 bits - Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// Last bit L, when set indicates last record.
	// So the data can be 13+27+1 bits = 41 bits, or 13+16+1 = 30 bits.

	CBBinaryPoint CurrentPoint;
	CBTime LastPointTime = 0;

	while (true)
	{
		// There must be at least one SOE available to get to here.

		if (!MyPointConf->PointTable.PeekNextTaggedEventPoint(CurrentPoint)) // Don't pop until we are happy...
		{
			// We have run out of data, so break out of the loop, but first we need to set the last event flag in the previous packet--this is the last bit in the vector
			// SET LAST EVENT FLAG
			if (UsedBits <= MaxSOEBits)
				BitArray[UsedBits - 1] = true; // This index is safe as to get to here there must have been at least one SOE processed.
			else
				Log.Error("{} BuildPackedEventBitArray UsedBits Exceeded MaxSOEBits",Name);
			break;
		}

		// We keep trying to add data until there is none left, or the data will not fit into our bit array.
		// The time field hours/minutes/seconds/msec since epoch (adjusted for the time set command)
		SOEEventFormat PackedEvent;
		PackedEvent.Group = CurrentPoint.GetGroup() & 0x07; // Bottom three bits of the point group,  not the SOE Group!
		PackedEvent.Number = CurrentPoint.GetSOEIndex();
		PackedEvent.ValueBit = CurrentPoint.GetBinary() ? true : false;
		if (CurrentPoint.GetPointType() == MCA)
			PackedEvent.ValueBit = !PackedEvent.ValueBit; // MCA point on the wire is inverted. Closed == 0

		PackedEvent.QualityBit = false;

		CBTime ChangedCorrectedTime = CurrentPoint.GetChangedTime() + (int64_t)((int64_t)SOETimeOffsetMinutes * 60 * 1000) + (int64_t)(SystemClockOffsetMilliSeconds);

		bool FirstEvent = (LastPointTime == 0);

		bool SendHoursAndMinutes = HoursMinutesHaveChanged(LastPointTime, ChangedCorrectedTime) || FirstEvent;

		LastPointTime = ChangedCorrectedTime;

		// The time we send gets reduced to H:M:S.msec
		PackedEvent.SetTimeFields(ChangedCorrectedTime, SendHoursAndMinutes);

		PackedEvent.LastEventFlag = false; // Might be changed on the loop exit, if there is nothing left in the SOE queue.

		// Now stuff the bits (41 or 30) into our bit array, at the end of the current data.
		if (PackedEvent.AddDataToBitArray(BitArray,UsedBits))
		{
			// Pop the event so we can move onto the next one
			MyPointConf->PointTable.PopNextTaggedEventPoint();
		}
		else
		{
			// We have run out of space, break out. Dont change the LastEventFlag bit in the last packet, as there are still events in the SOE queue.
			break; // Exit the while loop.
		}
	}

	// We must zero any remaining bits in the array. The payload packing later makes an assumption about this!
	for (uint32_t i = UsedBits; i < MaxSOEBits; i++)
		BitArray[i] = false;
}
// We use this to calculate an offset, which is then used when packaging up the SOE time stamps (added or subtracted)
// just echo message - normally it is UTC time of day in milliseconds since 1970 (CBTime()). Could be any time zone in practice.
//	TODO: generate odc::EventType::TimeSync event for downstream ports to also correct their clocks
//		don't forget there's a separate ms offset 'SystemClockOffsetMilliSeconds'
void CBOutstationPort::ProcessUpdateTimeRequest(CBMessage_t& CompleteCBMessage)
{
	CBMessage_t ResponseCBMessage;

	uint8_t hh, hhin;
	uint8_t mm, mmin;
	uint8_t ss, ssin;
	uint16_t msec, msecin;

	// Get the machine UTC time.
	to_hhmmssmmfromCBtime(CBNowUTC(), hh, mm, ss, msec);

	DecodeTimePayload(CompleteCBMessage[0].GetB(), CompleteCBMessage[1].GetA(), CompleteCBMessage[1].GetB(),hhin, mmin, ssin, msecin);
	// Work out the offset (in minutes) from the sent time. We assume the Master and ODC will have NTP time sync, so really about adjusting for time zone only.
	// Value can be +/-
	int16_t SetMinutes = hhin * 60 + mmin + (ssin > 30 ? 1 : 0);
	int16_t ClockMinutes = hh * 60 + mm + (ss > 30 ? 1 : 0);

	SOETimeOffsetMinutes = SetMinutes - ClockMinutes; // So when adjusting SOE times, just add the Offset to the Clock

	// If we are doing this around a UTC clock rollover, it can fail we can get 1430 instead of -10.
	// We dont care about the actual day, so 1430 is actually equal to -10.
	// So we will limit the range to +/- 1440/2 (i.e. +/- half a day)
	if (SOETimeOffsetMinutes > (1440 / 2))
		SOETimeOffsetMinutes = SOETimeOffsetMinutes - 1440;

	if (SOETimeOffsetMinutes < -(1440 / 2))
		SOETimeOffsetMinutes = SOETimeOffsetMinutes + 1440;

	Log.Debug("{} Received Time Set Command {}, Current UTC Time {} - SOE Offset Now Set to {} minutes",Name, to_stringfromhhmmssmsec(hhin, mmin, ssin, msecin), to_stringfromCBtime(CBNowUTC()), SOETimeOffsetMinutes);

	//(uint16_t P1B, uint16_t P2A, uint16_t P2B, uint8_t & hhin, uint8_t & mmin, uint8_t & ssin, uint16_t & msecin)

	// Have to adjust hh,mm,ss,msec to match adjusted time..in msec
	int64_t SetMilliSecs = (((uint64_t)hhin * 60 + (uint64_t)mmin) * 60 + (uint64_t)ssin) * 1000 + msecin + SystemClockOffsetMilliSeconds;

	// Now turn back into hh,mm,ss,msec - does not matter that our msec does not contain years/days.
	to_hhmmssmmfromCBtime(SetMilliSecs, hh, mm, ss, msec);
	uint16_t P1B = 0;
	uint16_t P2A = 0;
	uint16_t P2B = 0;
	PackageTimePayload(hh, mm, ss, msec, P1B, P2A, P2B);

	// We just echo back what we were sent. This is what is expected.
	CompleteCBMessage[0].SetB(P1B);
	CompleteCBMessage[1].SetA(P2A);
	CompleteCBMessage[1].SetB(P2B);
	ResponseCBMessage.push_back(CompleteCBMessage[0]);
	ResponseCBMessage.push_back(CompleteCBMessage[1]);

	SendCBMessage(ResponseCBMessage);
}

void CBOutstationPort::EchoReceivedHeaderToMaster(CBBlockData & Header)
{
	auto firstblock = CBBlockData(Header.GetStationAddress(), Header.GetGroup(), Header.GetFunctionCode(), 0, true);
	CBMessage_t ResponseCBMessage;
	ResponseCBMessage.push_back(firstblock);
	SendCBMessage(ResponseCBMessage);
}
#ifdef _MSC_VER
#pragma endregion

#pragma region Worker Methods
#endif
// This method is passed to the SystemFlags variable to do the necessary calculation
// Access through SystemFlags.GetDigitalChangedFlag()
bool CBOutstationPort::SOEAvailableFn(void)
{
	return MyPointConf->PointTable.TimeTaggedDataAvailable();
	// Was this - not correct	return (CountBinaryBlocksWithChanges() > 0);
}
bool CBOutstationPort::SOEOverflowFn(void)
{
	return SOEBufferOverflowFlag->getandclear();
}
void CBOutstationPort::MarkAllBinaryPointsAsChanged()
{
	MyPointConf->PointTable.ForEachBinaryPoint([](CBBinaryPoint &pt)
		{
			pt.SetChangedFlag();
		});
}

// Scan all binary/digital blocks for changes - used to determine what response we need to send
// We return the total number of changed blocks we assume every block supports time tagging
// If SendEverything is true,
uint8_t CBOutstationPort::CountBinaryBlocksWithChanges()
{
	uint8_t changedblocks = 0;
	int lastblock = -1; // Non valid value

	// The map is sorted, so when iterating, we are working to a specific order. We can have up to 16 points in a block only one changing will trigger a send.
	MyPointConf->PointTable.ForEachBinaryPoint([&lastblock,&changedblocks](CBBinaryPoint &pt)
		{
			if (pt.GetChangedFlag())
			{
				// Multiple bits can be changed in the block, but only the first one is required to trigger a send of the block.
				if (lastblock != pt.GetGroup())
				{
					lastblock = pt.GetGroup();
					changedblocks++;
				}
			}
		});

	return changedblocks;
}
// UI Handlers
std::pair<std::string, const IUIResponder *> CBOutstationPort::GetUIResponder()
{
	return std::pair<std::string, const CBOutstationPortCollection*>("CBOutstationPortControl", this->CBOutstationCollection.get());
}

bool CBOutstationPort::UIFailControl(const std::string &active)
{
	if (iequals(active,"true"))
	{
		FailControlResponse = true;
		Log.Critical("{} The Control Response Packet is set to fail - {}", Name, FailControlResponse);
		return true;
	}
	if (iequals(active, "false"))
	{
		FailControlResponse = false;
		Log.Critical("{} The Control Response Packet is set to fail - {}", Name, FailControlResponse);
		return true;
	}
	return false;
}
bool CBOutstationPort::UISetRTUReStartFlag(const std::string& active)
{
	// Set the Watchdog Timer bit in the RSW
	if (iequals(active, "true"))
	{
		SystemFlags.SetStartupFlag(true);
		Log.Critical("{} The RTU Reset Flag has been set", Name);
		return true;
	}
	if (iequals(active, "false"))
	{
		SystemFlags.SetStartupFlag(false);
		Log.Critical("{} The RTU Reset Flag has been cleared", Name);
		return true;
	}
	return false;
}
bool CBOutstationPort::UISetRTUControlIsolateFlag(const std::string& active)
{
	// Set the Control Isolate bit in the RSW
	if (iequals(active, "true"))
	{
		SystemFlags.SetControlIsolateFlag(true);
		Log.Critical("{} The RTU Control Isolate Flag has been set", Name);
		return true;
	}
	if (iequals(active, "false"))
	{
		SystemFlags.SetControlIsolateFlag(false);
		Log.Critical("{} The RTU Control Isolate Flag has been cleared", Name);
		return true;
	}
	return false;
}
bool CBOutstationPort::UIRandomReponseBitFlips(const std::string& probability)
{
	try
	{
		auto prob = std::stod(probability);
		if ((prob > 1.0) || (prob < 0.0))
			return false;
		BitFlipProbability = prob;
		Log.Critical("{} Set the probability of a flipped bit in the response packet to {}", Name, BitFlipProbability );
		return true;
	}
	catch (...)
	{}
	return false;
}
bool CBOutstationPort::UIRandomReponseDrops(const std::string& probability)
{
	try
	{
		auto prob = std::stod(probability);
		if ((prob > 1.0) || (prob < 0.0))
			return false;
		ResponseDropProbability = prob;
		Log.Critical("{} Set the probability of a dropped response packet to {}", Name, ResponseDropProbability);
		return true;
	}
	catch (...)
	{}
	return false;
}
bool CBOutstationPort::UIAdjustTimeOffsetMilliSeconds(const std::string& millisecondoffset)
{
	//TODO: generate odc::EventType::TimeSync event for downstream ports to also correct their clocks
	//	don't forget there's a separate minute offset 'SOETimeOffsetMinutes'
	try
	{
		SystemClockOffsetMilliSeconds = std::stoi(millisecondoffset);
		Log.Critical("{} Set the millisecond clock offset to {}", Name, SystemClockOffsetMilliSeconds);
		return true;
	}
	catch (...)
	{}
	return false;
}

#ifdef _MSC_VER
#pragma endregion
#endif
