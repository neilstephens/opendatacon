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
#include <iostream>
#include <future>
#include <regex>
#include <chrono>

#include "CB.h"
#include "CBUtility.h"
#include "CBOutstationPort.h"


void CBOutstationPort::ProcessCBMessage(CBMessage_t &CompleteCBMessage)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.
	assert(CompleteCBMessage.size() != 0);

	size_t ExpectedMessageSize = 1; // Only set in switch statement if not 1

	CBBlockData &Header = CompleteCBMessage[0];
	// Now based on the Command Function, take action. Some of these are responses from - not commands to an OutStation.

	bool NotImplemented = false;

	// All are included to allow better error reporting.
	// The Python simulator only implements the commands below, which will be our first target:
	//'0': 'scan data',
	//	'1' : 'execute',
	//	'2' : 'trip/close',
	//	'4' : 'trip/close',
	//	'9' : 'master station request'
	switch (Header.GetFunctionCode())
	{
		case FUNC_SCAN_DATA:
			break;
		case FUNC_EXECUTE_COMMAND:
			break;
		case FUNC_TRIP:
			break;
		case FUNC_SETPOINT_A:
			NotImplemented = true;
			break;
		case FUNC_CLOSE:
			break;
		case FUNC_SETPOINT_B:
			NotImplemented = true;
			break;
		case FUNC_RESET:
			NotImplemented = true;
			break;
		case FUNC_MASTER_STATION_REQUEST:
			break;
		case FUNC_SEND_NEW_SOE:
			NotImplemented = true;
			break;
		case FUNC_REPEAT_SOE:
			NotImplemented = true;
			break;
		case FUNC_UNIT_RAISE_LOWER:
			NotImplemented = true;
			break;
		case FUNC_FREEZE_AND_SCAN_ACC:
			NotImplemented = true;
			break;
		case FUNC_FREEZE_SCAN_AND_RESET_ACC:
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

	if (ExpectedMessageSize != CompleteCBMessage.size())
	{
		LOGERROR("Unexpected Message Size - " + std::to_string(CompleteCBMessage.size()) +
			" On Station Address - " + std::to_string(Header.GetStationAddress())+
			" Function - " + std::to_string(Header.GetFunctionCode()));
	}
}

#pragma region Worker Methods

// This method is passed to the SystemFlags variable to do the necessary calculation
// Access through SystemFlags.GetDigitalChangedFlag()
bool CBOutstationPort::DigitalChangedFlagCalculationMethod(void)
{
	// Return true if there is unsent digital changes
	return (CountBinaryBlocksWithChanges() > 0);
}
// This method is passed to the SystemFlags variable to do the necessary calculation
// Access through SystemFlags.GetTimeTaggedDataAvailableFlag()
bool CBOutstationPort::TimeTaggedDataAvailableFlagCalculationMethod(void)
{
	return true; //TODO: MyPointConf->PointTable.TimeTaggedDataAvailable();
}

#pragma endregion

#pragma region ANALOG and COUNTER
// Function 5
void CBOutstationPort::DoAnalogUnconditional(CBBlockData &Header)
{
	LOGDEBUG("OS - DoAnalogUnconditional - Fn5");
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

//	ReadAnalogOrCounterRange(Header.GetGroup(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
//	SendAnalogOrCounterUnconditional(ANALOG_UNCONDITIONAL,AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
}

// Function 31 - Essentially the same as Analog Unconditional. Either can be used to return either analog or counter, or both.
// The only difference is that an analog module seems to have 16 channels, the counter module only 8.
// The expectation is that if you ask for more than 8 from a counter module, it will roll over to the next module (counter or analog).
// As an analog module has 16, the most that can be requested the overflow never happens.
// In order to make this work, we need to know if the module we are dealing with is a counter or analog module.
void CBOutstationPort::DoCounterScan(CBBlockData &Header)
{
	LOGDEBUG("OS - DoCounterScan - Fn31");
	// This has only one response
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

	// This is the method that has to deal with analog/counter channel overflow issues - into the next module.
//	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now send those values.
//	SendAnalogOrCounterUnconditional(COUNTER_SCAN, AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
}
// Function 6
void CBOutstationPort::DoAnalogDeltaScan( CBBlockData &Header )
{
	LOGDEBUG("OS - DoAnalogDeltaScan - Fn6");
	// First work out what our response will be, loading the data to be sent into two vectors.
	std::vector<uint16_t> AnalogValues;
	std::vector<int> AnalogDeltaValues;
	AnalogChangeType ResponseType = NoChange;

//	ReadAnalogOrCounterRange(Header.GetModuleAddress(), Header.GetChannels(), ResponseType, AnalogValues, AnalogDeltaValues);

	// Now we know what type of response we are going to send.
	if (ResponseType == AllChange)
	{
//		SendAnalogOrCounterUnconditional(ANALOG_UNCONDITIONAL,AnalogValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
	else if (ResponseType == DeltaChange)
	{
//		SendAnalogDelta(AnalogDeltaValues, Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
	else
	{
//		SendAnalogNoChange(Header.GetStationAddress(), Header.GetModuleAddress(), Header.GetChannels());
	}
}

void CBOutstationPort::ReadAnalogOrCounterRange(uint8_t Group, uint8_t Channels, CBOutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues)
{
	// The Analog and Counters are  maintained in two lists, we need to deal with both of them as they both can be read by this method.
	// So if we find an entry in the analog list, we dont have to worry about overflow, as there are 16 channels, and the most we can ask for is 16.
	//
	uint16_t wordres = 0;
	bool hasbeenset;

	// Is it a counter module? Fails if not at this address
	if (MyPointConf->PointTable.GetCounterValueUsingCBIndex(Group, 0, wordres, hasbeenset))
	{
		// We have a counter module, can get up to 8 values from it
		uint8_t chancnt = Channels >= 8 ? 8 : Channels;
		GetAnalogModuleValues(CounterModule,chancnt, Group, ResponseType, AnalogValues, AnalogDeltaValues);

		if (Channels > 8)
		{
			// Now we have to get the remaining channels (up to 8) from the next module, if it exists.
			// Check if it is a counter, otherwise assume analog. Will return correct error codes if not there
			if (MyPointConf->PointTable.GetCounterValueUsingCBIndex(Group+1, 0, wordres,hasbeenset))
			{
				GetAnalogModuleValues(CounterModule, Channels - 8, Group + 1, ResponseType, AnalogValues, AnalogDeltaValues);
			}
			else
			{
				GetAnalogModuleValues(AnalogModule, Channels - 8, Group + 1, ResponseType, AnalogValues, AnalogDeltaValues);
			}
		}
	}
	else
	{
		// It must be an analog module (if it does not exist, we return appropriate error codes anyway
		// Yes proceed , up to 16 channels.
		GetAnalogModuleValues(AnalogModule, Channels, Group, ResponseType, AnalogValues, AnalogDeltaValues);
	}
}
void CBOutstationPort::GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, uint8_t Channels, uint8_t Group, CBOutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t> & AnalogValues, std::vector<int> & AnalogDeltaValues)
{
	for (uint8_t i = 0; i < Channels; i++)
	{
		uint16_t wordres = 0;
		int deltares = 0;
		bool hasbeenset;
		bool foundentry = false;

		if (IsCounterOrAnalog == CounterModule)
		{
			foundentry = MyPointConf->PointTable.GetCounterValueAndChangeUsingCBIndex(Group, i, wordres, deltares,hasbeenset);
		}
		else
		{
			foundentry= MyPointConf->PointTable.GetAnalogValueAndChangeUsingCBIndex(Group, i, wordres, deltares,hasbeenset);
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
void CBOutstationPort::SendAnalogOrCounterUnconditional(uint8_t functioncode, std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t Group, uint8_t Channels)
{
	CBMessage_t ResponseCBMessage;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,

	CBBlockData FormattedBlock = CBBlockData(StationAddress, false, functioncode, Group, Channels);

	ResponseCBMessage.push_back(FormattedBlock);

	assert(Channels == Analogs.size());

	int NumberOfDataBlocks = Channels / 2 + Channels % 2; // 2 --> 1, 3 -->2

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = CBBlockData(Analogs[2 * i], Analogs[2 * i + 1], lastblock);
		ResponseCBMessage.push_back(block);
	}
	SendCBMessage(ResponseCBMessage);
}
void CBOutstationPort::SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t Group, uint8_t Channels)
{
	CBMessage_t ResponseCBMessage;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	CBBlockData FormattedBlock = CBBlockData(StationAddress, false, 122, Group, Channels);

	ResponseCBMessage.push_back(FormattedBlock);

	assert(Channels == Deltas.size());

	// Can be 4 channel delta values to a block.
	int NumberOfDataBlocks = Channels / 4 + (Channels % 4 == 0 ? 0 : 1);

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool lastblock = (i + 1 == NumberOfDataBlocks);

		auto block = CBBlockData(numeric_cast<uint8_t>(Deltas[i * 4]), numeric_cast<uint8_t>(Deltas[i * 4 + 1]), numeric_cast<uint8_t>(Deltas[i * 4 + 2]), numeric_cast<uint8_t>(Deltas[i * 4 + 3]), lastblock);
		ResponseCBMessage.push_back(block);
	}
	SendCBMessage(ResponseCBMessage);
}
void CBOutstationPort::SendAnalogNoChange(uint8_t StationAddress, uint8_t Group, uint8_t Channels)
{
	CBMessage_t ResponseCBMessage;

	// The spec says echo the formatted block, but a few things need to change. EndOfMessage, MasterToStationMessage,
	CBBlockData FormattedBlock = CBBlockData(StationAddress, 2, 3,0xFFF,true);

	ResponseCBMessage.push_back(FormattedBlock);
	SendCBMessage(ResponseCBMessage);
}

#pragma endregion

#pragma region DIGITAL


// Function 11
void CBOutstationPort::DoDigitalScan(CBBlockData &Header)
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

	LOGDEBUG("OS - DoDigitalScan - Fn11");

	CBMessage_t ResponseCBMessage;

/*	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
      {
            // The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Don't do anything else.
            // If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
            // It would get very tricky to only commit change written information only when a new sequence number turns up.
            // The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

            SendCBMessage(LastDigitialScanResponseCBMessage);
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
            LOGDEBUG("OS - DoDigitalUnconditional - Nothing to send");
            CBBlockData FormattedBlock = CBBlockFn14StoM(Header.GetStationAddress(), Header.GetDigitalSequenceNumber());
            FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

            ResponseCBMessage.push_back(FormattedBlock);
      }
      else
      {
            // We have data to send.
            uint8_t TaggedEventCount = 0;
            uint8_t ModuleCount = Limit(ChangedBlocks, Header.GetModuleCount());

            // Set up the response block
            CBBlockFn11StoM FormattedBlock(Header.GetStationAddress(), TaggedEventCount, Header.GetDigitalSequenceNumber(), ModuleCount);
            ResponseCBMessage.push_back(FormattedBlock);

            // Add non-time tagged data. We can return a data block or a status block for each module.
            // If the module is active (not faulted) return the data, otherwise return the status block..
            // We always just start with the first module and move up through changed blocks until we reach the ChangedBlocks count

            std::vector<uint8_t> ModuleList;
            BuildListOfModuleAddressesWithChanges(0, ModuleList);

            BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseCBMessage);

            ModuleCount = static_cast<uint8_t>(ResponseCBMessage.size() - 1); // The number of module block is the size at this point, less the start block.

            if (AreThereTaggedEvents)
            {
                  // Add time tagged data
                  std::vector<uint16_t> ResponseWords;

                  Fn11AddTimeTaggedDataToResponseWords(Header.GetTaggedEventCount(), TaggedEventCount, ResponseWords); // TaggedEvent count is a ref, zero on entry and exit with number of tagged events added

                  // Convert data to 32 bit blocks and pad if necessary.
                  if ((ResponseWords.size() % 2) != 0)
                  {
                        // Add a filler packet if necessary to the end of the message. High byte zero, low byte does not matter.
                        ResponseWords.push_back(CBBlockFn11StoM::FillerPacket());
                  }

                  //TODO: NEW Style Digital - A flag block can appear in the OutStation Fn11 time tagged response - which can indicate Internal Buffer Overflow, Time sync fail and restoration. We have not implemented this

                  // Now translate the 16 bit packets into the 32 bit CB blocks.
                  for (uint16_t i = 0; i < ResponseWords.size(); i = i + 2)
                  {
                        CBBlockData blk(ResponseWords[i], ResponseWords[i + 1]);
                        ResponseCBMessage.push_back(blk);
                  }
            }

            // Mark the last block and set the module count and tagged event count
            if (ResponseCBMessage.size() != 0)
            {
                  CBBlockFn11StoM &firstblockref = static_cast<CBBlockFn11StoM&>(ResponseCBMessage.front());
                  firstblockref.SetModuleCount(ModuleCount);           // The number of blocks taking away the header...
                  firstblockref.SetTaggedEventCount(TaggedEventCount); // Update to the number we are sending
                  firstblockref.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

                  CBBlockData &lastblock = ResponseCBMessage.back();
                  lastblock.MarkAsEndOfMessageBlock();
            }
            LOGDEBUG("OS - DoDigitalUnconditional - Sending "+std::to_string(ModuleCount)+" Module Data Words and "+std::to_string(TaggedEventCount)+" Tagged Events");
      }
      SendCBMessage(ResponseCBMessage);

      // Store this set of packets in case we have to resend
      LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
      LastDigitialScanResponseCBMessage = ResponseCBMessage;
      */
}

void CBOutstationPort::MarkAllBinaryPointsAsChanged()
{
	MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &pt)
		{
			pt.SetChangedFlag();
		});
}

void CBOutstationPort::Fn11AddTimeTaggedDataToResponseWords(uint8_t MaxEventCount, uint8_t &EventCount, std::vector<uint16_t> &ResponseWords)
{
	CBBinaryPoint CurrentPoint;
	uint64_t LastPointmsec = 0;

	while ((EventCount < MaxEventCount)) // && MyPointConf->PointTable.PeekNextTaggedEventPoint(CurrentPoint))
	{
		// The time date is seconds, the data block contains a msec entry.
		// The time is accumulated from each event in the queue. If we have greater than 255 msec between events,
		// add a time packet to bridge the gap.

		if (EventCount == 0)
		{
			// First packet is the time/date block
			uint32_t FirstEventSeconds = static_cast<uint32_t>(CurrentPoint.GetChangedTime() / 1000);
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
		ResponseWords.push_back(ShiftLeft8Result16Bits(CurrentPoint.GetGroup()) | numeric_cast<uint16_t>(msecoffset));
		ResponseWords.push_back(CurrentPoint.GetModuleBinarySnapShot());

		LastPointmsec = CurrentPoint.GetChangedTime(); // Update the last changed time to match what we have just sent.
		//	MyPointConf->PointTable.PopNextTaggedEventPoint();
		EventCount++;
	}
}
// Function 12
void CBOutstationPort::DoDigitalUnconditional(CBBlockData &Header)
{
	CBMessage_t ResponseCBMessage;

	LOGDEBUG("OS - DoDigitalUnconditional - Fn12");

/*	if ((Header.GetDigitalSequenceNumber() == LastDigitalScanSequenceNumber) && (Header.GetDigitalSequenceNumber() != 0))
      {
            // The last time we sent this, it did not get there, so we have to just resend that stored set of blocks. Dont do anything else.
            // If we go back and reread the Binary bits, the change information will already have been lost, as we assume it was sent when we read it last time.
            // It would get very tricky to only commit change written information only when a new sequence number turns up.
            // The downside is that the stored message could be a no change message, but data may have changed since we last sent it.

            SendCBMessage(LastDigitialScanResponseCBMessage);
            return;
      }

      // The reply is the same format as for Fn11, but without time tagged data. Is the function code returned 11 or 12?
      std::vector<uint8_t> ModuleList;
      BuildListOfModuleAddressesWithChanges(Header.GetModuleCount(),Header.GetModuleAddress(), true, ModuleList);

      // Set up the response block - module count to be updated
      CBBlockFn11StoM FormattedBlock(Header.GetStationAddress(), 0, Header.GetDigitalSequenceNumber(), Header.GetModuleCount());
      ResponseCBMessage.push_back(FormattedBlock);

      BuildScanReturnBlocksFromList(ModuleList, Header.GetModuleCount(), Header.GetStationAddress(), true, ResponseCBMessage);

      // Mark the last block
      if (ResponseCBMessage.size() != 0)
      {
            CBBlockFn11StoM &firstblockref = static_cast<CBBlockFn11StoM&>(ResponseCBMessage.front());
            firstblockref.SetModuleCount(static_cast<uint8_t>(ResponseCBMessage.size() - 1)); // The number of blocks taking away the header...
            firstblockref.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

            CBBlockData &lastblock = ResponseCBMessage.back();
            lastblock.MarkAsEndOfMessageBlock();
      }

      SendCBMessage(ResponseCBMessage);

      // Store this set of packets in case we have to resend
      LastDigitalScanSequenceNumber = Header.GetDigitalSequenceNumber();
      LastDigitialScanResponseCBMessage = ResponseCBMessage;
      */
}

// Scan all binary/digital blocks for changes - used to determine what response we need to send
// We return the total number of changed blocks we assume every block supports time tagging
// If SendEverything is true,
uint8_t CBOutstationPort::CountBinaryBlocksWithChanges()
{
	uint8_t changedblocks = 0;
	int lastblock = -1; // Non valid value

	// The map is sorted, so when iterating, we are working to a specific order. We can have up to 16 points in a block only one changing will trigger a send.
	MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &pt)
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
// This is used to determine which response we should send NoChange, DeltaChange or AllChange
uint8_t CBOutstationPort::CountBinaryBlocksWithChangesGivenRange(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress)
{
	uint8_t changedblocks = 0;

	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool datachanged = false;

		for (uint8_t j = 0; j < 16; j++)
		{
			bool changed = false;

			if (!MyPointConf->PointTable.GetBinaryChangedUsingCBIndex(StartModuleAddress + i, j, changed)) // Does not change the changed bit
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
void CBOutstationPort::BuildListOfModuleAddressesWithChanges(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, bool forcesend, std::vector<uint8_t> &ModuleList)
{
	// We want a list of modules to send...
	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		bool datachanged = false;

		for (uint8_t j = 0; j < 16; j++)
		{
			bool changed = false;

			if (!MyPointConf->PointTable.GetBinaryChangedUsingCBIndex(StartModuleAddress + i, j, changed))
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
void CBOutstationPort::BuildListOfModuleAddressesWithChanges(uint8_t StartModuleAddress, std::vector<uint8_t> &ModuleList)
{
	uint8_t LastModuleAddress = 0;
	bool WeAreScanning = (StartModuleAddress == 0); // If the startmoduleaddress is zero, we store changes from the start.

	MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &Point)
		{
			uint8_t Group = Point.GetGroup();

			if (Group == StartModuleAddress)
				WeAreScanning = true;

			if (WeAreScanning && Point.GetChangedFlag() && (LastModuleAddress != Group))
			{
			// We should save the module address so we have a list of modules that have changed
			      ModuleList.push_back(Group);
			      LastModuleAddress = Group;
			}
		});

	if (StartModuleAddress != 0)
	{
		// We have to then start again from zero, if we did not start from there.
		MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &Point)
			{
				uint8_t Group = Point.GetGroup();

				if (Group == StartModuleAddress)
					WeAreScanning = false; // Stop when we reach the start again

				if (WeAreScanning && Point.GetChangedFlag() && (LastModuleAddress != Group))
				{
				// We should save the module address so we have a list of modules that have changed
				      ModuleList.push_back(Group);
				      LastModuleAddress = Group;
				}
			});
	}
}
// Fn 7,8
void CBOutstationPort::BuildBinaryReturnBlocks(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, uint8_t StationAddress, bool forcesend, CBMessage_t &ResponseCBMessage)
{
	std::vector<uint8_t> ModuleList;
	BuildListOfModuleAddressesWithChanges(NumberOfDataBlocks, StartModuleAddress, forcesend, ModuleList);
	// We have a list of modules to send...
	BuildScanReturnBlocksFromList(ModuleList, NumberOfDataBlocks, StationAddress,false, ResponseCBMessage);
}

// Fn 7, 8 and 10, 11 and 12 NOT Fn 9
void CBOutstationPort::BuildScanReturnBlocksFromList(std::vector<unsigned char> &ModuleList, uint8_t MaxNumberOfDataBlocks, uint8_t StationAddress, bool FormatForFn11and12, CBMessage_t & ResponseCBMessage)
{
	// For each module address, or the max we can send
	for (size_t i = 0; ((i < ModuleList.size()) && (i < MaxNumberOfDataBlocks)); i++)
	{
		uint8_t Group = ModuleList[i];

		// Have to collect all the bits into a uint16_t
		bool ModuleFailed = false;

		uint16_t wordres = MyPointConf->PointTable.CollectModuleBitsIntoWordandResetChangeFlags(Group, ModuleFailed);

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
				uint16_t lowword = ShiftLeft8Result16Bits(errorflags) | Group;
				uint16_t highword = ShiftLeft8Result16Bits(StationAddress);
				auto block = CBBlockData(highword, lowword, false);
				ResponseCBMessage.push_back(block);
			}
		}
		else
		{
			if (FormatForFn11and12)
			{
				// For Fn11 and 12 the data format is:
				uint16_t address = ShiftLeft8Result16Bits(Group); // Low byte is msec offset - which is 0 for non time tagged data
				auto block = CBBlockData(address, wordres, false);
				ResponseCBMessage.push_back(block);
			}
			else
			{
				// Queue the data block Fn 7,8 and 10
				uint16_t address = ShiftLeft8Result16Bits(StationAddress) | Group;
				auto block = CBBlockData(address, wordres, false);
				ResponseCBMessage.push_back(block);
			}
		}
	}
	// Not sure which is the last block for the send only changes..so just mark it when we get to here.
	// Format Fn 11 and 12 may not be the end of packet
	if ((ResponseCBMessage.size() != 0) && !FormatForFn11and12)
	{
		// The header for Fn7, Fn 8 and Fn 10 need this. NOT Fn9, Fn11 and Fn12
		CBBlockData &firstblockref = ResponseCBMessage.front();

		CBBlockData &lastblock = ResponseCBMessage.back();
		lastblock.MarkAsEndOfMessageBlock();
	}
}


#pragma endregion

#pragma region CONTROL

void CBOutstationPort::DoFreezeResetCounters(CBBlockData &Header)
{
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.
	// Don't have an equivalent ODC command?

	bool failed = false;


	uint32_t ODCIndex = MyPointConf->FreezeResetCountersPoint.second;
	MyPointConf->FreezeResetCountersPoint.first = static_cast<int32_t>(Header.GetData()); // Pass the actual packet to the master across ODC

	EventTypePayload<EventType::AnalogOutputInt32>::type val;
	val.first = MyPointConf->FreezeResetCountersPoint.first;

	auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt32, ODCIndex, Name);
	event->SetPayload<EventType::AnalogOutputInt32>(std::move(val));

	bool waitforresult = !MyPointConf->StandAloneOutstation;

	if (Header.GetStationAddress() != 0)
	{
		if (failed)
		{
			SendControlOrScanRejected(Header);
			return;
		}

		if (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS)
		{
			SendControlOK(Header);
		}
		else
		{
			SendControlOrScanRejected(Header);
		}
	}
	else
	{
		if (!failed)
		{
			// This is an all station (0 address) freeze reset function.
			// No response, don't wait for a result
			Perform(event, false);
		}
	}
}

// Think that POM stands for PULSE.
void CBOutstationPort::DoPOMControl(CBBlockData &Header, CBMessage_t &CompleteCBMessage)
{
	// We have two blocks incoming, not just one.
	// Seems we don\92t have any POM control signals greater than 7 in the data I have seen??
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.

	LOGDEBUG("OS - DoPOMControl");

	bool failed = false;

	// Check all the things we can
	if (CompleteCBMessage.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}


	// Check that the control point is defined, otherwise return a fail.
	size_t ODCIndex = 0;
//	failed = MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(Header.GetModuleAddress(), Header.GetOutputSelection() % 8, ODCIndex) ? failed : true;

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

	bool waitforresult = !MyPointConf->StandAloneOutstation;
	bool success = true;

	// POM is only a single bit, so easy to do... if the bit position is > 7, i.e.TRIP/OPEN/OFF 8-15 and PULSE_CLOSE/PULSE/LATCH_ON 0-7 for the up to 8 points in a POM module.
	bool point_on = 1; // (Header.GetOutputSelection() < 8);

	if (MyPointConf->POMControlPoint.second == 0) // If NO pass through, then do normal operation
	{
		// Module contains 0 to 7 channels..
/*		if (MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(Header.GetModuleAddress(), Header.GetOutputSelection() % 8, ODCIndex))
            {
                  EventTypePayload<EventType::ControlRelayOutputBlock>::type val;
                  val.functionCode = point_on ? ControlCode::LATCH_ON : ControlCode::LATCH_OFF;

                  auto event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock, ODCIndex, Name);
                  event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(val));

                  success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS); // If no subscribers will return quickly.
            }
*/    }

	// If the index is !=0, then the pass through is active. So success depends on the pass through
	if (MyPointConf->POMControlPoint.second != 0)
	{
		// Pass the command through ODC, just for CB on the other side.
		MyPointConf->POMControlPoint.first = static_cast<int32_t>(Header.GetData());

		ODCIndex = MyPointConf->POMControlPoint.second;

		EventTypePayload<EventType::AnalogOutputInt32>::type val;
		val.first = MyPointConf->POMControlPoint.first;

		auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt32, ODCIndex, Name);
		event->SetPayload<EventType::AnalogOutputInt32>(std::move(val));

		success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS);
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

// DOM stands for DIGITAL.
void CBOutstationPort::DoDOMControl(CBBlockData &Header, CBMessage_t &CompleteCBMessage)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.
	// The output is a 16bit word. Will send a 32 bit block through ODC that contains the output data , station address and module address.
	// The Perform method waits until it has a result, unless we are in stand alone mode.

	LOGDEBUG("OS - DoDOMControl");
	bool failed = false;

	// Check all the things we can
	if (CompleteCBMessage.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}


	// Check that the first one exists, not all 16 may exist.
	size_t ODCIndex = 0;
//	failed = MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(Header.GetModuleAddress(), 0, ODCIndex) ? failed : true;

	uint16_t output = 1; // Header.GetOutputFromSecondBlock(CompleteCBMessage[1]);
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

	bool success = true;
/*
      if (MyPointConf->DOMControlPoint.second == 0) // NO pass through, normal operation
      {
            // Send each of the DigitalOutputs (If we were connected to an DNP3 Port the CB pass through would not work)
            for (uint8_t i = 0; i < 16; i++)
            {
                  if (MyPointConf->PointTable.GetBinaryControlODCIndexUsingCBIndex(Header.GetModuleAddress(), i, ODCIndex))
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
      }

      // If the index is !=0, then the pass through is active. So success depends on the pass through
      if (MyPointConf->DOMControlPoint.second != 0)
      {
            // Pass the command through ODC, just for CB on the other side. Have to compress to fit into 32 bits
            uint32_t PacketData = static_cast<uint32_t>(Header.GetStationAddress()) << 24 | static_cast<uint32_t>(Header.GetModuleAddress()) << 16 | static_cast<uint32_t>(CompleteCBMessage[1].GetFirstWord());
            MyPointConf->DOMControlPoint.first = static_cast<int32_t>(PacketData);
            ODCIndex = MyPointConf->DOMControlPoint.second;

            EventTypePayload<EventType::AnalogOutputInt32>::type val;
            val.first = MyPointConf->DOMControlPoint.first;

            auto event = std::make_shared<EventInfo>(EventType::AnalogOutputInt32, ODCIndex, Name);
            event->SetPayload<EventType::AnalogOutputInt32>(std::move(val));

            success = (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS);
      }
*/
	if (success)
	{
		SendControlOK(Header);
	}
	else
	{
		SendControlOrScanRejected(Header);
	}
}

// AOM is ANALOG output control.
void CBOutstationPort::DoAOMControl(CBBlockData &Header, CBMessage_t &CompleteCBMessage)
{
	// We have two blocks incoming, not just one.
	// If the Station address is 0x00 - no response, otherwise, Response can be Fn 15 Control OK, or Fn 30 Control or scan rejected
	// We have to pass the command to ODC, then set up a lambda to handle the sending of the response - when we get it.

	LOGDEBUG("OS - DoAOMControl");

	bool failed = false;

	// Check all the things we can
	if (CompleteCBMessage.size() != 2)
	{
		// If we did not get two blocks, then send back a command rejected message.
		failed = true;
	}

	if (Header.GetStationAddress() == 0)
	{
		// An all station command we do not respond to.
		return;
	}

	// Check that the control point is defined, otherwise return a fail.
	size_t ODCIndex = 0;
/*	failed = MyPointConf->PointTable.GetAnalogControlODCIndexUsingCBIndex(Header.GetModuleAddress(), Header.GetChannel(), ODCIndex) ? failed : true;

      uint16_t output = Header.GetOutputFromSecondBlock(CompleteCBMessage[1]);
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
      */
}

#pragma endregion

#pragma region SYSTEM

// Function 44
void CBOutstationPort::DoSetDateTimeNew(CBBlockData &Header, CBMessage_t &CompleteCBMessage)
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

	LOGDEBUG("OS - DoSetdateTimeNew");

	if ((CompleteCBMessage.size() != 3) && (Header.GetStationAddress() != 0))
	{
		SendControlOrScanRejected(Header); // If we did not get three blocks, then send back a command rejected message.
		return;
	}

	CBBlockData &timedateblock = CompleteCBMessage[1];

	// If date time is within a window of now, accept. Otherwise send command rejected.
	uint64_t msecsinceepoch = 1; // static_cast<uint64_t>(timedateblock.GetData()) * 1000 + Header.GetMilliseconds();

	// Not used for now...
	// CBBlockData &utcoffsetblock = CompleteCBMessage[2];
	// int utcoffsetminutes = (int)utcoffsetblock.GetFirstWord();

	// CB only maintains a time tagged change list for digitals/binaries Epoch is 1970, 1, 1 - Same as for CB
	uint64_t currenttime = CBNow();

	if (abs(static_cast<int64_t>(msecsinceepoch) - static_cast<int64_t>(currenttime)) > 30000) // Set window as +-30 seconds
	{
		if (Header.GetStationAddress() != 0)
			SendControlOrScanRejected(Header);
	}
	else
	{
		uint32_t ODCIndex = MyPointConf->TimeSetPointNew.second;
		MyPointConf->TimeSetPointNew.first = numeric_cast<double>(msecsinceepoch); // Fit the 64 bit int into the 64 bit float.

		EventTypePayload<EventType::AnalogOutputDouble64>::type val;
		val.first = MyPointConf->TimeSetPointNew.first;

		auto event = std::make_shared<EventInfo>(EventType::AnalogOutputDouble64, ODCIndex, Name);
		event->SetPayload<EventType::AnalogOutputDouble64>(std::move(val));

		// If StandAloneOutstation, don\92t wait for the result - problem is ODC will always wait - no choice on commands. If no subscriber, will return immediately - good for testing
		bool waitforresult = !MyPointConf->StandAloneOutstation;

		// This does a PublishCommand and waits for the result - or times out.
		if (Perform(event, waitforresult) == odc::CommandStatus::SUCCESS)
		{
			if (Header.GetStationAddress() != 0)
				SendControlOK(Header);
		}
		else
		{
			if (Header.GetStationAddress() != 0)
				SendControlOrScanRejected(Header);
		}
	}
	SystemFlags.TimePacketReceived();
}

// Function 15 Output
void CBOutstationPort::SendControlOK(CBBlockData &Header)
{
	// The Control OK block seems to return the originating message first block, but with the function code changed to 15
	CBMessage_t ResponseCBMessage;

	// The CBBlockFn15StoM does the changes we need for us. The control blocks do not have space for the System flags to be returned...
	CBBlockData FormattedBlock(Header);
	ResponseCBMessage.push_back(FormattedBlock);
	SendCBMessage(ResponseCBMessage);
}
// Function 30 Output
void CBOutstationPort::SendControlOrScanRejected(CBBlockData &Header)
{
	// The Control rejected block seems to return the originating message first block, but with the function code changed to 30
	CBMessage_t ResponseCBMessage;

	// The CBBlockFn30StoM does the changes we need for us.
	CBBlockData FormattedBlock(Header);
	//FormattedBlock.SetFlags(SystemFlags.GetRemoteStatusChangeFlag(), SystemFlags.GetTimeTaggedDataAvailableFlag(), SystemFlags.GetDigitalChangedFlag());

	ResponseCBMessage.push_back(FormattedBlock);
	SendCBMessage(ResponseCBMessage);
}

#pragma endregion
