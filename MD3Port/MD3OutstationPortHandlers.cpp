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
  * MD3OutstationPort.cpp
  *
  *  Created on: 16/10/2014
  *      Author: Alan Murray
  */

  /* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
   This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/MD3) will turn back into scada commands and send out to the "real" Outstation.
   So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
   So an Event to an outstation will be data that needs to be sent up to the scada master.
   An event from an outstation will be a master control signal to turn something on or off.
  */
#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "MD3Engine.h"
#include "MD3OutstationPort.h"
#include "LogMacro.h"
#include "MD3.h"
#include "CRC.h"

#include <opendnp3/LogLevels.h>

//TODO: Check out http://www.pantheios.org/ logging library..



// The list of codes used by the AusGrid network - Sydney
/*
ANALOG_UNCONDITIONAL = 5,	// HAS MODULE INFORMATION ATTACHED
ANALOG_DELTA_SCAN = 6,		// HAS MODULE INFORMATION ATTACHED
DIGITAL_UNCONDITIONAL_OBS = 7,	// OBSOLETE // HAS MODULE INFORMATION ATTACHED
DIGITAL_DELTA_SCAN = 8,		// OBSOLETE // HAS MODULE INFORMATION ATTACHED
HRER_LIST_SCAN = 9,			// OBSOLETE
DIGITAL_CHANGE_OF_STATE = 10,// OBSOLETE // HAS MODULE INFORMATION ATTACHED
DIGITAL_CHANGE_OF_STATE_TIME_TAGGED = 11,
DIGITAL_UNCONDITIONAL = 12,
ANALOG_NO_CHANGE_REPLY = 13,	// HAS MODULE INFORMATION ATTACHED
DIGITAL_NO_CHANGE_REPLY = 14,	// HAS MODULE INFORMATION ATTACHED
CONTROL_REQUEST_OK = 15,
FREEZE_AND_RESET = 16,
POM_TYPE_CONTROL = 17,
DOM_TYPE_CONTROL = 19,	// NOT USED
INPUT_POINT_CONTROL = 20,
RAISE_LOWER_TYPE_CONTROL = 21,
AOM_TYPE_CONTROL = 23,
CONTROL_OR_SCAN_REQUEST_REJECTED = 30,	// HAS MODULE INFORMATION ATTACHED
COUNTER_SCAN = 31,						// HAS MODULE INFORMATION ATTACHED
SYSTEM_SIGNON_CONTROL = 40,
SYSTEM_SIGNOFF_CONTROL = 41,
SYSTEM_RESTART_CONTROL = 42,
SYSTEM_SET_DATETIME_CONTROL = 43,
FILE_DOWNLOAD = 50,
FILE_UPLOAD = 51,
SYSTEM_FLAG_SCAN = 52,
LOW_RES_EVENTS_LIST_SCAN = 60

*/

void MD3OutstationPort::ProcessMD3Message(std::vector<MD3Block> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	uint8_t ExpectedStationAddress = MyConf()->mAddrConf.OutstationAddr;

	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.
	// All are included to allow better error reporting.
	switch (CompleteMD3Message[0].GetFunctionCode())
	{
	case ANALOG_UNCONDITIONAL:	// Command and reply
		DoAnalogUnconditional(CompleteMD3Message);
		break;
	case ANALOG_DELTA_SCAN:		// Command and reply
		DoAnalogDeltaScan(CompleteMD3Message);
		break;
	case DIGITAL_UNCONDITIONAL_OBS:
		DoDigitalUnconditionalObs(CompleteMD3Message);
		break;
	case DIGITAL_DELTA_SCAN:
		DoDigitalChangeOnly(CompleteMD3Message);
		break;
	case HRER_LIST_SCAN:
		break;
	case DIGITAL_CHANGE_OF_STATE:
		break;
	case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
		break;
	case DIGITAL_UNCONDITIONAL:
		break;
	case ANALOG_NO_CHANGE_REPLY:
		// Master Only
		break;
	case DIGITAL_NO_CHANGE_REPLY:
		// Master Only
		break;
	case CONTROL_REQUEST_OK:
		break;
	case FREEZE_AND_RESET:
		break;
	case POM_TYPE_CONTROL :
		break;
	case DOM_TYPE_CONTROL: // NOT USED BY AUSGRID
		break;
	case INPUT_POINT_CONTROL:
		break;
	case RAISE_LOWER_TYPE_CONTROL :
		break;
	case AOM_TYPE_CONTROL :
		break;
	case CONTROL_OR_SCAN_REQUEST_REJECTED:
		break;
	case COUNTER_SCAN :
		break;
	case SYSTEM_SIGNON_CONTROL:
		break;
	case SYSTEM_SIGNOFF_CONTROL:
		break;
	case SYSTEM_RESTART_CONTROL:
		break;
	case SYSTEM_SET_DATETIME_CONTROL:
		break;
	case FILE_DOWNLOAD:
		break;
	case FILE_UPLOAD :
		break;
	case SYSTEM_FLAG_SCAN:
		break;
	case LOW_RES_EVENTS_LIST_SCAN :
		break;
	default:
		LOG("DNP3OutstationPort", openpal::logflags::ERR, "", "Unknown Command Function - " + std::to_string(CompleteMD3Message[0].GetFunctionCode()) + " On Station Address - " + std::to_string(CompleteMD3Message[0].GetStationAddress()));
		break;
	}
}
#pragma region ANALOG
// Function 5
void MD3OutstationPort::DoAnalogUnconditional(std::vector<MD3Block> &CompleteMD3Message)
{
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	ReadAnalogRange(CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
	SendAnalogUnconditional(AnalogValues, CompleteMD3Message[0].GetStationAddress(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
}

// Function 6
void MD3OutstationPort::DoAnalogDeltaScan(std::vector<MD3Block> &CompleteMD3Message)
{
	// First work out what our response will be, loading the data to be sent into two vectors.
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	ReadAnalogRange(CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now we know what type of response we are going to send.
	if (ResponseType == AllChange)
	{
		SendAnalogUnconditional(AnalogValues, CompleteMD3Message[0].GetStationAddress(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	}
	else if (ResponseType == DeltaChange)
	{
		SendAnalogDelta(AnalogDeltaValues, CompleteMD3Message[0].GetStationAddress(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	}
	else
	{
		SendAnalogNoChange(CompleteMD3Message[0].GetStationAddress(), CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	}
}

void MD3OutstationPort::ReadAnalogRange(int ModuleAddress, int Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues)
{
	for (int i = 0; i < Channels; i++)
	{
		uint16_t wordres = 0;
		int deltares = 0;
		if (!GetAnalogValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares))
		{
			// Point does not exist - need to send analog unconditional as response.
			ResponseType = AllChange;
			AnalogValues.push_back(0x8000);			// Magic value
			AnalogDeltaValues.push_back(0);
		}
		else
		{
			AnalogValues.push_back(wordres);
			AnalogDeltaValues.push_back(deltares);

			if (abs(deltares) > 127)
			{
				ResponseType = AllChange;
			}
			else if (abs(deltares > 0) && (ResponseType != AllChange))
			{
				ResponseType = DeltaChange;
			}
		}
	}
}
void MD3OutstationPort::SendAnalogUnconditional(std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(StationAddress, false, ANALOG_UNCONDITIONAL, ModuleAddress, Channels);
	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Analogs.size());

	int NumberOfDataBlocks = Channels / 2 + Channels % 2;	// 2 --> 1, 3 -->2

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = MD3Block(Analogs[2 * i], Analogs[2 * i + 1], lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendResponse(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(StationAddress, false, ANALOG_DELTA_SCAN, ModuleAddress, Channels);
	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Deltas.size());

	// Can be 4 channel delta values to a block.
	int NumberOfDataBlocks = Channels / 4 + (Channels % 4 == 0 ? 0 : 1);

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = MD3Block((char)Deltas[i * 4], (char)Deltas[i * 4 + 1], (char)Deltas[i * 4 + 2], (char)Deltas[i * 4 + 3], lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendResponse(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(StationAddress, false, ANALOG_NO_CHANGE_REPLY, ModuleAddress, Channels, true);
	ResponseMD3Message.push_back(FormattedBlock);
	SendResponse(ResponseMD3Message);
}

#pragma endregion

#pragma region DIGITAL
// Function 7
void MD3OutstationPort::DoDigitalUnconditionalObs(std::vector<MD3Block> &CompleteMD3Message)
{
	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	std::vector<MD3Block> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, DIGITAL_UNCONDITIONAL_OBS, CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	int NumberOfDataBlocks = CompleteMD3Message[0].GetChannels(); // Actually the number of modules - 0 numbered, does not make sense to ask for none...

	BuildDigitalReturnBlocks(NumberOfDataBlocks, CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetStationAddress(), true, ResponseMD3Message);
	SendResponse(ResponseMD3Message);
}
// Function 8
void MD3OutstationPort::DoDigitalChangeOnly(std::vector<MD3Block> &CompleteMD3Message)
{
	// Have three possible replies, Digital Unconditional #7,  Delta Scan #8 and Digital No Change #14
	// So the data is deemed to have changed if a bit has changed, or the status has changed.
	// We detect changes on a module basis, and send the data in the same format as the Digital Unconditional.
	// So the Delta Scan can be all the same data as the uncondtional.
	// If no changes, send the single #14 function block. Keep the module and channel values matching the orginal message.

	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	std::vector<MD3Block> ResponseMD3Message;

	bool NoChange = true;
	bool SomeChange = false;
	int NumberOfDataBlocks = CompleteMD3Message[0].GetChannels(); // Actually the number of modules - 0 numbered, does not make sense to ask for none...

	int ChangedBlocks = CheckDigitalChangeBlocks(NumberOfDataBlocks, CompleteMD3Message[0].GetModuleAddress());

	if (ChangedBlocks == 0)	// No change
	{
		MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, DIGITAL_NO_CHANGE_REPLY, CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetChannels(), true);
		ResponseMD3Message.push_back(FormattedBlock);

		SendResponse(ResponseMD3Message);
	}
	else if (ChangedBlocks != NumberOfDataBlocks)	// Some change
	{
		//TODO: What are the module and channel set to in Function 8 digital change response packets - packet count can vary..
		MD3Block FormattedBlock = MD3Block(CompleteMD3Message[0].GetStationAddress(), false, DIGITAL_DELTA_SCAN, CompleteMD3Message[0].GetModuleAddress(), ChangedBlocks);
		ResponseMD3Message.push_back(FormattedBlock);

		BuildDigitalReturnBlocks(NumberOfDataBlocks, CompleteMD3Message[0].GetModuleAddress(), CompleteMD3Message[0].GetStationAddress(), false, ResponseMD3Message);

		SendResponse(ResponseMD3Message);
	}
	else
	{
		// All changed..do Digital Unconditional
		DoDigitalUnconditionalObs(CompleteMD3Message);
	}
}

int MD3OutstationPort::CheckDigitalChangeBlocks(int NumberOfDataBlocks, int StartModuleAddress)
{
	int changedblocks = 0;

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		// Have to collect all the bits into a uint16_t
		uint16_t wordres = 0;
		bool missingdata = false;
		bool datachanged = false;

		for (int j = 0; j < 16; j++)
		{
			uint8_t bitres = 0;
			bool changed = false;

			if (!GetBinaryChangedUsingMD3Index(StartModuleAddress + i, j, changed))	// Does not change the changed bit
			{
				// Data is missing, need to send the error block for this module address.
				missingdata = true;
				changed = true;
			}
			if (changed)
				datachanged = true;
		}
		if (datachanged)
			changedblocks++;
	}
	return changedblocks;
}
void MD3OutstationPort::BuildDigitalReturnBlocks(int NumberOfDataBlocks, int StartModuleAddress, int StationAddress, bool forcesend, std::vector<MD3Block> &ResponseMD3Message)
{
	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		// Have to collect all the bits into a uint16_t
		uint16_t wordres = 0;
		bool missingdata = false;
		bool datachanged = false;

		for (int j = 0; j < 16; j++)
		{
			uint8_t bitres = 0;
			bool changed = false;

			if (!GetBinaryValueUsingMD3Index(StartModuleAddress + i, j, bitres, changed))	//TODO: Reading this clears the changed bit, does this need to be smarter???
			{
				// Data is missing, need to send the error block for this module address.
				missingdata = true;
				changed = true;
			}
			else
			{
				// TODO: Check the bit order here of the binaries
				wordres |= (uint16_t)bitres << (15 - j);
			}
			if (changed)
				datachanged = true;
		}
		if (datachanged || forcesend)
		{
			if (missingdata)
			{
				// Queue the error block
				uint8_t errorflags = 0;		//TODO: Application dependent, depends on the outstation implementation/master expectations. We could build in functionality here
				uint16_t lowword = (uint16_t)errorflags << 8 | (StartModuleAddress + i);
				auto block = MD3Block((uint16_t)StationAddress << 8, lowword, false);	// Read the MD3 Data Structure - Module and Channel - dummy at the moment...
				ResponseMD3Message.push_back(block);
			}
			else
			{
				// Queue the data block
				uint16_t address = (uint16_t)StationAddress << 8 | (StartModuleAddress + i);
				auto block = MD3Block(address, wordres, false);
				ResponseMD3Message.push_back(block);
			}
		}
	}
	// Not sure which is the last block for the send only changes..so just mark it when we get to here.
	MD3Block &lastblock = ResponseMD3Message.back();
	lastblock.MarkAsEndOfMessageBlock();
}

#pragma endregion
