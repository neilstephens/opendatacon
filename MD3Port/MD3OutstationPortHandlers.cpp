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
#include <iostream>
#include <future>
#include <regex>
#include <chrono>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include "MD3Engine.h"
#include "MD3OutstationPort.h"
#include "MD3.h"

#include <opendnp3/LogLevels.h>

//TODO: Check out http://www.pantheios.org/ logging library..

/*func, count, name Analysis of traffic results

11 , 138125            Digital COS Time Tagged - Done
12 , 45678              Digital Unconditional - Done
13 , 3710363          Analog No Change Reply - Done
14 , 32040              Digital No Change Reply - Done
15 , 20165              Control Request OK - Done
16 , 2                       Freeze And Reset - Done
17 , 53                    POM Type Control - Done
19 , 2                      DOM Type Control - Done - Cant find example in the data
21 , 1                      Raise Lower Type Control - This is reserved for future use...
23 , 1                       AOM Type Control - No valid example.. yet
30 , 135                  Control or Scan Request Rejected - Done
31 , 1                      Counter Scan - Done - checksum passes
40 , 1                       System SIGNON Control  - Done - but failed checksum so probably not used
43 , 23274              System SET DATE AND TIME Control - Done
5 , 380836              Analog Unconditional - Done
52 , 6120                System Flag Scan - Done
6 , 7041839            Analog Delta Scan - Done
7 , 14                      Obsolete Digital Unconditional - Done
8 , 1                         Digital Delta Scan - Done
9 , 10                       HRER List Scan - Done
*/

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
FILE_DOWNLOAD = 50,
FILE_UPLOAD = 51,
SYSTEM_FLAG_SCAN = 52,
LOW_RES_EVENTS_LIST_SCAN = 60

*/

void MD3OutstationPort::ProcessMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteMD3Message.size() != 0);

	uint8_t ExpectedStationAddress = MyConf()->mAddrConf.OutstationAddr;
	int ExpectedMessageSize = 1;	// Only set in switch statement if not 1

	MD3BlockFormatted Header = CompleteMD3Message[0];
	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.

	//TODO: SJE Check that the flag to master in the message is not set - ie. is a message from a master!

	// All are included to allow better error reporting.
	switch (Header.GetFunctionCode())
	{
	case ANALOG_UNCONDITIONAL:	// Command and reply
		DoAnalogUnconditional(Header);
		break;
	case ANALOG_DELTA_SCAN:		// Command and reply
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
		// Master Only
		break;
	case DIGITAL_NO_CHANGE_REPLY:
		// Master Only
		break;
	case CONTROL_REQUEST_OK:
		// Master Only
		break;
	case FREEZE_AND_RESET:
		DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
		break;
	case POM_TYPE_CONTROL :
		ExpectedMessageSize = 2;
		DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
		break;
	case DOM_TYPE_CONTROL:
		ExpectedMessageSize = 2;
		DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
		break;
	case INPUT_POINT_CONTROL:
		break;
	case RAISE_LOWER_TYPE_CONTROL :
		break;
	case AOM_TYPE_CONTROL :
		ExpectedMessageSize = 2;
		DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
		break;
	case CONTROL_OR_SCAN_REQUEST_REJECTED:
		// Master Only
		break;
	case COUNTER_SCAN :
		DoCounterScan(Header);
		break;
	case SYSTEM_SIGNON_CONTROL:
		DoSystemSignOnControl(static_cast<MD3BlockFn40&>(Header));
		break;
	case SYSTEM_SIGNOFF_CONTROL:
		break;
	case SYSTEM_RESTART_CONTROL:
		break;
	case SYSTEM_SET_DATETIME_CONTROL:
		ExpectedMessageSize = 2;
		DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
		break;
	case FILE_DOWNLOAD:
		break;
	case FILE_UPLOAD :
		break;
	case SYSTEM_FLAG_SCAN:
		ExpectedMessageSize = CompleteMD3Message.size();	// Variable size
		DoSystemFlagScan(Header, CompleteMD3Message);
		break;
	case LOW_RES_EVENTS_LIST_SCAN :
		break;
	default:
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", "Unknown Command Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		break;
	}

	if (ExpectedMessageSize != CompleteMD3Message.size())
	{
			LOG("MD3OutstationPort", openpal::logflags::ERR, "", "Unexpected Message Size - " + std::to_string(CompleteMD3Message.size()) +
				" On Station Address - " + std::to_string(Header.GetStationAddress())+
				" Function - " + std::to_string(Header.GetFunctionCode()));
	}
}
#pragma region ANALOG and COUNTER
// Function 5
void MD3OutstationPort::DoAnalogUnconditional(MD3BlockFormatted &Header)
{
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
	SendAnalogOrCounterUnconditional(ANALOG_UNCONDITIONAL,AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
}

// Function 31 - Esentially the same as Analog Unconditional. Either can be used to return either analog or counter, or both.
// The only difference is that an analog module seems to have 16 channels, the counter module only 8.
// The expectation is that if you ask for more than 8 from a counter module, it will roll over to the next module (counter or analog).
// As an analog module has 16, the most that can be requested the overflow never happens.
// In order to make this work, we need to know if the module we are dealing with is a counter or analog module.
void MD3OutstationPort::DoCounterScan(MD3BlockFormatted &Header)
{
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

void MD3OutstationPort::ReadAnalogOrCounterRange(int ModuleAddress, int Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues)
{
	// The Analog and Counters are  maintained in two lists, we need to deal with both of them as they both can be read by this method.
	// So if we find an entry in the analog list, we dont have to worry about overflow, as there are 16 channels, and the most we can ask for is 16.
	//
	uint16_t wordres = 0;
	int deltares = 0;

	// Is it a counter module? Fails if not at this address
	if (GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres))
	{
		// We have a counter module, can get up to 8 values from it
		int chancnt = Channels >= 8 ? 8 : Channels;
		GetAnalogModuleValues(CounterModule,chancnt, ModuleAddress, ResponseType, AnalogValues, AnalogDeltaValues);

		if (Channels > 8)
		{
			// Now we have to get the remaining channels (up to 8) from the next module, if it exists.
			// Check if it is a counter, otherwise assume analog. Will return correct error codes if not there
			if (GetCounterValueUsingMD3Index(ModuleAddress+1, 0, wordres))
			{
				GetAnalogModuleValues(CounterModule, Channels - 8, ModuleAddress + 1, ResponseType, AnalogValues, AnalogDeltaValues);
			}
			else
			{
				GetAnalogModuleValues(AnalogModule, Channels - 8, ModuleAddress + 1, ResponseType, AnalogValues, AnalogDeltaValues);
			}
		}
	}
	else
	{
		// It must be an analog module (if it does not exist, we return appropriate error codes anyway
		// Yes proceed , up to 16 channels.
		GetAnalogModuleValues(AnalogModule, Channels, ModuleAddress, ResponseType, AnalogValues, AnalogDeltaValues);
	}
}
void MD3OutstationPort::GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, int Channels, int ModuleAddress, MD3OutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t> & AnalogValues, std::vector<int> & AnalogDeltaValues)
{
	for (int i = 0; i < Channels; i++)
	{
		uint16_t wordres = 0;
		int deltares = 0;
		bool foundentry = GetAnalogValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares);

		if (IsCounterOrAnalog == CounterModule)
		{
			foundentry = GetCounterValueAndChangeUsingMD3Index(ModuleAddress, i, wordres, deltares);
		}

		if (!foundentry)
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
void MD3OutstationPort::SendAnalogOrCounterUnconditional(MD3_FUNCTION_CODE functioncode, std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3BlockData> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockData FormattedBlock = MD3BlockFormatted(StationAddress, false, functioncode, ModuleAddress, Channels);
	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Analogs.size());

	int NumberOfDataBlocks = Channels / 2 + Channels % 2;	// 2 --> 1, 3 -->2

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = MD3BlockData(Analogs[2 * i], Analogs[2 * i + 1], lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendMD3Message(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3BlockData> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockData FormattedBlock = MD3BlockFormatted(StationAddress, false, ANALOG_DELTA_SCAN, ModuleAddress, Channels);
	ResponseMD3Message.push_back(FormattedBlock);

	assert(Channels == Deltas.size());

	// Can be 4 channel delta values to a block.
	int NumberOfDataBlocks = Channels / 4 + (Channels % 4 == 0 ? 0 : 1);

	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = MD3BlockData((char)Deltas[i * 4], (char)Deltas[i * 4 + 1], (char)Deltas[i * 4 + 2], (char)Deltas[i * 4 + 3], lastblock);
		ResponseMD3Message.push_back(block);
	}
	SendMD3Message(ResponseMD3Message);
}
void MD3OutstationPort::SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels)
{
	std::vector<MD3BlockData> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockData FormattedBlock = MD3BlockFormatted(StationAddress, false, ANALOG_NO_CHANGE_REPLY, ModuleAddress, Channels, true);
	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}

#pragma endregion

#pragma region DIGITAL
// Function 7
void MD3OutstationPort::DoDigitalUnconditionalObs(MD3BlockFormatted &Header)
{
	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	std::vector<MD3BlockData> ResponseMD3Message;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	MD3BlockFormatted FormattedBlock = MD3BlockFormatted(Header.GetStationAddress(), false, DIGITAL_UNCONDITIONAL_OBS, Header.GetModuleAddress(), Header.GetChannels());
	ResponseMD3Message.push_back(FormattedBlock);

	int NumberOfDataBlocks = Header.GetChannels(); // Actually the number of modules

	BuildBinaryReturnBlocks(NumberOfDataBlocks, Header.GetModuleAddress(), Header.GetStationAddress(), true, ResponseMD3Message);
	SendMD3Message(ResponseMD3Message);
}
// Function 8
void MD3OutstationPort::DoDigitalChangeOnly(MD3BlockFormatted &Header)
{
	// Have three possible replies, Digital Unconditional #7,  Delta Scan #8 and Digital No Change #14
	// So the data is deemed to have changed if a bit has changed, or the status has changed.
	// We detect changes on a module basis, and send the data in the same format as the Digital Unconditional.
	// So the Delta Scan can be all the same data as the uncondtional.
	// If no changes, send the single #14 function block. Keep the module and channel values matching the orginal message.

	// For this function, the channels field is actually the number of consecutive modules to return. We always return 16 channel bits.
	// If there is an invalid module, we return a different block for that module.
	std::vector<MD3BlockData> ResponseMD3Message;

	bool NoChange = true;
	bool SomeChange = false;
	int NumberOfDataBlocks = Header.GetChannels(); // Actually the number of modules - 0 numbered, does not make sense to ask for none...

	int ChangedBlocks = CountBinaryBlocksWithChangesGivenRange(NumberOfDataBlocks, Header.GetModuleAddress());

	if (ChangedBlocks == 0)	// No change
	{
		MD3BlockData FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
		ResponseMD3Message.push_back(FormattedBlock);

		SendMD3Message(ResponseMD3Message);
	}
	else if (ChangedBlocks != NumberOfDataBlocks)	// Some change
	{
		//TODO: What are the module and channel set to in Function 8 digital change response packets - packet count can vary..
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
void MD3OutstationPort::DoDigitalHRER(MD3BlockFn9 &Header, std::vector<MD3BlockData> &CompleteMD3Message)
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

	std::vector<MD3BlockData> ResponseMD3Message;


	if (Header.GetSequenceNumber() == LastHRERSequenceNumber)
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

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
	int EventCount = 0;

	if (Header.GetEventCount() == 0)
	{
		// There should be a second packet, and this is actually a set time/date command
		SendControlOrScanRejected(Header);	// We are not supporting this message so send command rejected message.

		// We just log that we got it at the moment
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", "Received a Fn 9 Set Time/Date Command - not handled. Station Address - " + std::to_string(Header.GetStationAddress()));
		//TODO: Pass through FN9 zero maxiumumevents used to set time in outstation.
		return;
	}
	else
	{
		std::vector<uint16_t> ResponseWords;

		// Handle a normal packet - each event can be a word( 16 bits) plus time and other words
		Fn9AddTimeTaggedDataToResponseWords(Header.GetEventCount(), EventCount, ResponseWords);

		//TODO: Fn9 Add any Internal HRER events unrelated to the digital bits. EventBufferOverflow (1) is the only one of interest. Use Zero Module address and 1 in the channel field. The time is when this occurred

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

	MD3BlockFn9 &firstblock = static_cast<MD3BlockFn9&>(ResponseMD3Message[0]);
	firstblock.SetEventCountandMoreEventsFlag(EventCount, (MyPointConf()->BinaryTimeTaggedEventQueue.read_available() > 0));	// If not empty, set more events (MEV) flag

	LastDigitialHRERResponseMD3Message = ResponseMD3Message;	// Copy so we can resend if necessary
	SendMD3Message(ResponseMD3Message);
}

void MD3OutstationPort::Fn9AddTimeTaggedDataToResponseWords( int MaxEventCount, int &EventCount, std::vector<uint16_t> &ResponseWords)
{
	MD3BinaryPoint CurrentPoint;
	uint64_t LastPointmsec = 0;
	bool CanSend = true;

	while ((MyPointConf()->BinaryTimeTaggedEventQueue.read_available() > 0) && (EventCount < MaxEventCount) && CanSend)
	{
		CurrentPoint = MyPointConf()->BinaryTimeTaggedEventQueue.front();

		if (EventCount == 0)
		{
			// First packet is the time/date block and a millseconds packet
			uint32_t FirstEventSeconds = (uint32_t)(CurrentPoint.ChangedTime / 1000);
			uint16_t FirstEventMSec = (uint16_t)(CurrentPoint.ChangedTime % 1000);
			ResponseWords.push_back(FirstEventSeconds >> 16);
			ResponseWords.push_back(FirstEventSeconds & 0x0FFFF);
			ResponseWords.push_back(MD3BlockFn9::MilliSecondsPacket(FirstEventMSec));
			LastPointmsec = CurrentPoint.ChangedTime;
		}
		else
		{
			// Do we need a Milliseconds packet? If so, is the gap more than 31 seconds? If so, finish this response off, so master can issue a new HRER request.
			uint64_t delta = CurrentPoint.ChangedTime - LastPointmsec;
			if (delta > 31999)
			{
				//Delta > 31.999 seconds, We can't send this point, finish the packet so the master can ask for a new HRER response.
				CanSend = false;
			}
			else if (delta != 0)
			{
				ResponseWords.push_back(MD3BlockFn9::MilliSecondsPacket((uint16_t)delta));
				LastPointmsec = CurrentPoint.ChangedTime;						// The last point time moves with time added by the msec packets
			}
		}

		if (CanSend)
		{
			ResponseWords.push_back(MD3BlockFn9::HREREventPacket(CurrentPoint.Binary, CurrentPoint.Channel, CurrentPoint.ModuleAddress));

			MyPointConf()->BinaryTimeTaggedEventQueue.pop();
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

	std::vector<MD3BlockData> ResponseMD3Message;

	bool NoChange = true;
	bool SomeChange = false;
	int NumberOfDataBlocks = Header.GetModuleCount();

	bool ChangedBlocks = (CountBinaryBlocksWithChanges() != 0);

	if (ChangedBlocks == false)	// No change
	{
		MD3BlockData FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetModuleAddress(), (uint8_t)0);
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

		MD3BlockFn10 &firstblock = (MD3BlockFn10 &)ResponseMD3Message[0];
		firstblock.SetModuleCount((uint8_t)ResponseMD3Message.size() - 1);	// The number of blocks taking away the header...

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

	std::vector<MD3BlockData> ResponseMD3Message;

	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

		SendMD3Message(LastDigitialScanResponseMD3Message);
		return;
	}

	if (Header.GetDigitalSequenceNumber() == 0)
	{
		// Special case, mark every module with data changed, then send the first group.
		// There may be more to send, the master will get those with another command, with a normal sequence number.
		MarkAllBinaryBlocksAsChanged();
	}

	int ChangedBlocks = CountBinaryBlocksWithChanges();
	bool AreThereTaggedEvents = (MyPointConf()->BinaryModuleTimeTaggedEventQueue.read_available() > 0);

	if ((ChangedBlocks == 0) && !AreThereTaggedEvents)
	{
		//TODO: Digital No Change - check assumption that the second word needs to mirror that sent in the function 11 command (tagged event count, digital sequence # and ModuleCount
		MD3BlockFormatted FormattedBlock = MD3BlockFn14StoM(Header.GetStationAddress(), Header.GetDigitalSequenceNumber());
		ResponseMD3Message.push_back(FormattedBlock);
	}
	else
	{
		// We have data to send.
		int TaggedEventCount = 0;
		int ModuleCount = Limit(ChangedBlocks, Header.GetModuleCount());

		// Setup the response block
		MD3BlockFn11StoM FormattedBlock(Header.GetStationAddress(), TaggedEventCount, Header.GetDigitalSequenceNumber(), ModuleCount);
		ResponseMD3Message.push_back(FormattedBlock);

		// Add non-time tagged data. We can return a data block or a status block for each module.
		// If the module is active (not faulted) return the data, otherwise return the status block..
		// We always just start with the first module and move up through changed blocks until we reach the ChangedBlocks count
		//TODO: For remotes with latching digital changes - we can reply with two data blocks, the first being the latched change, the second being the current data - not sure how this works...

		std::vector<uint8_t> ModuleList;
		BuildListOfModuleAddressesWithChanges(0, ModuleList);

		BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseMD3Message);

		FormattedBlock.SetModuleCount((uint8_t)ResponseMD3Message.size() - 1);	// The number of blocks taking away the header...

		if (AreThereTaggedEvents)
		{
			// Add timetagged data
			std::vector<uint16_t> ResponseWords;

			Fn11AddTimeTaggedDataToResponseWords(Header.GetTaggedEventCount(), TaggedEventCount, ResponseWords);

			// Convert data to 32 bit blocks and pad if necessary.
			if ((ResponseWords.size() % 2) != 0)
			{
				// Add a filler packet if necessary
				ResponseWords.push_back(MD3BlockFn11StoM::FillerPacket());
			}

			//TODO: A flag block can appear in the Fn11 time tagged response - which can indicate Internal Buffer Overflow, Time sync fail and restoration.

			// Now translate the 16 bit packets into the 32 bit MD3 blocks.
			for (uint16_t i = 0; i < ResponseWords.size(); i = i + 2)
			{
				MD3BlockData blk(ResponseWords[i], ResponseWords[i + 1]);
				ResponseMD3Message.push_back(blk);
			}
		}

		FormattedBlock.SetTaggedEventCount(TaggedEventCount);		// Update to the number we are sending

		// Mark the last block
		if (ResponseMD3Message.size() != 0)
		{
			MD3BlockData &lastblock = ResponseMD3Message.back();
			lastblock.MarkAsEndOfMessageBlock();
		}
	}
	SendMD3Message(ResponseMD3Message);

	// Store this set of packets in case we have to resend
	//TODO: Is the sequence number function dependent - i.e. do we maintain one for each digital function or is it common across all digital functions.
	LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
	LastDigitialScanResponseMD3Message = ResponseMD3Message;
}

void MD3OutstationPort::Fn11AddTimeTaggedDataToResponseWords(int MaxEventCount, int &EventCount, std::vector<uint16_t> &ResponseWords)
{
	MD3BinaryPoint CurrentPoint;
	uint64_t LastPointmsec = 0;

	while ((MyPointConf()->BinaryModuleTimeTaggedEventQueue.read_available() > 0) && (EventCount < MaxEventCount))
	{
		CurrentPoint = MyPointConf()->BinaryModuleTimeTaggedEventQueue.front();

		// The time date is seconds, the data block contains a msec entry.
		// The time is accumulated from each event in the queue. If we have greater than 255 msec between events,
		// add a time packet to bridge the gap.

		if (EventCount == 0)
		{
			// First packet is the time/date block
			uint32_t FirstEventSeconds = (uint32_t)(CurrentPoint.ChangedTime / 1000);
			ResponseWords.push_back(FirstEventSeconds >> 16);
			ResponseWords.push_back(FirstEventSeconds & 0x0FFFF);
			LastPointmsec = CurrentPoint.ChangedTime - CurrentPoint.ChangedTime % 1000;	// The first one is seconds only. Later events have actual msec
		}

		uint64_t msecoffset = CurrentPoint.ChangedTime - LastPointmsec;

		if (msecoffset > 255)	// Do we need a Milliseconds extension packet? A TimeBlock
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
			ResponseWords.push_back((uint16_t)msecoffsetdiv256);
			LastPointmsec += msecoffsetdiv256*256;	// The last point time moves with time added by the msec packet
			msecoffset = CurrentPoint.ChangedTime - LastPointmsec;
		}

		// Push the block onto the response word list
		assert(msecoffset < 256);
		ResponseWords.push_back((uint16_t)CurrentPoint.ModuleAddress << 8 | (uint16_t)msecoffset);
		ResponseWords.push_back((uint16_t)CurrentPoint.ModuleBinarySnapShot);

		LastPointmsec = CurrentPoint.ChangedTime;	// Update the last changed time to match what we have just sent.
		MyPointConf()->BinaryModuleTimeTaggedEventQueue.pop();
		EventCount++;
	}
}
// Function 12
void MD3OutstationPort::DoDigitalUnconditional(MD3BlockFn12MtoS &Header)
{
	std::vector<MD3BlockData> ResponseMD3Message;

	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
	{
		// The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
		// If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
		// It would get very tricky to only commit change written information only when a new sequence number turns up.
		// The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

		SendMD3Message(LastDigitialScanResponseMD3Message);
		return;
	}

	// The reply is the same format as for Fn11, but without time tagged data. Is the function code returned 11 or 12?
	std::vector<uint8_t> ModuleList;
	BuildListOfModuleAddressesWithChanges(Header.GetModuleCount(),Header.GetModuleAddress(), true, ModuleList);

	// Setup the response block - module count to be updated
	MD3BlockFn11StoM FormattedBlock(Header.GetStationAddress(), 0, Header.GetDigitalSequenceNumber(), Header.GetModuleCount());
	ResponseMD3Message.push_back(FormattedBlock);

	BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseMD3Message);

	FormattedBlock.SetModuleCount((uint8_t)ResponseMD3Message.size() - 1);	// The number of blocks taking away the header...

	// Mark the last block
	if (ResponseMD3Message.size() != 0)
	{
		MD3BlockData &lastblock = ResponseMD3Message.back();
		lastblock.MarkAsEndOfMessageBlock();
	}

	SendMD3Message(ResponseMD3Message);

	// Store this set of packets in case we have to resend
	//TODO: Is the sequence number function dependent - i.e. do we maintain one for each digital function or is it common across all digital functions.
	LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
	LastDigitialScanResponseMD3Message = ResponseMD3Message;
}

// This will be called when we get a zero sequence number for Fn 11 or 12. It is sent on Master startup to ensure that all data is sent in following
// change only commands - if there are sufficient modules
//TODO: Have to think about how zero sequence number is passed through ODC to our master talking to the real slave
void MD3OutstationPort::MarkAllBinaryBlocksAsChanged()
{
	// The map is sorted, so when iterating, we are working to a specific order. We can have up to 16 points in a block only one changing will trigger a send.
	for (auto md3pt : MyPointConf()->BinaryMD3PointMap)
	{
		(*md3pt.second).Changed = true;
	}
}

uint16_t MD3OutstationPort::CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool &ModuleFailed)
{
	uint16_t wordres = 0;

	for (int j = 0; j < 16; j++)
	{
		uint8_t bitres = 0;
		bool changed = false;	// We dont care about the returned value

		if (GetBinaryValueUsingMD3Index(ModuleAddress, j, bitres, changed))	// Reading this clears the changed bit
		{
			//TODO: Check the bit order here of the binaries
			wordres |= (uint16_t)bitres << (15 - j);
		}
		//TODO: Check and update the module failed status for this module.
	}
	return wordres;
}
uint16_t MD3OutstationPort::CollectModuleBitsIntoWord(const uint8_t ModuleAddress, bool &ModuleFailed)
{
	uint16_t wordres = 0;

	for (int j = 0; j < 16; j++)
	{
		uint8_t bitres = 0;
		bool changed = false;	// We dont care about the returned value

		if (GetBinaryValueUsingMD3Index(ModuleAddress, j, bitres))	// Reading this clears the changed bit
		{
			//TODO: Check the bit order here of the binaries
			wordres |= (uint16_t)bitres << (15 - j);
		}
		//TODO: Check and update the module failed status for this module.
	}
	return wordres;
}
// Scan all binary/digital blocks for changes - used to determine what response we need to send
// We return the total number of changed blocks we assume every block supports time tagging
// If SendEverything is true,
int MD3OutstationPort::CountBinaryBlocksWithChanges()
{
	int changedblocks = 0;
	int lastblock = -1;	// Non valid value

	// The map is sorted, so when iterating, we are working to a specific order. We can have up to 16 points in a block only one changing will trigger a send.
	for (auto md3pt : MyPointConf()->BinaryMD3PointMap)
	{
		if ((*md3pt.second).Changed)
		{
			// Multiple bits can be changed in the block, but only the first one is required to trigger a send of the block.
			if (lastblock != (*md3pt.second).ModuleAddress)
			{
				lastblock = (*md3pt.second).ModuleAddress;
				changedblocks++;
			}
		}
	}
	return changedblocks;
}
// This is used to determine which response we should send NoChange, DeltaChange or AllChange
int MD3OutstationPort::CountBinaryBlocksWithChangesGivenRange(int NumberOfDataBlocks, int StartModuleAddress)
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

// Used in Fn7, Fn8 and Fn12
void MD3OutstationPort::BuildListOfModuleAddressesWithChanges(int NumberOfDataBlocks, int StartModuleAddress, bool forcesend, std::vector<uint8_t> &ModuleList)
{
	// We want a list of modules to send...
	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		bool datachanged = false;

		for (int j = 0; j < 16; j++)
		{
			uint8_t bitres = 0;
			bool changed = false;

			if (!GetBinaryChangedUsingMD3Index(StartModuleAddress + i, j, changed))
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
void MD3OutstationPort::BuildListOfModuleAddressesWithChanges(int StartModuleAddress, std::vector<uint8_t> &ModuleList)
{
	uint8_t LastModuleAddress = 0;
	bool WeAreScanning = (StartModuleAddress == 0);	// If the startmoduleaddress is zero, we store changes from the start.

	for (auto MapPt : MyPointConf()->BinaryMD3PointMap)
	{
		auto MDPt = *(MapPt.second);
		auto Address = (MapPt.first);
		uint8_t ModuleAddress = (Address >> 8);
		uint8_t Channel = (Address & 0x0FF);

		if (ModuleAddress == StartModuleAddress)
			WeAreScanning = true;

		if (WeAreScanning && MDPt.Changed && (LastModuleAddress != ModuleAddress))
		{
			// We should save the module address so we have a list of modules that have changed
			ModuleList.push_back(ModuleAddress);
			LastModuleAddress = ModuleAddress;
		}
	}
	if (StartModuleAddress != 0)
	{
		// We have to then start again from zero, if we did not start from there.
		for (auto MapPt : MyPointConf()->BinaryMD3PointMap)
		{
			auto MDPt = *(MapPt.second);
			auto Address = (MapPt.first);
			uint8_t ModuleAddress = (Address >> 8);
			uint8_t Channel = (Address & 0x0FF);

			if (ModuleAddress == StartModuleAddress)
				WeAreScanning = false;	// Stop when we reach the start again

			if (WeAreScanning && MDPt.Changed && (LastModuleAddress != ModuleAddress))
			{
				// We should save the module address so we have a list of modules that have changed
				ModuleList.push_back(ModuleAddress);
				LastModuleAddress = ModuleAddress;
			}
		}
	}
}
// Fn 7,8
void MD3OutstationPort::BuildBinaryReturnBlocks(int NumberOfDataBlocks, int StartModuleAddress, int StationAddress, bool forcesend, std::vector<MD3BlockData> &ResponseMD3Message)
{
	std::vector<uint8_t> ModuleList;
	BuildListOfModuleAddressesWithChanges(NumberOfDataBlocks, StartModuleAddress, forcesend, ModuleList);
	// We have a list of modules to send...
	BuildScanReturnBlocksFromList(ModuleList, NumberOfDataBlocks, StationAddress,false, ResponseMD3Message);
}

// Fn 7, 8 and 10
void MD3OutstationPort::BuildScanReturnBlocksFromList(std::vector<unsigned char> &ModuleList, int MaxNumberOfDataBlocks, int StationAddress, bool FormatForFn11and12, std::vector<MD3BlockData> & ResponseMD3Message)
{
	// For each module address, or the max we can send
	for (int i = 0; (i < (int)ModuleList.size()) && (i < MaxNumberOfDataBlocks); i++)
	{
		uint8_t ModuleAddress = ModuleList[i];

		// Have to collect all the bits into a uint16_t
		bool ModuleFailed = false;

		uint16_t wordres = CollectModuleBitsIntoWordandResetChangeFlags(ModuleAddress, ModuleFailed);

		if (ModuleFailed)
		{
			if (FormatForFn11and12)
			{
				//TODO: Module failed response for Fn11/12 BuildScanReturnBlocksFromList
			}
			else
			{
				// Queue the error block - Fn 7, 8 and 10 format
				uint8_t errorflags = 0;		//TODO: Application dependent, depends on the outstation implementation/master expectations. We could build in functionality here
				uint16_t lowword = (uint16_t)errorflags << 8 | (ModuleAddress);
				auto block = MD3BlockData((uint16_t)StationAddress << 8, lowword, false);
				ResponseMD3Message.push_back(block);
			}
		}
		else
		{
			if (FormatForFn11and12)
			{
				// For Fn11 and 12 the data format is:
				uint16_t address = (uint16_t)ModuleAddress << 8;	// Low byte is msec offset - which is 0 for non time tagged data
				auto block = MD3BlockData(address, wordres, false);
				ResponseMD3Message.push_back(block);
			}
			else
			{
				// Queue the data block Fn 7,8 and 10
				uint16_t address = (uint16_t)StationAddress << 8 | (ModuleAddress);
				auto block = MD3BlockData(address, wordres, false);
				ResponseMD3Message.push_back(block);
			}
		}
	}
	// Not sure which is the last block for the send only changes..so just mark it when we get to here.
	// Format Fn 11 and 12 may not be the end of packet
	if ((ResponseMD3Message.size() != 0) && !FormatForFn11and12)
	{
		MD3BlockData &lastblock = ResponseMD3Message.back();
		lastblock.MarkAsEndOfMessageBlock();
	}
}


#pragma endregion

#pragma region CONTROL

void MD3OutstationPort::DoFreezeResetCounters(MD3BlockFn16MtoS &Header)
{
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then setup a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

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

		//TODO: SJE Send the Freeze-Reset Counter command through ODC and wait for a response. For the moment we are just sinking the command as if we were an outstation
		SendControlOK(Header);
	}
	else
	{
		if (!failed)
		{
			//TODO: SJE Send the Freeze-Reset Counter command through ODC but dont wait for a response. There will be none.
			// No response
		}
	}
}

void MD3OutstationPort::DoPOMControl(MD3BlockFn17MtoS &Header, std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We have two blocks incomming, not just one.
	// Seems we dont have any POM control signals greater than 7 in the data I have seen??
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then setup a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

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

	// Check that the control point is defined, otherwise return a fail. The value 0 to 15 is a trip or close for points 0 to 7 (hence mod 8?)
	bool foundentry = CheckBinaryControlExistsUsingMD3Index(Header.GetModuleAddress(), Header.GetOutputSelection() % 8);
	if (!foundentry)
	{
		// Control point does not exist...
		failed = true;
	}

	if (Header.GetStationAddress() != 0)
	{
		if (failed)
		{
			SendControlOrScanRejected(Header);
			return;
		}

		//TODO: SJE Send the POM Control command through ODC and wait for a response. For the moment we are just sinking the command as if we were an outstation
		SendControlOK(Header);
	}
	else
	{
		if (!failed)
		{
			//TODO: SJE Send the POM Control command through ODC but dont wait for a response. There will be none.
			// No response
		}
	}
}

void MD3OutstationPort::DoDOMControl(MD3BlockFn19MtoS &Header, std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We have two blocks incomming, not just one.
	// Seems we dont have any POM control signals greater than 7 in the data I have seen??
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then setup a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

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

	// Check that the control point is defined, otherwise return a fail. The value 0 to 15 is a trip or close for points 0 to 7 (hence mod 8?)
	// Check that the first one exists, not all 16 may exist.
	bool foundentry = CheckBinaryControlExistsUsingMD3Index(Header.GetModuleAddress(), 0);
	if (!foundentry)
	{
		// Control point does not exist...
		failed = true;
	}

	if (Header.GetStationAddress() != 0)
	{
		if (failed)
		{
			SendControlOrScanRejected(Header);
			return;
		}

		//TODO: SJE Send the POM Control command through ODC and wait for a response. For the moment we are just sinking the command as if we were an outstation
		SendControlOK(Header);
	}
	else
	{
		if (!failed)
		{
			//TODO: SJE Send the POM Control command through ODC but dont wait for a response. There will be none.
			// No response
		}
	}
}

void MD3OutstationPort::DoAOMControl(MD3BlockFn23MtoS &Header, std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We have two blocks incomming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then setup a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

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

	// Check that the control point is defined, otherwise return a fail. The value 0 to 15 is a trip or close for points 0 to 7 (hence mod 8?)
	//TODO: SJE AOM Control needs analog points to control
/*	bool foundentry = CheckBinaryControlExistsUsingMD3Index(Header.GetModuleAddress(), Header.GetChannel());
	if (!foundentry)
	{
		// Control point does not exist...
		failed = true;
	}
*/
	if (Header.GetStationAddress() != 0)
	{
		if (failed)
		{
			SendControlOrScanRejected(Header);
			return;
		}

		//TODO: SJE Send the POM Control command through ODC and wait for a response. For the moment we are just sinking the command as if we were an outstation
		SendControlOK(Header);
	}
	else
	{
		if (!failed)
		{
			//TODO: SJE Send the POM Control command through ODC but dont wait for a response. There will be none.
			// No response
		}
	}
}

#pragma endregion

#pragma region SYSTEM

// Function 40 - SYSTEM_SIGNON_CONTROL
void MD3OutstationPort::DoSystemSignOnControl(MD3BlockFn40 &Header)
{
	// This is used to turn on radios (typically) before a real command is sent.
	// If address is zero, the master is asking us to identify ourselves.

	if (Header.IsValid())
	{
		if (Header.GetStationAddress() == 0)
		{
			// Need to send a response by echoing the Header, but with the StationAddress set to match our address.
			MD3BlockFn40 FormattedBlock(MyConf()->mAddrConf.OutstationAddr, false);

			std::vector<MD3BlockData> ResponseMD3Message;
			ResponseMD3Message.push_back(FormattedBlock);
			SendMD3Message(ResponseMD3Message);
		}

		//TODO: SJE Send the systemsignoncontrol command through ODC and wait for a response.
	}
	else
	{
		LOG("MD3OutstationPort", openpal::logflags::ERR, "", "Invalid SYSTEM_SIGNON_CONTROL message, On Station Address - " + std::to_string(Header.GetStationAddress()) );
	}
}
// Function 43
void MD3OutstationPort::DoSetDateTime(MD3BlockFn43MtoS &Header, std::vector<MD3BlockData> &CompleteMD3Message)
{
	// We have two blocks incomming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to  ODC, then setup a lambda to handle the sending of the response - when we get it.

	// Two possible responses, they depend on the future result - of type CommandStatus.
	// SUCCESS = 0 /// command was accepted, initiated, or queue
	// TIMEOUT = 1 /// command timed out before completing
	// BLOCKED_OTHER_MASTER = 17 /// command not accepted because the outstation is forwarding the request to another downstream device which cannot be reached
	// Really comes down to success - which we want to be we have an answer - not that the request was queued..
	// Non-zero will be a fail.

	if (Header.GetStationAddress() != 0)
	{
		if (CompleteMD3Message.size() != 2)
		{
			SendControlOrScanRejected(Header);	// If we did not get two blocks, then send back a command rejected message.
			return;
		}

		MD3BlockData &timedateblock = CompleteMD3Message[1];

		// If date time is within a window of now, accept. Otherwise send command rejected.
		uint64_t msecsinceepoch = (uint64_t)timedateblock.GetData() * 1000 + Header.GetMilliseconds();

		// MD3 only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1 - Same as for MD3
		uint64_t currenttime = asiopal::UTCTimeSource::Instance().Now().msSinceEpoch;

		if (abs((int64_t)msecsinceepoch - (int64_t)currenttime) > 30000)	// Set window as +-30 seconds
		{
			SendControlOrScanRejected(Header);
		}
		else
		{
			//TODO: SJE Send the timechange command through ODC and wait for a response. For the moment we are just sinking the command as if we were an outstation
			SendControlOK(Header);
		}
	}
	else
	{
		//TODO: SJE Send the timechange command through ODC dont wait for a response. There will be none.
		// No response
	}
}

// Function 52
void MD3OutstationPort::DoSystemFlagScan(MD3BlockFormatted &Header, std::vector<MD3BlockData> &CompleteMD3Message)
{
	// Normally will be just one block and the reply will be one block, but can be more than one, and is contract dependent.
	//
	// As far as we can tell AusGrid does not have any extra packets
	//
	// The second 16 bits of the response are the flag bits. A change in any will set the RSF bit in ANY scan/control replies.
	//TODO: Make sure the RSF bit gets set appropriately in the reply blocks, from a global flag. Reset it in DoSystemFlagScan

	if (CompleteMD3Message.size() != 1)
	{
		//TODO Handle Flag scan conmmands with more than one block
		SendControlOrScanRejected(Header);	// If we did not get one blocks, then send back a command rejected message - for NOW
		return;
	}


	//TODO: SJE Dont think the flag scan will be passed through ODC, will just mirror what we know about the outstation
	std::vector<MD3BlockData> ResponseMD3Message;

	// Change the direction, set the flag data.
	MD3BlockFormatted RetBlock(Header.GetStationAddress(), false, SYSTEM_FLAG_SCAN, 0,0);
	// Need to set the second word values to the flag values.

	ResponseMD3Message.push_back(RetBlock);
	// Set the flags????
	SendMD3Message(ResponseMD3Message);
}

// Function 15 Output
void MD3OutstationPort::SendControlOK(MD3BlockFormatted &Header)
{
	// The Control OK block seems to return the orginating message first block, but with the function code changed to 15
	std::vector<MD3BlockData> ResponseMD3Message;

	// The MD3BlockFn15StoM does the changes we need for us.
	MD3BlockFn15StoM FormattedBlock(Header);
	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}
// Function 30 Output
void MD3OutstationPort::SendControlOrScanRejected(MD3BlockFormatted &Header)
{
	// The Control rejected block seems to return the orginating message first block, but with the function code changed to 30
	std::vector<MD3BlockData> ResponseMD3Message;

	// The MD3BlockFn30StoM does the changes we need for us.
	MD3BlockFn30StoM FormattedBlock(Header);
	ResponseMD3Message.push_back(FormattedBlock);
	SendMD3Message(ResponseMD3Message);
}

#pragma endregion