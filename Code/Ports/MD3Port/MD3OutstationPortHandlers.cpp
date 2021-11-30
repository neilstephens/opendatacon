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
* MD3OutStationPortHandlers.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

/* The out station port is connected to the Overall System Scada master, so the master thinks it is talking to an outstation.
 This code then fires off events to the connector, which the connected master port(s) (of some type DNP3/ModBus/MD3) will turn back into scada commands and send out to the "real" Outstation.
 So it makes sense to connect the SIM (which generates data) to a DNP3 Outstation which will feed the data back to the SCADA master.
 So an Event to an outstation will be data that needs to be sent up to the scada master.
 An event from an outstation will be a master control signal to turn something on or off.
*/
#include "MD3.h"
#include "MD3OutstationPort.h"
#include "MD3OutStationPortCollection.h"
#include "MD3Utility.h"
#include <chrono>
#include <future>
#include <iostream>
#include <regex>


// The list of codes in use
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
DOM_TYPE_CONTROL = 19,
INPUT_POINT_CONTROL = 20,
RAISE_LOWER_TYPE_CONTROL = 21,
AOM_TYPE_CONTROL = 23,
CONTROL_OR_SCAN_REQUEST_REJECTED = 30,	// HAS MODULE INFORMATION ATTACHED
COUNTER_SCAN = 31,						// HAS MODULE INFORMATION ATTACHED
SYSTEM_SIGNON_CONTROL = 40,
SYSTEM_SIGNOFF_CONTROL = 41,
SYSTEM_RESTART_CONTROL = 42,
SYSTEM_SET_DATETIME_CONTROL = 43,
SYSTEM_SET_DATETIME_CONTROL_NEW = 44,
FILE_DOWNLOAD = 50,
FILE_UPLOAD = 51,
SYSTEM_FLAG_SCAN = 52,
LOW_RES_EVENTS_LIST_SCAN = 60

*/


//Synchronise processing the messages with the internal events
void MD3OutstationPort::ProcessMD3Message(MD3Message_t&& CompleteMD3Message)
{
	EventSyncExecutor->post([this,msg{std::move(CompleteMD3Message)}]() mutable
		{
			ProcessMD3Message_(std::move(msg));
		});
}

//This should only be called by the synchronisation wrapper above
void MD3OutstationPort::ProcessMD3Message_(MD3Message_t&& CompleteMD3Message)
{
	if (!enabled.load()) return; // Port Disabled so dont process

	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	size_t ExpectedMessageSize = 1; // Only set in switch statement if not 1

	MD3BlockFormatted Header = MD3BlockFormatted(CompleteMD3Message[0]);
	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.

	if (Header.IsMasterToStationMessage() != true)
	{
		LOGERROR("Received a Station to Master message at the outstation - ignoring - " + std::to_string(Header.GetFunctionCode()) +
			" On Station Address - " + std::to_string(Header.GetStationAddress()));
		//	Let timeout deal with the error
		return;
	}

	bool NotImplemented = false;

	// All are included to allow better error reporting.
	switch (Header.GetFunctionCode())
	{
		case ANALOG_UNCONDITIONAL: // Command and reply
			DoAnalogUnconditional(Header);
			break;
		case ANALOG_DELTA_SCAN: // Command and reply
			DoAnalogDeltaScan(Header);
			break;
		case DIGITAL_UNCONDITIONAL_OBS:
			DoDigitalUnconditionalObs(Header);
			break;
		case DIGITAL_DELTA_SCAN:
			DoDigitalChangeOnly(Header);
			break;
		case HRER_LIST_SCAN:
			// Can be a size of 1 or 2
			ExpectedMessageSize = CompleteMD3Message.size();
			if ((ExpectedMessageSize == 1) || (ExpectedMessageSize == 2))
				DoDigitalHRER(static_cast<MD3BlockFn9&>(Header), CompleteMD3Message);
			else
				ExpectedMessageSize = 1;
			break;
		case DIGITAL_CHANGE_OF_STATE:
			DoDigitalCOSScan(static_cast<MD3BlockFn10&>(Header));
			break;
		case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
			DoDigitalScan(static_cast<MD3BlockFn11MtoS&>(Header));
			break;
		case DIGITAL_UNCONDITIONAL:
			DoDigitalUnconditional(static_cast<MD3BlockFn12MtoS&>(Header));
			break;
		case ANALOG_NO_CHANGE_REPLY:
			NotImplemented = true;
			// Master Only
			break;
		case DIGITAL_NO_CHANGE_REPLY:
			NotImplemented = true;
			// Master Only
			break;
		case CONTROL_REQUEST_OK:
			NotImplemented = true;
			// Master Only
			break;
		case FREEZE_AND_RESET:
			DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
			break;
		case POM_TYPE_CONTROL:
			ExpectedMessageSize = 2;
			DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
			break;
		case DOM_TYPE_CONTROL:
			ExpectedMessageSize = 2;
			DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
			break;
		case INPUT_POINT_CONTROL:
			// Can be a size of 2 or 3
			ExpectedMessageSize = CompleteMD3Message.size();
			if ((ExpectedMessageSize == 2) || (ExpectedMessageSize == 3))
				DoInputPointControl(static_cast<MD3BlockFn20MtoS&>(Header), CompleteMD3Message);
			else
				ExpectedMessageSize = 1;
			break;
		case RAISE_LOWER_TYPE_CONTROL:
			NotImplemented = true;
			break;
		case AOM_TYPE_CONTROL:
			ExpectedMessageSize = 2;
			DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
			break;
		case CONTROL_OR_SCAN_REQUEST_REJECTED:
			NotImplemented = true;
			// Master Only
			break;
		case COUNTER_SCAN:
			DoCounterScan(Header);
			break;
		case SYSTEM_SIGNON_CONTROL:
			DoSystemSignOnControl(static_cast<MD3BlockFn40MtoS&>(Header));
			break;
		case SYSTEM_SIGNOFF_CONTROL:
			NotImplemented = true;
			break;
		case SYSTEM_RESTART_CONTROL:
			NotImplemented = true;
			break;
		case SYSTEM_SET_DATETIME_CONTROL:
			ExpectedMessageSize = 2;
			DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
			break;
		case SYSTEM_SET_DATETIME_CONTROL_NEW:
			ExpectedMessageSize = 3;
			DoSetDateTimeNew(static_cast<MD3BlockFn44MtoS&>(Header), CompleteMD3Message);
			break;
		case FILE_DOWNLOAD:
			NotImplemented = true;
			break;
		case FILE_UPLOAD:
			NotImplemented = true;
			break;
		case SYSTEM_FLAG_SCAN:
			ExpectedMessageSize = CompleteMD3Message.size(); // Variable size
			DoSystemFlagScan(static_cast<MD3BlockFn52MtoS&>(Header), CompleteMD3Message);
			break;
		case LOW_RES_EVENTS_LIST_SCAN:
			NotImplemented = true;
			break;
		default:
			LOGERROR("Unknown Command Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
			break;
	}
	if (NotImplemented == true)
	{
		LOGERROR("Command Function NOT Implemented - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
	}

	if (ExpectedMessageSize != CompleteMD3Message.size())
	{
		LOGERROR("Unexpected Message Size - " + std::to_string(CompleteMD3Message.size()) +
			" On Station Address - " + std::to_string(Header.GetStationAddress())+
			" Function - " + std::to_string(Header.GetFunctionCode()));
	}
}

#ifdef _MSC_VER
#pragma region Worker Methods
#endif

// This method is passed to the SystemFlags variable to do the necessary calculation
// Access through SystemFlags.GetDigitalChangedFlag()
bool MD3OutstationPort::DigitalChangedFlagCalculationMethod(void)
{
	// Return true if there is unsent digital changes
	return (CountBinaryBlocksWithChanges() > 0);
}
// This method is passed to the SystemFlags variable to do the necessary calculation
// Access through SystemFlags.GetTimeTaggedDataAvailableFlag()
bool MD3OutstationPort::TimeTaggedDataAvailableFlagCalculationMethod(void)
{
	return MyPointConf->PointTable.TimeTaggedDataAvailable();
}

#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region ANALOG and COUNTER
#endif
// Function 5
void MD3OutstationPort::DoAnalogUnconditional(MD3BlockFormatted &Header)
{
	LOGDEBUG("{} - DoAnalogUnconditional - Fn5",Name);
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
	SendAnalogOrCounterUnconditional(ANALOG_UNCONDITIONAL,AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
}

// Function 31 - Essentially the same as Analog Unconditional. Either can be used to return either analog or counter, or both.
// The only difference is that an analog module seems to have 16 channels, the counter module only 8.
// The expectation is that if you ask for more than 8 from a counter module, it will roll over to the next module (counter or analog).
// As an analog module has 16, the most that can be requested the overflow never happens.
// In order to make this work, we need to know if the module we are dealing with is a counter or analog module.
void MD3OutstationPort::DoCounterScan(MD3BlockFormatted &Header)
{
	LOGDEBUG("{} - DoCounterScan - Fn31",Name);
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	// This is the method that has to deal with analog/counter channel overflow issues - into the next module.
	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
	SendAnalogOrCounterUnconditional(COUNTER_SCAN, AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
}
// Function 6
void MD3OutstationPort::DoAnalogDeltaScan( MD3BlockFormatted &Header )
{
	LOGDEBUG("{} - DoAnalogDeltaScan - Fn6",Name);
	// First work out what our response will be, loading the data to be sent into two vectors.
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now we know what type of response we are going to send.
	if (ResponseType == AllChange)
	{
		SendAnalogOrCounterUnconditional(ANALOG_UNCONDITIONAL,AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
	else if (ResponseType == DeltaChange)
	{
		SendAnalogDelta(AnalogDeltaValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
	else
	{
		SendAnalogNoChange(Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
}

void MD3OutstationPort::ReadAnalogOrCounterRange(uint8_t ModuleAddress, uint8_t Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues)
{
	// The Analog and Counters are  maintained in two lists, we need to deal with both of them as they both can be read by this method.
	// So if we find an entry in the analog list, we dont have to worry about overflow, as there are 16 channels, and the most we can ask for is 16.
	//
	uint16_t wordres = 0;
	bool hasbeenset;

	// Can get up to 8 values from each module
	uint8_t chancnt = Channels >= 8 ? 8 : Channels;
	// Is it a counter or analog?
	if (MyPointConf->PointTable.GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres, hasbeenset))
	{
		GetAnalogModuleValues(CounterModule,chancnt, ModuleAddress, ResponseType, AnalogValues, AnalogDeltaValues);
	}
	else
	{
		// It must be an analog module (if it does not exist, we return appropriate error codes anyway
		// Yes proceed , up to 16 channels.
		GetAnalogModuleValues(AnalogModule, chancnt, ModuleAddress, ResponseType, AnalogValues, AnalogDeltaValues);
	}
	if (Channels > 8)
	{
		// Now we have to get the remaining channels (up to 8) from the next module, if it exists.
		// Check if it is a counter, otherwise assume analog. Will return correct error codes if not there
		if (MyPointConf->PointTable.GetCounterValueUsingMD3Index(ModuleAddress+1, 0, wordres,hasbeenset))
		{
			GetAnalogModuleValues(CounterModule, Channels - 8, ModuleAddress + 1, ResponseType, AnalogValues, AnalogDeltaValues);
		}
		else
		{
			GetAnalogModuleValues(AnalogModule, Channels - 8, ModuleAddress + 1, ResponseType, AnalogValues, AnalogDeltaValues);
		}
	}

}
void MD3OutstationPort::GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, uint8_t Channels, uint8_t ModuleAddress, MD3OutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t> & AnalogValues, std::vector<int> & AnalogDeltaValues)
{
	for (uint8_t i = 0; i < Channels; i++)
	{
		uint16_t wordres = 0;
		int deltares = 0;
		bool hasbeenset;
		bool foundentry = false;

		if (IsCounterOrAnalog == CounterModule)
		{
			foundentry = MyPointConf->PointTable.GetCounterValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares,hasbeenset);
		}
		else
		{
			foundentry= MyPointConf->PointTable.GetAnalogValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares,hasbeenset);
		}

		if (!foundentry)
		{
			// Point does not exist - If we do an unconditional we send 0x8000, if a delta we send 0 (so the 0x8000 will not change when 0 gets added to it!!)
			AnalogValues.push_back(0x8000); // Magic value
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
			else if ((abs(deltares) > 0) && (ResponseType != AllChange))
			{
				ResponseType = DeltaChange;
			}
		}
	}
}
void MD3OutstationPort::SendAnalogOrCounterUnconditional(MD3_FUNCTION_CODE functioncode, std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	MD3Message_t ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,

	MD3BlockFormatted FormattedBlock = MD3BlockFormatted(StationAddress, false, functioncode, ModuleAddress, Channels);
	FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Analogs.size());

	int NumberOfDataBlocks = Channels / 2 + Channels % 2; // 2 --> 1, 3 -->2

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);
		auto firstblock = Analogs[2 * i];

		uint16_t secondblock = 0;
		if (Analogs.size() > (2u * i + 1))
			secondblock = Analogs[2 * i + 1];

		auto block = MD3BlockData(firstblock, secondblock, lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendMD3Message(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	MD3Message_t ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockFormatted FormattedBlock = MD3BlockFormatted(StationAddress, false, ANALOG_DELTA_SCAN, ModuleAddress, Channels);
	FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Deltas.size());

	// Can be 4 channel delta values to a block.
	int NumberOfDataBlocks = Channels / 4 + (Channels % 4 == 0 ? 0 : 1);

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		// Check for overflows, we can only guarantee the first value....
		auto block1 = numeric_cast<uint8_t>(Deltas[i * 4]);
		uint8_t block2 = 0;
		uint8_t block3 = 0;
		uint8_t block4 = 0;

		if (Deltas.size() > (i * 4u + 1))
			block2 = numeric_cast<uint8_t>(Deltas[i * 4 + 1]);
		if (Deltas.size() > (i * 4u + 2))
			block3 = numeric_cast<uint8_t>(Deltas[i * 4 + 2]);
		if (Deltas.size() > (i * 4u + 3))
			block4 = numeric_cast<uint8_t>(Deltas[i * 4 + 3]);

		auto block = MD3BlockData(block1, block2 , block3, block4, lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendMD3Message(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	MD3Message_t ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockFormatted FormattedBlock = MD3BlockFormatted(StationAddress, false, ANALOG_NO_CHANGE_REPLY, ModuleAddress, Channels, true);
	FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}

#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region DIGITAL
#endif
// Function 7
void MD3OutstationPort::DoDigitalUnconditionalObs(MD3BlockFormatted &Header)
{
	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	MD3Message_t ResponseMD3Message;

	LOGDEBUG("{} - DoDigitalUnconditionalObs - Fn7",Name);

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockFormatted FormattedBlock = MD3BlockFormatted(Header.GetStationAddress(), false, DIGITAL_UNCONDITIONAL_OBS, Header.GetModuleAddress(), Header.GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	uint8_t NumberOfDataBlocks = Header.GetChannels(); // Actually the number of modules

	BuildBinaryReturnBlocks(NumberOfDataBlocks, Header.GetModuleAddress(), Header.GetStationAddress(), true, ResponseMD3Message);
	SendMD3Message(ResponseMD3Message);
}
// Function 8
void MD3OutstationPort::DoDigitalChangeOnly(MD3BlockFormatted &Header)
{
	// Have three possible replies, Digital Unconditional #7,  Delta Scan #8 and Digital No Change #14
	// So the data is deemed to have changed if a bit has changed, or the status has changed.
	// We detect changes on a module basis, and send the data in the same format as the Digital Unconditional.
	// So the Delta Scan can be all the same data as the unconditional.
	// If no changes, send the single #14 function block. Keep the module and channel values matching the orginal message.

	LOGDEBUG("{} - DoDigitalChangeOnly - Fn8",Name);

	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	MD3Message_t ResponseMD3Message;

	uint8_t NumberOfDataBlocks = Header.GetChannels(); // Actually the number of modules - 0 numbered, does not make sense to ask for none...

	uint8_t ChangedBlocks = CountBinaryBlocksWithChangesGivenRange(NumberOfDataBlocks, Header.GetModuleAddress());

	if (ChangedBlocks == 0) // No change
	{
		MD3BlockFormatted FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
		FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

		ResponseMD3Message.push_back(FormattedBlock);

		SendMD3Message(ResponseMD3Message);
	}
	else if (ChangedBlocks != NumberOfDataBlocks) // Some change
	{
		//TODO:  OLD STYLE DIGITAL - What are the module and channel set to in Function 8 digital change response packets - packet count can vary..
		MD3BlockData FormattedBlock = MD3BlockFormatted(Header.GetStationAddress(), false, DIGITAL_DELTA_SCAN, Header.GetModuleAddress(), ChangedBlocks);
		ResponseMD3Message.push_back(FormattedBlock);

		BuildBinaryReturnBlocks(NumberOfDataBlocks, Header.GetModuleAddress(), Header.GetStationAddress(), false, ResponseMD3Message);

		SendMD3Message(ResponseMD3Message);
	}
	else
	{
		// All changed..do Digital Unconditional
		DoDigitalUnconditionalObs(Header);
	}
}

// Function 9
void MD3OutstationPort::DoDigitalHRER(MD3BlockFn9 &Header, MD3Message_t &CompleteMD3Message)
{
	// We have a separate queue of time tagged digital events. This method only processes that queue.
	//
	// The master will start with sequence # of zero, and there after use 1-15. If we get zero, we respond with 1
	// If the sequence number is non-zero, we resend that response.
	//
	// If the maxiumum events field is zero, then the next block in the message will contain the time. Another way to set the time.
	//
	// The date and time block is a 32 bit number representing the number of seconds since 1/1/1970
	// The milliseconds component is derived from the offset component in the module data block.
	//
	// Because there can be 3 different tyeps of 16 bit words in the response, including filler packets,
	// we need to build the response as those words first, then convert to MD3Blocks - with a potential filler packet in the last word of the last block.
	LOGDEBUG("{} - DoDigitalHRER - Fn9",Name);
	MD3Message_t ResponseMD3Message;


	if (Header.GetSequenceNumber() == LastHRERSequenceNumber)
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.
		LOGDEBUG("{} - Resend Last Response",Name);
		SendMD3Message(LastDigitialHRERResponseMD3Message);
		return;
	}

	if (Header.GetSequenceNumber() == 0)
	{
		// The master has just started up. We will respond with a sequence number of 1.
		LastHRERSequenceNumber = 1;
	}
	else
		LastHRERSequenceNumber = Header.GetSequenceNumber();

	// Have to change the last two variables (event count and more events) after we have processed the queue.
	MD3BlockFn9 FormattedBlock = MD3BlockFn9(Header.GetStationAddress(), false, LastHRERSequenceNumber, 0, true);
	ResponseMD3Message.push_back(FormattedBlock);
	uint8_t EventCount = 0;

	if (Header.GetEventCount() == 0)
	{
		// There should be a second packet, and this is actually a set time/date command
		SendControlOrScanRejected(Header); // We are not supporting this message so send command rejected message.

		// We just log that we got it at the moment
		LOGERROR("Received a Fn 9 Set Time/Date Command - not handled. Station Address - " + std::to_string(Header.GetStationAddress()));
		//TODO: OLD STYLE DIGITAL - Pass through FN9 zero maxiumumevents used to set time in outstation.
		return;
	}
	else
	{
		std::vector<uint16_t> ResponseWords;

		// Handle a normal packet - each event can be a word( 16 bits) plus time and other words
		Fn9AddTimeTaggedDataToResponseWords(Header.GetEventCount(), EventCount, ResponseWords);

		//TODO: OLD STYLE DIGITAL - Fn9 Add any Internal HRER events unrelated to the digital bits. EventBufferOverflow (1) is the only one of interest. Use Zero Module address and 1 in the channel field. The time is when this occurred

		if ((ResponseWords.size() % 2) != 0)
		{
			// Add a filler packet if necessary
			ResponseWords.push_back(MD3BlockFn9::FillerPacket());
		}

		// Now translate the 16 bit packets into the 32 bit MD3 blocks.
		for (uint16_t i = 0; i < ResponseWords.size(); i = i + 2)
		{
			MD3BlockData blk(ResponseWords[i], ResponseWords[i + 1]);
			ResponseMD3Message.push_back(blk);
		}
	}

	MD3BlockData &lastblock = ResponseMD3Message.back();
	lastblock.MarkAsEndOfMessageBlock();

	auto &firstblock = static_cast<MD3BlockFn9&>(ResponseMD3Message[0]);
	bool MoreEventsFlag = MyPointConf->PointTable.TimeTaggedDataAvailable();
	firstblock.SetEventCountandMoreEventsFlag(EventCount, MoreEventsFlag); // If not empty, set more events (MEV) flag

	LOGDEBUG("{} - Sending {} Events, More Events Flag - {} Sequence Number - {}",Name,EventCount, MoreEventsFlag, LastHRERSequenceNumber);

	LastDigitialHRERResponseMD3Message = ResponseMD3Message; // Copy so we can resend if necessary
	SendMD3Message(ResponseMD3Message);
}

void MD3OutstationPort::Fn9AddTimeTaggedDataToResponseWords( uint8_t MaxEventCount, uint8_t &EventCount, std::vector<uint16_t> &ResponseWords)
{
	MD3BinaryPoint CurrentPoint;
	uint64_t LastPointmsec = 0;
	bool CanSend = true;

	while ((EventCount < MaxEventCount) && CanSend && MyPointConf->PointTable.PeekNextTaggedEventPoint(CurrentPoint)) // Don't pop until we are happy...
	{
		if (EventCount == 0)
		{
			// First packet is the time/date block and a milliseconds packet
			auto FirstEventSeconds = static_cast<uint32_t>(CurrentPoint.GetChangedTime() / 1000);
			auto FirstEventMSec = static_cast<uint16_t>(CurrentPoint.GetChangedTime() % 1000);
			ResponseWords.push_back(FirstEventSeconds >> 16);
			ResponseWords.push_back(FirstEventSeconds & 0x0FFFF);
			ResponseWords.push_back(MD3BlockFn9::MilliSecondsPacket(FirstEventMSec));
			LastPointmsec = CurrentPoint.GetChangedTime();
		}
		else
		{
			// Do we need a Milliseconds packet? If so, is the gap more than 31 seconds? If so, finish this response off, so master can issue a new HRER request.
			uint64_t delta = CurrentPoint.GetChangedTime() - LastPointmsec;
			if (delta > 31999)
			{
				//Delta > 31.999 seconds, We can't send this point, finish the packet so the master can ask for a new HRER response.
				CanSend = false;
			}
			else if (delta != 0)
			{
				ResponseWords.push_back(MD3BlockFn9::MilliSecondsPacket(static_cast<uint16_t>(delta)));
				LastPointmsec = CurrentPoint.GetChangedTime(); // The last point time moves with time added by the msec packets
			}
		}

		if (CanSend)
		{
			ResponseWords.push_back(MD3BlockFn9::HREREventPacket(CurrentPoint.GetBinary(), CurrentPoint.GetChannel(), CurrentPoint.GetModuleAddress()));

			MyPointConf->PointTable.PopNextTaggedEventPoint();
			EventCount++;
		}
	}
}

// Function 10
void MD3OutstationPort::DoDigitalCOSScan(MD3BlockFn10 &Header)
{
	// Have two possible replies COSScan #10 and Digital No Change #14
	// So the data is deemed to have changed if a bit has changed, or the status has changed.
	// If no changes, send the single #14 function block.
	// If module address is 0, then start scan from first module. If not 0 wrap scan around if we have space to send more data
	// The module count is the number of blocks we can return

	// The return data blocks are the same as for Fn 7
	LOGDEBUG("{} - DoDigitalCOSScan - Fn10",Name);

	MD3Message_t ResponseMD3Message;
	uint8_t NumberOfDataBlocks = Header.GetModuleCount();

	bool ChangedBlocks = (CountBinaryBlocksWithChanges() != 0);

	if (ChangedBlocks == false) // No change
	{
		MD3BlockFormatted FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetModuleAddress(), static_cast<uint8_t>(0));
		FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

		ResponseMD3Message.push_back(FormattedBlock);

		SendMD3Message(ResponseMD3Message);
	}
	else
	{
		// The number of changed blocks is how many will follow the header packet. Have to change the value at the end...
		MD3BlockFn10 FormattedBlock = MD3BlockFn10(Header.GetStationAddress(), false,  Header.GetModuleAddress(), 0, false);
		ResponseMD3Message.push_back(FormattedBlock);

		// First we have to get the changed ModuleAddresses
		std::vector<uint8_t> ModuleList;
		BuildListOfModuleAddressesWithChanges(Header.GetModuleAddress(), ModuleList);

		BuildScanReturnBlocksFromList(ModuleList, NumberOfDataBlocks, Header.GetStationAddress(), false, ResponseMD3Message);

		auto &firstblock = static_cast<MD3BlockFn10 &>(ResponseMD3Message[0]);
		firstblock.SetModuleCount(static_cast<uint8_t>(ResponseMD3Message.size() - 1)); // The number of blocks taking away the header...

		SendMD3Message(ResponseMD3Message);
	}
}
// Function 11
void MD3OutstationPort::DoDigitalScan(MD3BlockFn11MtoS &Header)
{
	// So we scan all our digital modules, creating change records for where there are changes.
	// We can return non-time tagged data - up to the number given by ModuleCount.
	// Following this we can return time tagged data - up to the the number given by TaggedEventCount. Both are 1 numbered - i.e. 0 is a valid value.
	// The time tagging in the modules is optional, so need to build that into our config file. It is the module capability that determines what we return.
	//
	// If the sequence number is zero, every time tagged module will be sent. It is used by the master to reset its state on power up.
	//
	// The date and time block is a 32 bit number representing the number of seconds since 1/1/1970
	// The milliseconds component is derived from the offset component in the module data block.
	//
	// If we get another scan message (and nothing else changes) we will send the next block of changes and so on.

	LOGDEBUG("{} - DoDigitalScan - Fn11 includes COS",Name);

	MD3Message_t ResponseMD3Message;

	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Don't do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

		SendMD3Message(LastDigitialScanResponseMD3Message);
		return;
	}

	if (Header.GetDigitalSequenceNumber() == 0)
	{
		// This will be called when we get a zero sequence number for Fn 11 or 12. It is sent on Master start-up to ensure that all data is sent in following
		// change only commands - if there are sufficient modules
		MarkAllBinaryPointsAsChanged();
	}

	uint8_t ChangedBlocks = CountBinaryBlocksWithChanges();
	bool AreThereTaggedEvents = MyPointConf->PointTable.TimeTaggedDataAvailable();

	if ((ChangedBlocks == 0) && !AreThereTaggedEvents)
	{
		// No change block
		LOGDEBUG("{} - DoDigitalUnconditional - Nothing to send",Name);
		MD3BlockFormatted FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetDigitalSequenceNumber());
		FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

		ResponseMD3Message.push_back(FormattedBlock);
	}
	else
	{
		// We have data to send.
		uint8_t TaggedEventCount = 0;
		uint8_t ModuleCount = Limit(ChangedBlocks, Header.GetModuleCount());

		// Set up the response block
		MD3BlockFn11StoM FormattedBlock(Header.GetStationAddress(), TaggedEventCount, Header.GetDigitalSequenceNumber(), ModuleCount);
		ResponseMD3Message.push_back(FormattedBlock);

		// Add non-time tagged data. We can return a data block or a status block for each module.
		// If the module is active (not faulted) return the data, otherwise return the status block..
		// We always just start with the first module and move up through changed blocks until we reach the ChangedBlocks count

		std::vector<uint8_t> ModuleList;
		BuildListOfModuleAddressesWithChanges(0, ModuleList);

		BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseMD3Message);

		ModuleCount = static_cast<uint8_t>(ResponseMD3Message.size() - 1); // The number of module block is the size at this point, less the start block.

		if (AreThereTaggedEvents)
		{
			// Add time tagged data
			std::vector<uint16_t> ResponseWords;

			Fn11AddTimeTaggedDataToResponseWords(Header.GetTaggedEventCount(), TaggedEventCount, ResponseWords); // TaggedEvent count is a ref, zero on entry and exit with number of tagged events added

			// Convert data to 32 bit blocks and pad if necessary.
			if ((ResponseWords.size() % 2) != 0)
			{
				// Add a filler packet if necessary to the end of the message. High byte zero, low byte does not matter.
				ResponseWords.push_back(MD3BlockFn11StoM::FillerPacket());
			}

			//TODO: NEW Style Digital - A flag block can appear in the OutStation Fn11 time tagged response - which can indicate Internal Buffer Overflow, Time sync fail and restoration. We have not implemented this

			// Now translate the 16 bit packets into the 32 bit MD3 blocks.
			for (uint16_t i = 0; i < ResponseWords.size(); i = i + 2)
			{
				MD3BlockData blk(ResponseWords[i], ResponseWords[i + 1]);
				ResponseMD3Message.push_back(blk);
			}
		}

		// Mark the last block and set the module count and tagged event count
		if (ResponseMD3Message.size() != 0)
		{
			auto &firstblockref = static_cast<MD3BlockFn11StoM&>(ResponseMD3Message.front());
			firstblockref.SetModuleCount(ModuleCount);           // The number of blocks taking away the header...
			firstblockref.SetTaggedEventCount(TaggedEventCount); // Update to the number we are sending
			firstblockref.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

			MD3BlockData &lastblock = ResponseMD3Message.back();
			lastblock.MarkAsEndOfMessageBlock();
		}
		LOGDEBUG("{} - DoDigitalUnconditional - Sending {} Module Data Words and {} Tagged Events",Name, ModuleCount,TaggedEventCount);
	}
	SendMD3Message(ResponseMD3Message);

	// Store this set of packets in case we have to resend
	LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
	LastDigitialScanResponseMD3Message = ResponseMD3Message;
}

void MD3OutstationPort::MarkAllBinaryPointsAsChanged()
{
	MyPointConf->PointTable.ForEachBinaryPoint([](MD3BinaryPoint &pt)
		{
			pt.SetChangedFlag();
		});
}

void MD3OutstationPort::Fn11AddTimeTaggedDataToResponseWords(uint8_t MaxEventCount, uint8_t &EventCount, std::vector<uint16_t> &ResponseWords)
{
	MD3BinaryPoint CurrentPoint;
	uint64_t LastPointmsec = 0;

	while ((EventCount < MaxEventCount) && MyPointConf->PointTable.PeekNextTaggedEventPoint(CurrentPoint))
	{
		// The time date is seconds, the data block contains a msec entry.
		// The time is accumulated from each event in the queue. If we have greater than 255 msec between events,
		// add a time packet to bridge the gap.

		if (EventCount == 0)
		{
			// First packet is the time/date block - adjust for SOEOffset (handles local utc time translation)
			// We dont currently do anything with the timezone sent in a 44 command
			auto FirstEventSeconds = static_cast<uint32_t>(CurrentPoint.GetChangedTime() / 1000 + SOETimeOffsetMinutes*60);
			ResponseWords.push_back(FirstEventSeconds >> 16);
			ResponseWords.push_back(FirstEventSeconds & 0x0FFFF);
			LastPointmsec = CurrentPoint.GetChangedTime() - CurrentPoint.GetChangedTime() % 1000; // The first one is seconds only. Later events have actual msec
		}

		uint64_t msecoffset = CurrentPoint.GetChangedTime() - LastPointmsec;

		if (msecoffset > 255) // Do we need a Milliseconds extension packet? A TimeBlock
		{
			uint64_t msecoffsetdiv256 = msecoffset / 256;

			if (msecoffsetdiv256 > 255)
			{
				// Huston we have a problem, to much time offset..
				// The master will have to do another scan to get this data. So here we will just return as if we have sent all we can
				// The point we were looking at remains on the queue to be processed on the next call.
				return;
			}
			assert(msecoffsetdiv256 < 256);
			ResponseWords.push_back(numeric_cast<uint16_t>(msecoffsetdiv256));
			LastPointmsec += msecoffsetdiv256 * 256; // The last point time moves with time added by the msec packet
			msecoffset = CurrentPoint.GetChangedTime() - LastPointmsec;
		}

		// Push the block onto the response word list
		assert(msecoffset < 256);
		ResponseWords.push_back(ShiftLeft8Result16Bits(CurrentPoint.GetModuleAddress()) | numeric_cast<uint16_t>(msecoffset));
		ResponseWords.push_back(CurrentPoint.GetModuleBinarySnapShot());

		LastPointmsec = CurrentPoint.GetChangedTime(); // Update the last changed time to match what we have just sent.
		MyPointConf->PointTable.PopNextTaggedEventPoint();
		EventCount++;
	}
}
// Function 12
void MD3OutstationPort::DoDigitalUnconditional(MD3BlockFn12MtoS &Header)
{
	MD3Message_t ResponseMD3Message;

	LOGDEBUG("{} - DoDigitalUnconditional - Fn12",Name);

	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

		SendMD3Message(LastDigitialScanResponseMD3Message);
		return;
	}

	// The reply is the same format as for Fn11, but without time tagged data. We return Fn 11 always
	std::vector<uint8_t> ModuleList;
	BuildListOfModuleAddressesWithChanges(Header.GetModuleCount(),Header.GetModuleAddress(), true, ModuleList);

	// Set up the response block - module count to be updated
	MD3BlockFn11StoM FormattedBlock(Header.GetStationAddress(), 0, Header.GetDigitalSequenceNumber(), Header.GetModuleCount());
	ResponseMD3Message.push_back(FormattedBlock);

	BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseMD3Message);

	// Mark the last block
	if (ResponseMD3Message.size() != 0)
	{
		auto &firstblockref = static_cast<MD3BlockFn11StoM&>(ResponseMD3Message.front());
		firstblockref.SetModuleCount(static_cast<uint8_t>(ResponseMD3Message.size() - 1)); // The number of blocks taking away the header...
		firstblockref.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

		MD3BlockData &lastblock = ResponseMD3Message.back();
		lastblock.MarkAsEndOfMessageBlock();
	}

	SendMD3Message(ResponseMD3Message);

	// Store this set of packets in case we have to resend
	LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
	LastDigitialScanResponseMD3Message = ResponseMD3Message;
}

// Scan all binary/digital blocks for changes - used to determine what response we need to send
// We return the total number of changed blocks we assume every block supports time tagging
// If SendEverything is true,
uint8_t MD3OutstationPort::CountBinaryBlocksWithChanges()
{
	uint8_t changedblocks = 0;
	int lastblock = -1; // Non valid value

	// The map is sorted, so when iterating, we are working to a specific order. We can have up to 16 points in a block only one changing will trigger a send.
	MyPointConf->PointTable.ForEachBinaryPoint([&lastblock,&changedblocks](MD3BinaryPoint &pt)
		{
			if (pt.GetChangedFlag())
			{
			// Multiple bits can be changed in the block, but only the first one is required to trigger a send of the block.
			      if (lastblock != pt.GetModuleAddress())
			      {
			            lastblock = pt.GetModuleAddress();
			            changedblocks++;
				}
			}
		});

	return changedblocks;
}
// This is used to determine which response we should send NoChange, DeltaChange or AllChange
uint8_t MD3OutstationPort::CountBinaryBlocksWithChangesGivenRange(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress)
{
	uint8_t changedblocks = 0;

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool datachanged = false;

		for (uint8_t j = 0; j < 16; j++)
		{
			bool changed = false;

			if (!MyPointConf->PointTable.GetBinaryChangedUsingMD3Index(StartModuleAddress + i, j, changed)) // Does not change the changed bit
			{
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

// Used in Fn7, Fn8 and Fn12
void MD3OutstationPort::BuildListOfModuleAddressesWithChanges(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, bool forcesend, std::vector<uint8_t> &ModuleList)
{
	// We want a list of modules to send...
	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool datachanged = false;

		for (uint8_t j = 0; j < 16; j++)
		{
			bool changed = false;

			if (!MyPointConf->PointTable.GetBinaryChangedUsingMD3Index(StartModuleAddress + i, j, changed))
			{
				// Data is missing, need to send the error block for this module address.
				changed = true;
			}
			if (changed)
				datachanged = true;
		}
		if (datachanged || forcesend)
		{
			ModuleList.push_back(StartModuleAddress + i);
		}
	}
}
// Fn 10
void MD3OutstationPort::BuildListOfModuleAddressesWithChanges(uint8_t StartModuleAddress, std::vector<uint8_t> &ModuleList)
{
	uint8_t LastModuleAddress = 0;
	bool WeAreScanning = (StartModuleAddress == 0); // If the startmoduleaddress is zero, we store changes from the start.

	MyPointConf->PointTable.ForEachBinaryPoint([StartModuleAddress,&WeAreScanning,&ModuleList,&LastModuleAddress](MD3BinaryPoint &Point)
		{
			uint8_t ModuleAddress = Point.GetModuleAddress();

			if (ModuleAddress == StartModuleAddress)
				WeAreScanning = true;

			if (WeAreScanning && Point.GetChangedFlag() && (LastModuleAddress != ModuleAddress))
			{
			// We should save the module address so we have a list of modules that have changed
			      ModuleList.push_back(ModuleAddress);
			      LastModuleAddress = ModuleAddress;
			}
		});

	if (StartModuleAddress != 0)
	{
		// We have to then start again from zero, if we did not start from there.
		MyPointConf->PointTable.ForEachBinaryPoint([StartModuleAddress,&WeAreScanning,&ModuleList,&LastModuleAddress](MD3BinaryPoint &Point)
			{
				uint8_t ModuleAddress = Point.GetModuleAddress();

				if (ModuleAddress == StartModuleAddress)
					WeAreScanning = false; // Stop when we reach the start again

				if (WeAreScanning && Point.GetChangedFlag() && (LastModuleAddress != ModuleAddress))
				{
				// We should save the module address so we have a list of modules that have changed
				      ModuleList.push_back(ModuleAddress);
				      LastModuleAddress = ModuleAddress;
				}
			});
	}
}
// Fn 7,8
void MD3OutstationPort::BuildBinaryReturnBlocks(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, uint8_t StationAddress, bool forcesend, MD3Message_t &ResponseMD3Message)
{
	std::vector<uint8_t> ModuleList;
	BuildListOfModuleAddressesWithChanges(NumberOfDataBlocks, StartModuleAddress, forcesend, ModuleList);
	// We have a list of modules to send...
	BuildScanReturnBlocksFromList(ModuleList, NumberOfDataBlocks, StationAddress,false, ResponseMD3Message);
}

// Fn 7, 8 and 10, 11 and 12 NOT Fn 9
void MD3OutstationPort::BuildScanReturnBlocksFromList(std::vector<unsigned char> &ModuleList, uint8_t MaxNumberOfDataBlocks, uint8_t StationAddress, bool FormatForFn11and12, MD3Message_t & ResponseMD3Message)
{
	// For each module address, or the max we can send
	for (size_t i = 0; ((i < ModuleList.size()) && (i < MaxNumberOfDataBlocks)); i++)
	{
		uint8_t ModuleAddress = ModuleList[i];

		// Have to collect all the bits into a uint16_t
		bool ModuleFailed = false;

		uint16_t wordres = MyPointConf->PointTable.CollectModuleBitsIntoWordandResetChangeFlags(ModuleAddress, ModuleFailed);

		if (ModuleFailed)
		{
			if (FormatForFn11and12)
			{
				//TODO: NEW STYLE DIGITAL -  Module failed response for Fn11/12 BuildScanReturnBlocksFromList
			}
			else
			{
				// Queue the error block - Fn 7, 8 and 10 format
				uint8_t errorflags = 0; // Application dependent, depends on the outstation implementation/master expectations. We could build in functionality here
				uint16_t lowword = ShiftLeft8Result16Bits(errorflags) | ModuleAddress;
				uint16_t highword = ShiftLeft8Result16Bits(StationAddress);
				auto block = MD3BlockData(highword, lowword, false);
				ResponseMD3Message.push_back(block);
			}
		}
		else
		{
			if (FormatForFn11and12)
			{
				// For Fn11 and 12 the data format is:
				uint16_t address = ShiftLeft8Result16Bits(ModuleAddress); // Low byte is msec offset - which is 0 for non time tagged data
				auto block = MD3BlockData(address, wordres, false);
				ResponseMD3Message.push_back(block);
			}
			else
			{
				// Queue the data block Fn 7,8 and 10
				uint16_t address = ShiftLeft8Result16Bits(StationAddress) | ModuleAddress;
				auto block = MD3BlockData(address, wordres, false);
				ResponseMD3Message.push_back(block);
			}
		}
	}
	// Not sure which is the last block for the send only changes..so just mark it when we get to here.
	// Format Fn 11 and 12 may not be the end of packet
	if ((ResponseMD3Message.size() != 0) && !FormatForFn11and12)
	{
		// The header for Fn7, Fn 8 and Fn 10 need this. NOT Fn9, Fn11 and Fn12
		auto &firstblockref = static_cast<MD3BlockFormatted&>(ResponseMD3Message.front());
		firstblockref.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

		MD3BlockData &lastblock = ResponseMD3Message.back();
		lastblock.MarkAsEndOfMessageBlock();
	}
}


#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region CONTROL
#endif

void MD3OutstationPort::DoFreezeResetCounters(MD3BlockFn16MtoS &Header)
{
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.
	// Don't have an equivalent ODC command?

	bool failed = false;

	if (!Header.IsValid())
	{
		// Message verification failed - something was corrupted
		failed = true;
	}

	if (Header.GetStationAddress() != 0)
	{
		if (failed)
		{
			SendControlOrScanRejected(Header);
			return;
		}

		SendControlOK(Header);
	}
}

// Think that POM stands for PULSE.
void MD3OutstationPort::DoPOMControl(MD3BlockFn17MtoS &Header, MD3Message_t &CompleteMD3Message)
{
	// We have two blocks incoming, not just one. Channel is 0 to 15
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.

	bool failed = false;

	// Check all the things we can
	if (CompleteMD3Message.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}

	if (!Header.VerifyAgainstSecondBlock(CompleteMD3Message[1]))
	{
		// Message verification failed - something was corrupted
		failed = true;
	}

	// Check that the control point is defined (and get the index), otherwise return a fail.
	size_t ODCIndex = 0;
	failed = MyPointConf->PointTable.GetBinaryControlODCIndexUsingMD3Index(Header.GetModuleAddress(), Header.GetOutputSelection(), ODCIndex) ? failed : true;

	LOGDEBUG("{} - DoPOMControl - Address {}, Channel {}, ODCIndex {}", Name, Header.GetModuleAddress(), Header.GetOutputSelection(), ODCIndex);

	if (Header.GetStationAddress() == 0)
	{
		// An all station command we do not respond to.
		return;
	}

	if (failed)
	{
		LOGDEBUG("{} - DoPOMControl, Failed", Name);
		SendControlOrScanRejected(Header);
		return;
	}

	bool waitforresult = !MyPointConf->StandAloneOutstation;
	bool success = true;

	// POM is only a single bit, so easy to do...
	// For AUSGRID Systems, POM modules are always PULSE_ON, and what you pulse determines what happens (can be a trip or close)

	if (FailControlResponse)
	{
		SendControlOrScanRejected(Header);
		return;
	}

	// Module contains 0 to 15 channels..
	EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
	val.functionCode = ControlCode::PULSE_ON; // Always pulse on for POM control!

	auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
	event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

	success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS); // If no subscribers will return quickly.

	if (success)
	{
		SendControlOK(Header);
	}
	else
	{
		SendControlOrScanRejected(Header);
	}
}

// DOM stands for DIGITAL.
void MD3OutstationPort::DoDOMControl(MD3BlockFn19MtoS &Header, MD3Message_t &CompleteMD3Message)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.
	// The output is a 16bit word. Will send a 32 bit block through ODC that contains the output data , station address and module address.
	// The Perform method waits until it has a result, unless we are in stand alone mode.

	LOGDEBUG("{} - DoDOMControl",Name);
	bool failed = false;

	// Check all the things we can
	if (CompleteMD3Message.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}

	if (!Header.VerifyAgainstSecondBlock(CompleteMD3Message[1]))
	{
		// Message verification failed - something was corrupted
		failed = true;
	}

	// Check that the first one exists, not all 16 may exist.
	size_t ODCIndex = 0;
	failed = MyPointConf->PointTable.GetBinaryControlODCIndexUsingMD3Index(Header.GetModuleAddress(), 0, ODCIndex) ? failed : true;

	uint16_t output = Header.GetOutputFromSecondBlock(CompleteMD3Message[1]);
	bool waitforresult = !MyPointConf->StandAloneOutstation;

	if (Header.GetStationAddress() == 0)
	{
		// An all station command we do not respond to.
		return;
	}

	if (failed)
	{
		SendControlOrScanRejected(Header);
		return;
	}

	if (FailControlResponse)
	{
		SendControlOrScanRejected(Header);
		return;
	}

	bool success = true;

	// Send each of the DigitalOutputs
	for (uint8_t i = 0; i < 16; i++)
	{
		if (MyPointConf->PointTable.GetBinaryControlODCIndexUsingMD3Index(Header.GetModuleAddress(), i, ODCIndex))
		{
			EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
			val.functionCode = ((output >> (15 - i) & 0x01) == 1) ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

			auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
			event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

			if (CommandStatus::SUCCESS != Perform(event, waitforresult)) // If no subscribers will return quickly.
			{
				success = false;
			}
		}
	}

	if (success)
	{
		SendControlOK(Header);
	}
	else
	{
		SendControlOrScanRejected(Header);
	}
}


void MD3OutstationPort::DoInputPointControl(MD3BlockFn20MtoS& Header, MD3Message_t& CompleteMD3Message)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected

	bool failed = false;
	uint16_t output = 0;

	// Check all the things we can
	if (!((CompleteMD3Message.size() == 2) || (CompleteMD3Message.size() == 3)))
	{
		// If we did not get two or three blocks, then send back a command rejected message.
		failed = true;
	}

	if (!Header.VerifyAgainstSecondBlock(CompleteMD3Message[1]))
	{
		// Message verification failed - something was corrupted
		failed = true;
	}

	if (CompleteMD3Message.size() == 3)
	{
		if (!Header.VerifyThirdBlock(CompleteMD3Message[2]))
		{
			// Message verification failed - something was corrupted
			failed = true;
		}
		else
			output = Header.GetOutputFromThirdBlock(CompleteMD3Message[2]);
	}
	if ((CompleteMD3Message.size() == 2) && (Header.GetControlSelection() == SETPOINT))
	{
		failed = true;
		LOGERROR("{} - DoInputPointControl, Failed received a SETPOINT command, but only 2 blocks - should be 3", Name);
	}

	// Check that the control point is defined, otherwise return a fail.
	size_t ODCIndex = 0;
	if (Header.GetControlSelection() == SETPOINT)
	{
		// If it is a set point, we consider it an analog control...
		if (!MyPointConf->PointTable.GetAnalogControlODCIndexUsingMD3Index(Header.GetModuleAddress(), Header.GetChannelSelection(), ODCIndex))
		{
			LOGDEBUG("{} - DoInputPointControl - Could not find matching analog control point - Address {}, Control {}, Channel {}", Name, Header.GetModuleAddress(), ToString(Header.GetControlSelection()), Header.GetChannelSelection());
			failed = true;
		}
	}
	else
	{
		if (!MyPointConf->PointTable.GetBinaryControlODCIndexUsingMD3Index(Header.GetModuleAddress(), Header.GetChannelSelection(), ODCIndex))
		{
			LOGDEBUG("{} - DoInputPointControl - Could not find matching point - Address {}, Control {}, Channel {}", Name, Header.GetModuleAddress(), ToString(Header.GetControlSelection()), Header.GetChannelSelection());
			failed = true;
		}
	}
	// We just treat this as a slightly different form of output control. So we send the same commands to the mapped ODC index.
	LOGDEBUG("{} - DoInputPointControl - Address {}, Control {}, Channel {}, Blocks {}, Output {}", Name, Header.GetModuleAddress(), ToString(Header.GetControlSelection()), Header.GetChannelSelection(), CompleteMD3Message.size(),output);

	if (Header.GetStationAddress() == 0)
	{
		// An all station command we do not respond to.
		return;
	}

	if (failed)
	{
		LOGDEBUG("{} - DoInputPointControl, Failed", Name);
		SendControlOrScanRejected(Header);
		return;
	}

	if (FailControlResponse)
	{
		SendControlOrScanRejected(Header);
		return;
	}

	bool waitforresult = !MyPointConf->StandAloneOutstation;
	bool success = true;

	if (CompleteMD3Message.size() == 3)
	{
		if (Header.GetControlSelection() == SETPOINT) // Analog Setpoint command
		{
			EventTypePayload<EventType::AnalogOutputInt16>::type val;
			val.first = numeric_cast<int16_t>(output);

			auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ODCIndex, Name);
			event->SetPayload<EventType::AnalogOutputInt16>(std::move(val));
			success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS); // If no subscribers will return quickly.
		}
		else
		{
			// Think is a trigger for a LPC routine on the RTU.
			// The code below pretends to work in a simulator setting - but cannot be correct.
			// Need to understand what the code is, where it is stored in DNMS and how to respond.
			LOGDEBUG("{} - DoInputPointControl, Warning - received a reserved Control Code - taking a default acton", Name);
			EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
			val.functionCode = ControlCode::PULSE_ON;
			val.onTimeMS = output;                        //used to send value with command
			val.offTimeMS = Header.GetControlSelection(); // Should be 9 for our case
			auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
			event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));
			success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS); // If no subscribers will return quickly.
		}
	}
	else
	{
		EventTypePayload<EventType::ControlRelayOutputBlock>::type val;

		switch (Header.GetControlSelection())
		{
			case TRIP: val.functionCode = ControlCode::TRIP_PULSE_ON;
				break;
			case CLOSE: val.functionCode = ControlCode::CLOSE_PULSE_ON;
				break;
			case RAISE: val.functionCode = ControlCode::PULSE_ON;
				break;
			case LOWER: val.functionCode = ControlCode::PULSE_OFF; // No Raise/Lower??
				break;
			case SINGLEPOLEOPERATE: val.functionCode = ControlCode::PULSE_ON;
				break;
			case OFF: val.functionCode = ControlCode::LATCH_OFF;
				break;
			case ON: val.functionCode = ControlCode::LATCH_ON;
				break;
			default: val.functionCode = ControlCode::PULSE_ON;
				break;
		}

		auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
		event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));
		success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS); // If no subscribers will return quickly.
	}

	if (success )
	{
		SendControlOK(Header);
	}
	else
	{
		SendControlOrScanRejected(Header);
	}
}

// AOM is ANALOG output control.
void MD3OutstationPort::DoAOMControl(MD3BlockFn23MtoS &Header, MD3Message_t &CompleteMD3Message)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.

	LOGDEBUG("{} - DoAOMControl",Name);

	bool failed = false;

	// Check all the things we can
	if (CompleteMD3Message.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}

	if (!Header.VerifyAgainstSecondBlock(CompleteMD3Message[1]))
	{
		// Message verification failed - something was corrupted
		failed = true;
	}

	if (Header.GetStationAddress() == 0)
	{
		// An all station command we do not respond to.
		return;
	}

	// Check that the control point is defined, otherwise return a fail.
	size_t ODCIndex = 0;
	failed = MyPointConf->PointTable.GetAnalogControlODCIndexUsingMD3Index(Header.GetModuleAddress(), Header.GetChannel(), ODCIndex) ? failed : true;

	uint16_t output = Header.GetOutputFromSecondBlock(CompleteMD3Message[1]);
	bool waitforresult = !MyPointConf->StandAloneOutstation;

	EventTypePayload<EventType::AnalogOutputInt16>::type val;
	val.first = numeric_cast<int16_t>(output);

	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt16, ODCIndex, Name);
	event->SetPayload<EventType::AnalogOutputInt16>(std::move(val));

	if (!failed && (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS))
	{
		SendControlOK(Header);
	}
	else
	{
		SendControlOrScanRejected(Header);
	}
}

#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region SYSTEM
#endif

// Function 40 - SYSTEM_SIGNON_CONTROL
// The response is what we have received with the direction bit changed. If the address is zero, we send our address.
void MD3OutstationPort::DoSystemSignOnControl(MD3BlockFn40MtoS &Header)
{
	// This is used to turn on radios (typically) before a real command is sent.
	// If address is zero, the master is asking us to identify ourselves.
	LOGDEBUG("{} - DoSystemSignOnControl",Name);
	if (Header.IsValid())
	{
		// Need to send a response, but with the StationAddress set to match our address.
		MD3BlockFn40StoM FormattedBlock(MyConf->mAddrConf.OutstationAddr);

		MD3Message_t ResponseMD3Message;
		ResponseMD3Message.push_back(FormattedBlock);
		SendMD3Message(ResponseMD3Message);
	}
	else
	{
		// SIGNON does not send back a rejected message
		//SendControlOrScanRejected(Header);
		LOGERROR("Invalid SYSTEM_SIGNON_CONTROL message, On Station Address - " + std::to_string(Header.GetStationAddress()));
	}
}
// Function 43
void MD3OutstationPort::DoSetDateTime(MD3BlockFn43MtoS& Header, MD3Message_t& CompleteMD3Message)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to  ODC, then set-up a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

	if ((CompleteMD3Message.size() != 2) && (Header.GetStationAddress() != 0))
	{
		SendControlOrScanRejected(Header); // If we did not get two blocks, then send back a command rejected message.
		return;
	}

	MD3BlockData& timedateblock = CompleteMD3Message[1];

	// If date time is within a window of now, accept. Otherwise send command rejected.
	uint64_t msecsinceepoch = static_cast<uint64_t>(timedateblock.GetData()) * 1000 + Header.GetMilliseconds();

	// MD3 only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1
	uint64_t currenttime = MD3NowUTC();

	// So when adjusting SOE times, just add the Offset to the Clock (current time)
	SOETimeOffsetMinutes = static_cast<int>(round(((msecsinceepoch > currenttime ) ? msecsinceepoch - currenttime : currenttime - msecsinceepoch )/(60.0*1000.0)));
	if (msecsinceepoch < currenttime)
		SOETimeOffsetMinutes = -SOETimeOffsetMinutes;

	LOGDEBUG("{} DoSetDateTime Time {}, Current UTC Time {} - SOE Offset Now Set to {} minutes", Name, to_stringfromMD3time(msecsinceepoch), to_stringfromMD3time(currenttime), SOETimeOffsetMinutes);

	if (abs(SOETimeOffsetMinutes) > 24 * 60)
	{
		// If the abs time offset > 24 hours, ignore!
		SendControlOrScanRejected(Header); // If we did not get two blocks, then send back a command rejected message.
		return;
	}

	if (Header.GetStationAddress() != 0)
		SendControlOK(Header);
	SystemFlags.TimePacketReceived();
}
// Function 44
void MD3OutstationPort::DoSetDateTimeNew(MD3BlockFn44MtoS& Header, MD3Message_t& CompleteMD3Message)
{
	// We have three blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to  ODC, then set-up a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

	LOGDEBUG("{} - DoSetdateTimeNew", Name);

	if ((CompleteMD3Message.size() != 3) && (Header.GetStationAddress() != 0))
	{
		SendControlOrScanRejected(Header); // If we did not get three blocks, then send back a command rejected message.
		return;
	}

	MD3BlockData& timedateblock = CompleteMD3Message[1];

	// If date time is within a window of now, accept. Otherwise send command rejected.
	uint64_t msecsinceepoch = static_cast<uint64_t>(timedateblock.GetData()) * 1000 + Header.GetMilliseconds();

	// Not used for now...
	MD3BlockData& utcoffsetblock = CompleteMD3Message[2];
	auto utcoffsetminutes = static_cast<int16_t>(utcoffsetblock.GetFirstWord());

	// MD3 only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1 - Same as for MD3
	uint64_t currenttime = MD3NowUTC();

	// So when adjusting SOE times, just add the Offset to the Clock (current time)
	SOETimeOffsetMinutes = static_cast<int>(round(((msecsinceepoch > currenttime) ? msecsinceepoch - currenttime : currenttime - msecsinceepoch) / (60.0 * 1000.0)));
	if (msecsinceepoch < currenttime)
		SOETimeOffsetMinutes = -SOETimeOffsetMinutes;

	LOGDEBUG("{} DoSetDateTime Time {} UTC, Offset {} min, Current UTC Time {} - SOE Offset Now Set to {} minutes",
		Name, to_stringfromMD3time(msecsinceepoch), utcoffsetminutes, to_stringfromMD3time(currenttime), SOETimeOffsetMinutes);

	if (abs(SOETimeOffsetMinutes) > 24 * 60)
	{
		// If the abs time offset > 24 hours, ignore!
		LOGDEBUG("{} DoSetDateTime Time Offset Greater than 24 hours, ignoring...", Name );
		SendControlOrScanRejected(Header); // If we did not get two blocks, then send back a command rejected message.
		return;
	}

	if (Header.GetStationAddress() != 0)
		SendControlOK(Header);

	SystemFlags.TimePacketReceived();
}
// Function 52
void MD3OutstationPort::DoSystemFlagScan(MD3BlockFn52MtoS &Header, MD3Message_t &CompleteMD3Message)
{
	// Normally will be just one block and the reply will be one block, but can be more than one, and is contract dependent.
	// As far as we can tell AusGrid does not have any extra packets
	//
	// The second 16 bits of the response are the flag bits. A change in any will set the RSF bit in ANY scan/control replies.
	LOGDEBUG("{} - DoSystemFlagScan",Name);

	if (CompleteMD3Message.size() != 1)
	{
		// Handle Flag scan commands with more than one block - this only occurs if the MD3 software has been modified for a contract
		SendControlOrScanRejected(Header); // If we did not get one blocks, then send back a command rejected message - for NOW
		LOGDEBUG("{} - Got a flag scan command with more than one block - not handled at the moment and contract dependent",Name);
		return;
	}

	// The flag scan will NOT be passed through ODC, will just mirror what we know about the outstation
	MD3Message_t ResponseMD3Message;

	// Create the response packet
	MD3BlockFn52StoM RetBlock(Header);

	RetBlock.SetSystemFlags(SystemFlags.GetSystemPoweredUpFlag(), SystemFlags.GetSystemTimeIncorrectFlag());

	ResponseMD3Message.push_back(RetBlock);
	SendMD3Message(ResponseMD3Message);

	SystemFlags.FlagScanPacketSent();
}

// Function 15 Output
void MD3OutstationPort::SendControlOK(MD3BlockFormatted &Header)
{
	// The Control OK block seems to return the originating message first block, but with the function code changed to 15
	MD3Message_t ResponseMD3Message;

	// The MD3BlockFn15StoM does the changes we need for us. The control blocks do not have space for the System flags to be returned...
	MD3BlockFn15StoM FormattedBlock(Header);
	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}
// Function 30 Output
void MD3OutstationPort::SendControlOrScanRejected(MD3BlockFormatted &Header)
{
	// The Control rejected block seems to return the originating message first block, but with the function code changed to 30
	MD3Message_t ResponseMD3Message;

	// The MD3BlockFn30StoM does the changes we need for us.
	MD3BlockFn30StoM FormattedBlock(Header);
	FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}

// UI Handlers
std::pair<std::string, const IUIResponder *> MD3OutstationPort::GetUIResponder()
{
	return std::pair<std::string, const MD3OutstationPortCollection*>("MD3OutstationPortControl", this->MD3OutstationCollection.get());
}

bool MD3OutstationPort::UIFailControl(const std::string& active)
{
	if (iequals(active, "true"))
	{
		FailControlResponse = true;
		LOGCRITICAL("{} The Control Response Packet is set to fail - {}", Name, FailControlResponse);
		return true;
	}
	if (iequals(active, "false"))
	{
		FailControlResponse = false;
		LOGCRITICAL("{} The Control Response Packet is set to fail - {}", Name, FailControlResponse);
		return true;
	}
	return false;
}
bool MD3OutstationPort::UIRandomReponseBitFlips(const std::string& probability)
{
	try
	{
		auto prob = std::stod(probability);
		if ((prob > 1.0) || (prob < 0.0))
			return false;
		BitFlipProbability = prob;
		LOGCRITICAL("{} Set the probability of a flipped bit in the response packet to {}", Name, BitFlipProbability);
		return true;
	}
	catch (...)
	{}
	return false;
}
bool MD3OutstationPort::UIRandomReponseDrop(const std::string& probability)
{
	try
	{
		auto prob = std::stod(probability);
		if ((prob > 1.0) || (prob < 0.0))
			return false;
		ResponseDropProbability = prob;
		LOGCRITICAL("{} Set the probability of a dropped reponseset to {}", Name, ResponseDropProbability);
		return true;
	}
	catch (...)
	{}
	return false;
}
#ifdef _MSC_VER
#pragma endregion
#endif
