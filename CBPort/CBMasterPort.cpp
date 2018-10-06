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
* CBMasterPort.cpp
*
*  Created on: 15/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#include <thread>
#include <chrono>
#include <array>

#include "CB.h"
#include "CBUtility.h"
#include "CBMasterPort.h"


CBMasterPort::CBMasterPort(const std::string &aName, const std::string &aConfFilename, const Json::Value &aConfOverrides):
	CBPort(aName, aConfFilename, aConfOverrides),
	PollScheduler(nullptr)
{
	std::string over = "None";
	if (aConfOverrides.isObject()) over = aConfOverrides.toStyledString();

	IsOutStation = false;

	LOGDEBUG("CBMaster Constructor - " + aName + " - " + aConfFilename + " Overrides - " + over);
}

CBMasterPort::~CBMasterPort()
{
	Disable();
	CBConnection::RemoveMaster(MyConf->mAddrConf.ChannelID(),MyConf->mAddrConf.OutstationAddr);
}

void CBMasterPort::Enable()
{
	if (enabled) return;
	try
	{
		CBConnection::Open(MyConf->mAddrConf.ChannelID()); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop

		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection TCP : " + Name + " : " + e.what());
		return;
	}
}
void CBMasterPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	CBConnection::Close(MyConf->mAddrConf.ChannelID()); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void CBMasterPort::SocketStateHandler(bool state)
{
	std::string msg;
	if (state)
	{
		PollScheduler->Start();
		PublishEvent(std::move(ConnectState::CONNECTED));
		msg = Name + ": Connection established.";
		ResetDigitalCommandSequenceNumber(); // Outstation when it sees this will send all digital values as if on power up.
	}
	else
	{
		PollScheduler->Stop();

		SetAllPointsQualityToCommsLost(); // All the connected points need their quality set to comms lost.

		ClearCBCommandQueue(); // Remove all waiting commands and callbacks

		PublishEvent(std::move(ConnectState::DISCONNECTED));
		msg = Name + ": Connection closed.";
	}
	LOGINFO(msg);
}

void CBMasterPort::Build()
{
	uint64_t ChannelID = MyConf->mAddrConf.ChannelID();

	if (PollScheduler == nullptr)
		PollScheduler.reset(new ASIOScheduler(*pIOS));

	MasterCommandProtectedData.CurrentCommandTimeoutTimer.reset(new Timer_t(*pIOS));
	MasterCommandStrand.reset(new asio::strand(*pIOS));

	// Need a couple of things passed to the point table.
	MyPointConf->PointTable.Build(IsOutStation, *pIOS);

	// Creates internally if necessary
	CBConnection::AddConnection(pIOS, IsServer(), MyConf->mAddrConf.IP,MyConf->mAddrConf.Port, MyPointConf->IsBakerDevice, MyConf->mAddrConf.TCPConnectRetryPeriodms); //Static method

	CBConnection::AddMaster(ChannelID,MyConf->mAddrConf.OutstationAddr,
		std::bind(&CBMasterPort::ProcessCBMessage, this, std::placeholders::_1),
		std::bind(&CBMasterPort::SocketStateHandler, this, std::placeholders::_1),
		MyPointConf->IsBakerDevice);

	PollScheduler->Stop();
	PollScheduler->Clear();
	for (auto pg : MyPointConf->PollGroups)
	{
		auto id = pg.second.ID;
		auto action = [=]()
				  {
					  this->DoPoll(id);
				  };
		PollScheduler->Add(pg.second.pollrate, action);
	}
	//	PollScheduler->Start(); // This is started and stopped in the socket state handler
}

void CBMasterPort::SendCBMessage(const CBMessage_t &CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 0)
	{
		LOGERROR("MA - Tried to send an empty message to the TCP Port");
		return;
	}
	LOGDEBUG("MA - Sending Message - " + CBMessageAsString(CompleteCBMessage));

	// Done this way just to get context into log messages.
	CBPort::SendCBMessage(CompleteCBMessage);
}

#pragma region MasterCommandQueue

// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
// There is a fixed timeout function (below) which will queue the next command if we timeout.
// If the ProcessCBMessage callback gets the command it expects, it will send the next command in the queue.
// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
void CBMasterPort::QueueCBCommand(const CBMessage_t &CompleteCBMessage, SharedStatusCallback_t pStatusCallback)
{
	MasterCommandStrand->dispatch([=]() // Tries to execute, if not able to will post. Note the calling thread must be one of the io_service threads.... this changes our tests!
		{
			if (MasterCommandProtectedData.MasterCommandQueue.size() < MasterCommandProtectedData.MaxCommandQueueSize)
			{
			      MasterCommandProtectedData.MasterCommandQueue.push(MasterCommandQueueItem(CompleteCBMessage, pStatusCallback)); // async
			}
			else
			{
			      LOGDEBUG("Tried to queue another CB Master Command when the command queue is full");
			      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED); // Failed...
			}

			// Will only send if we can - blockindex.e. not currently processing a command
			UnprotectedSendNextMasterCommand(false);
		});
}
// Handle the many single block command messages better
void CBMasterPort::QueueCBCommand(const CBBlockData &SingleBlockCBMessage, SharedStatusCallback_t pStatusCallback)
{
	CBMessage_t CommandCBMessage;
	CommandCBMessage.push_back(SingleBlockCBMessage);
	QueueCBCommand(CommandCBMessage, pStatusCallback);
}


// Just schedule the callback, don't want to do it in a strand protected section.
void CBMasterPort::PostCallbackCall(const odc::SharedStatusCallback_t &pStatusCallback, CommandStatus c)
{
	if (pStatusCallback != nullptr)
	{
		pIOS->post([&, pStatusCallback, c]()
			{
				(*pStatusCallback)(c);
			});
	}
}


// If a command is available in the queue, then send it.
void CBMasterPort::SendNextMasterCommand()
{
	// We have to control access in the TCP callback, the send command, clear commands and the timeout callbacks.
	MasterCommandStrand->dispatch([&]()
		{
			UnprotectedSendNextMasterCommand(false);
		});
}
// There is a strand protected version of this command, as we can use it in another (already) protected area.
// We will retry the required number of times, and then move onto the next command. If a retry we send exactly the same command as previously, the Outstation  will
// check the sequence numbers on those commands that support it and reply accordingly.
void CBMasterPort::UnprotectedSendNextMasterCommand(bool timeoutoccured)
{
	if (!MasterCommandProtectedData.ProcessingCBCommand)
	{
		if (timeoutoccured)
		{
			// Do we need to resend a command?
			if (MasterCommandProtectedData.RetriesLeft-- > 0)
			{
				MasterCommandProtectedData.ProcessingCBCommand = true;
				LOGDEBUG("Sending Retry on command :" + std::to_string(MasterCommandProtectedData.CurrentFunctionCode))
			}
			else
			{
				// Have had multiple retries fail,

				// Execute the callback with a fail code.
				PostCallbackCall(MasterCommandProtectedData.CurrentCommand.second, CommandStatus::UNDEFINED);

				// so mark everything as if we have lost comms!
				pIOS->post([&]()
					{
						SetAllPointsQualityToCommsLost(); // All the connected points need their quality set to comms lost
					});

				LOGDEBUG("Reached maximum number of retries on command :" + std::to_string(MasterCommandProtectedData.CurrentFunctionCode))
			}
		}

		if (!MasterCommandProtectedData.MasterCommandQueue.empty() && (MasterCommandProtectedData.ProcessingCBCommand != true))
		{
			// Send the next command if there is one and we are not retrying.

			MasterCommandProtectedData.ProcessingCBCommand = true;
			MasterCommandProtectedData.RetriesLeft = MyPointConf->CBCommandRetries;

			MasterCommandProtectedData.CurrentCommand = MasterCommandProtectedData.MasterCommandQueue.front();
			MasterCommandProtectedData.MasterCommandQueue.pop();

			MasterCommandProtectedData.CurrentFunctionCode = MasterCommandProtectedData.CurrentCommand.first[0].GetFunctionCode();
			LOGDEBUG("Sending next command :" + std::to_string(MasterCommandProtectedData.CurrentFunctionCode))
		}

		// If either of the above situations need us to send a command, do so.
		if (MasterCommandProtectedData.ProcessingCBCommand == true)
		{
			SendCBMessage(MasterCommandProtectedData.CurrentCommand.first); // This should be the only place this is called for the CBMaster...

			// Start an async timed callback for a timeout - cancelled if we receive a good response.
			MasterCommandProtectedData.TimerExpireTime = std::chrono::milliseconds(MyPointConf->CBCommandTimeoutmsec);
			MasterCommandProtectedData.CurrentCommandTimeoutTimer->expires_from_now(MasterCommandProtectedData.TimerExpireTime);

			std::chrono::milliseconds endtime = MasterCommandProtectedData.TimerExpireTime;

			MasterCommandProtectedData.CurrentCommandTimeoutTimer->async_wait([&,endtime](asio::error_code err_code)
				{
					if (err_code != asio::error::operation_aborted)
					{
					// We need strand protection for the variables, so this will queue another chunk of work below.
					// If we get an answer in the delay this causes, no big deal - the length of the timeout will kind of jitter.
					      MasterCommandStrand->dispatch([&,endtime]()
							{
								// The checking of the expire time is another way to make sure that we have not cancelled the timer. We really need to make sure that if
								// we have cancelled the timer and this callback is called, that we do NOT take any action!
								if (endtime == MasterCommandProtectedData.TimerExpireTime)
								{
								      LOGDEBUG("CB Master Timeout valid - CB Function " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode));

								      MasterCommandProtectedData.ProcessingCBCommand = false; // Only gets reset on success or timeout.

								      UnprotectedSendNextMasterCommand(true); // We already have the strand, so don't need the wrapper here
								}
								else
									LOGDEBUG("CB Master Timeout callback called, when we had already moved on to the next command");
							});
					}
					else
					{
					      LOGDEBUG("CB Master Timeout callback cancelled");
					}
				});
		}
		else
		{
			// Clear current command?
		}
	}
}
// Strand protected clear the command queue.
void CBMasterPort::ClearCBCommandQueue()
{
	MasterCommandStrand->dispatch([&]()
		{
			CBMessage_t NextCommand;
			while (!MasterCommandProtectedData.MasterCommandQueue.empty())
			{
			      MasterCommandProtectedData.MasterCommandQueue.pop();
			}
			MasterCommandProtectedData.CurrentFunctionCode = 0;
			MasterCommandProtectedData.ProcessingCBCommand = false;
		});
}

#pragma endregion

#pragma region MessageProcessing

// After we have sent a command, this is where we will end up when the OutStation replies.
// It is the callback for the ConnectionManager
// If we get the wrong answer, we could ditch that command, and move on to the next one,
// but then the actual reply might be in the following message and we would never re-sync.
// If we timeout on (some) commands, we can ask the OutStation to resend the last command response.
// We would have to limit how many times we could do this without giving up.
void CBMasterPort::ProcessCBMessage(CBMessage_t &CompleteCBMessage)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.

	//! Anywhere we find that we don't have what we need, return. If we succeed we send the next command at the end of this method.
	// If the timeout on the command is activated, then the next command will be sent - or resent.
	// Cant just use by reference, complete message and header will go out of scope...
	MasterCommandStrand->dispatch([&, CompleteCBMessage]()
		{

			if (CompleteCBMessage.size() == 0)
			{
			      LOGERROR("Received a Master to Station message with zero length!!! ");
			      return;
			}

			CBBlockData Header = CompleteCBMessage[0];

			LOGDEBUG("CB Master received a response to sending cmd " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode)+" On Station Address - " + std::to_string(Header.GetStationAddress()));

			// If we have an error, we have to wait for the timeout to occur, there may be another packet in behind which is the correct one. If we bail now we may never re-synchronise.
			if (Header.GetStationAddress() == 0)
			{
			      LOGERROR("Received broadcast return message - address 0 - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}
			if (Header.GetStationAddress() != MyConf->mAddrConf.OutstationAddr)
			{
			      LOGERROR("Received a message from the wrong address - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}
			if (Header.GetFunctionCode() != MasterCommandProtectedData.CurrentFunctionCode)
			{
			      LOGERROR("Received a message with the wrong (non-matching) function code - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}

			bool success = false;
			bool NotImplemented = false;

			// Now based on the Command Function (Not the function code we got back!!), take action. Not all codes are expected or result in action
			switch (MasterCommandProtectedData.CurrentFunctionCode)
			{
				case FUNC_SCAN_DATA:
					success = ProcessScanRequestReturn(CompleteCBMessage); // Fn - 0
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
					LOGERROR("Unknown Message Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
					break;
			}

			if (NotImplemented == true)
			{
			      LOGERROR("Command Function NOT Implemented - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
			}

			if (success) // Move to the next command. Only other place we do this is in the timeout.
			{
			      MasterCommandProtectedData.CurrentCommandTimeoutTimer->cancel(); // Have to be careful the handler still might do something?
			      MasterCommandProtectedData.ProcessingCBCommand = false;          // Only gets reset on success or timeout.

			      // Execute the callback with a success code.
			      PostCallbackCall(MasterCommandProtectedData.CurrentCommand.second, CommandStatus::SUCCESS); // Does null check
			      UnprotectedSendNextMasterCommand(false);                                                    // We already have the strand, so don't need the wrapper here. Pass in that this is not a retry.
			}
			else
			{
			      LOGERROR("Command Response failed - Received - " + std::to_string(Header.GetFunctionCode()) +
					" Expecting " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
			}
			#pragma warning(suppress: 26495)
		});
}


// We have received data from a Scan command. Now decode...
// Return success or failure
bool CBMasterPort::ProcessScanRequestReturn(const CBMessage_t& CompleteCBMessage)
{
	uint8_t Group = CompleteCBMessage[0].GetGroup();

	uint32_t NumberOfBlocks = CompleteCBMessage.size();

	LOGDEBUG("Scan Data processing ");

	// For each of the payloads, find the matching points, save the values and trigger the Events.
	// There is always Block 1, payload B no matter what.

	auto payloadlocation = PayloadLocationType(1, PayloadABType::PositionB);

	ProccessScanPayload(CompleteCBMessage[0].GetB(), Group,  payloadlocation);

	for (uint32_t blockindex = 1; blockindex < NumberOfBlocks; blockindex++)
	{
		payloadlocation = PayloadLocationType(blockindex + 1, PayloadABType::PositionA);

		ProccessScanPayload(CompleteCBMessage[blockindex].GetA(), Group, payloadlocation);

		payloadlocation = PayloadLocationType(blockindex + 1, PayloadABType::PositionB);

		ProccessScanPayload(CompleteCBMessage[blockindex].GetB(), Group, payloadlocation);
	}
	return true;
}
void CBMasterPort::ProccessScanPayload(uint16_t data, uint8_t group, PayloadLocationType payloadlocation)
{
	bool FoundMatch = false;
	uint16_t Payload = 0;
	CBTime now = CBNow();

	LOGDEBUG("MA - Group - "+ std::to_string(group) + "Processing Payload - " + payloadlocation.to_string() + " Value 0x" + to_hexstring(data));

	MyPointConf->PointTable.ForEachMatchingAnalogPoint(group, payloadlocation, [&](CBAnalogCounterPoint &pt)
		{
			// We have a matching point - there will be 1 or 2, set a flag to indicate we have a match.

			uint16_t analogvalue = data;
			if (pt.GetPointType() == ANA6)
			{
			      if (pt.GetChannel() == 1)
					analogvalue = (data >> 6) & 0x3F; // Top 6 bits only.
			      else
					analogvalue &= 0x3F; // Bottom 6 bits only.
			}

			pt.SetAnalog(analogvalue, now);

			uint32_t ODCIndex = pt.GetIndex();
			QualityFlags qual = QualityFlags::ONLINE; // CalculateAnalogQuality(enabled, data, now); //TODO: Handle quality better?

			LOGDEBUG("MA - Published Event - Analog - Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(data));

			auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible);
			event->SetPayload<EventType::Analog>(std::move(data));
			PublishEvent(event);

			FoundMatch = true;
		});

	if (!FoundMatch)
	{
		MyPointConf->PointTable.ForEachMatchingCounterPoint(group, payloadlocation, [&](CBAnalogCounterPoint &pt)
			{
				// We have a matching point - there will be only 1!!, set a flag to indicate we have a match.
				pt.SetAnalog(data, now);

				uint32_t ODCIndex = pt.GetIndex();
				QualityFlags qual = QualityFlags::ONLINE; // CalculateAnalogQuality(enabled, data, now); //TODO: Handle quality better?

				LOGDEBUG("MA - Published Event - Counter - Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(data));

				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible);
				event->SetPayload<EventType::Counter>(std::move(data));
				PublishEvent(event);

				FoundMatch = true;
			});
	}
	if (!FoundMatch)
	{
		MyPointConf->PointTable.ForEachMatchingBinaryPoint(group, payloadlocation, [&](CBBinaryPoint &pt)
			{
				// We have a matching point, set a flag to indicate we have a match, save and trigger an event
				if (pt.GetPointType() == DIG)
				{
				      uint8_t ch = pt.GetChannel();
				      uint8_t bitvalue = (data >> (12 - ch)) & 0x0001;

				// Only process if the value has changed
				      if ((pt.GetBinary() != bitvalue) || (pt.GetHasBeenSet() == false))
				      {
				            pt.SetBinary(bitvalue, now); // Sets the has been set flag!

				            uint32_t ODCIndex = pt.GetIndex();

				            QualityFlags qual = QualityFlags::ONLINE; // CalculateBinaryQuality(enabled, now); //TODO: Handle quality better?
				            LOGDEBUG("Published Event - Binary Index " + std::to_string(ODCIndex) + " Value " + std::to_string(bitvalue));
				            auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now));
				            event->SetPayload<EventType::Binary>(bitvalue == 1);
				            PublishEvent(event);
					}
				      FoundMatch = true;
				}
				else
					//TODO: Works only for DIG type, need other processing for other Digital types.
					LOGERROR("We received an unhandled digital point type. Only deal with DIG type for now - Group " + std::to_string(group) + " Payload Location " + payloadlocation.to_string());
			});
	}
	if (!FoundMatch)
	{
		// See if it is a StatusByte we need to handle - there is only one status byte, but it could be requested in several groups. So deal with it whenever it comes back
		MyPointConf->PointTable.ForEachMatchingStatusByte(group, payloadlocation, [&](void)
			{
				// We have a matching status byte, set a flag to indicate we have a match.
				LOGDEBUG("Received a Status Byte at : " + std::to_string(group) + " - " + payloadlocation.to_string());
				//TODO: Not sure what we are going to do with the status byte - YET
				FoundMatch = true;
			});
	}
	if (!FoundMatch)
	{
		LOGDEBUG("Failed to find a payload for: " + payloadlocation.to_string() + " Setting to zero");
	}
}

/*
// Fn 6. This is only one of three possible responses to the Fn 6 command from the Master. The others are 5 (Unconditional) and 13 (No change)
// The data that comes back is up to 16 8 bit signed values representing the change from the last sent value.
// If there was a fault at the OutStation an Unconditional response will be sent instead.
// If a channel is missing or in fault, the value will be 0, so it can still just be added to the value (even if it is MISSINGVALUE - the fault value).
// Return success or failure
bool CBMasterPort::ProcessAnalogDeltaScanReturn(const CBMessage_t& CompleteCBMessage)
{
      uint8_t Group = Header.GetModuleAddress();
      uint8_t Channels = Header.GetChannels();

      uint8_t NumberOfDataBlocks = Channels / 4 + Channels % 4; // 2 --> 1, 5 -->2

      if (NumberOfDataBlocks != CompleteCBMessage.size() - 1)
      {
            LOGERROR("MA - Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) +
                  " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Doing Analog Delta processing ");

      // Unload the analog delta values from the blocks - 4 per block.
      std::vector<int8_t> AnalogDeltaValues;
      int ChanCount = 0;
      for (uint8_t blockindex = 0; blockindex < NumberOfDataBlocks; blockindex++)
      {
            for (uint8_t j = 0; j < 4; j++)
            {
                  AnalogDeltaValues.push_back(numeric_cast<char>(CompleteCBMessage[blockindex + 1].GetByte(j)));
                  ChanCount++;

                  // The last block may only have one reading in it. The 1,2,3 bytes might be filler.
                  if (ChanCount >= Channels)
                  {
                        // Reached as far as we need to go.
                        break;
                  }
            }
      }

      // Now take the returned values and add them to the points
      uint16_t wordres = 0;
      bool hasbeenset;

      // Search to see if the first value is a counter or analog
      bool FirstModuleIsCounterModule = MyPointConf->PointTable.GetCounterValueUsingCBIndex(Group, 0, wordres,hasbeenset);
      CBTime now = CBNow();

      for (uint8_t blockindex = 0; blockindex < Channels; blockindex++)
      {
            // Code to adjust the Group and index if the first module is a counter module (8 channels)
            // 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
            // We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
            uint8_t idx = FirstModuleIsCounterModule ? blockindex % 8 : blockindex;
            uint16_t maddress = (FirstModuleIsCounterModule && blockindex > 8) ? Group + 1 : Group;

            if (MyPointConf->PointTable.GetAnalogValueUsingCBIndex(maddress, idx, wordres,hasbeenset))
            {
                  wordres += AnalogDeltaValues[blockindex]; // Add the signed delta.
                  MyPointConf->PointTable.SetAnalogValueUsingCBIndex(maddress, idx, wordres);

                  LOGDEBUG("MA - Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

                  size_t ODCIndex;
                  if (MyPointConf->PointTable.GetAnalogODCIndexUsingCBIndex(maddress, idx, ODCIndex))
                  {
                        QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
                        LOGDEBUG("MA - Published Event - Analog Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
                        auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible
                        event->SetPayload<EventType::Analog>(std::move(wordres));
                        PublishEvent(event);
                  }
            }
            else if (MyPointConf->PointTable.GetCounterValueUsingCBIndex(maddress, idx,wordres,hasbeenset))
            {
                  wordres += AnalogDeltaValues[blockindex]; // Add the signed delta.
                  MyPointConf->PointTable.SetCounterValueUsingCBIndex(maddress, idx, wordres);

                  LOGDEBUG("MA - Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

                  size_t ODCIndex;
                  if (MyPointConf->PointTable.GetCounterODCIndexUsingCBIndex(maddress, idx, ODCIndex))
                  {
                        QualityFlags qual = CalculateAnalogQuality(enabled,wordres, now);
                        LOGDEBUG("MA - Published Event - Counter Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
                        auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible);
                        event->SetPayload<EventType::Counter>(std::move(wordres));
                        PublishEvent(event);
                  }
            }
            else
            {
                  LOGERROR("Fn6 Failed to set an Analog or Counter Value - " + std::to_string(Header.GetFunctionCode())
                        + " On Station Address - " + std::to_string(Header.GetStationAddress())
                        + " Module : " + std::to_string(maddress) + " Channel : " + std::to_string(idx));
                  return false;
            }
      }
      return true;
}

// We have received data from an Analog command - could be  the result of Fn 5 or 6
bool CBMasterPort::ProcessAnalogNoChangeReturn(const CBMessage_t& CompleteCBMessage)
{
      uint8_t Group = Header.GetModuleAddress();
      uint8_t Channels = Header.GetChannels();

      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received an analog no change with extra data - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Doing Analog NoChange processing ");

      // Now take the returned values and store them into the points
      uint16_t wordres = 0;
      bool hasbeenset;

      // ANALOG_NO_CHANGE_REPLY do we update the times on the points that we asked to be updated?
      if (MyPointConf->UpdateAnalogCounterTimeStamps)
      {
            // Search to see if the first value is a counter or analog
            bool FirstModuleIsCounterModule = MyPointConf->PointTable.GetCounterValueUsingCBIndex(Group, 0, wordres, hasbeenset);

            CBTime now = CBNow();

            for (uint8_t blockindex = 0; blockindex < Channels; blockindex++)
            {
                  // Code to adjust the Group and index if the first module is a counter module (8 channels)
                  // 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
                  // We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
                  uint8_t idx = FirstModuleIsCounterModule ? blockindex % 8 : blockindex;
                  uint8_t maddress = (FirstModuleIsCounterModule && blockindex > 8) ? Group + 1 : Group;

                  if (MyPointConf->PointTable.GetAnalogValueUsingCBIndex(maddress, idx, wordres, hasbeenset))
                  {
                        MyPointConf->PointTable.SetAnalogValueUsingCBIndex(maddress, idx, wordres);

                        LOGDEBUG("MA - Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

                        size_t ODCIndex;
                        if (MyPointConf->PointTable.GetAnalogODCIndexUsingCBIndex(maddress, idx, ODCIndex))
                        {
                              QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
                              LOGDEBUG("MA - Published Event - Analog Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
                              auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible
                              event->SetPayload<EventType::Analog>(std::move(wordres));
                              PublishEvent(event);
                        }
                  }
                  else if (MyPointConf->PointTable.GetCounterValueUsingCBIndex(maddress, idx, wordres, hasbeenset))
                  {
                        // Get the value and set it again, but with a new time.
                        MyPointConf->PointTable.SetCounterValueUsingCBIndex(maddress, idx, wordres); // This updates the time
                        LOGDEBUG("MA - Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

                        size_t ODCIndex;
                        if (MyPointConf->PointTable.GetCounterODCIndexUsingCBIndex(maddress, idx, ODCIndex))
                        {
                              QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
                              LOGDEBUG("MA - Published Event - Counter Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
                              auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible);
                              event->SetPayload<EventType::Counter>(std::move(wordres));
                              PublishEvent(event);
                        }
                  }
                  else
                  {
                        LOGERROR("Fn5 Failed to set an Analog or Counter Time - " + std::to_string(Header.GetFunctionCode())
                              + " On Station Address - " + std::to_string(Header.GetStationAddress())
                              + " Module : " + std::to_string(maddress) + " Channel : " + std::to_string(idx));
                        return false;
                  }
            }
      }
      return true;
}

bool CBMasterPort::ProcessDigitalNoChangeReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received a digital no change with extra data - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Doing Digital NoChange processing... ");

      // DIGITAL_NO_CHANGE_REPLY we do not update time on digital points

      return true;
}
bool CBMasterPort::ProcessDigitalScan(const CBMessage_t& CompleteCBMessage)
{
      CBBlockFn11StoM Fn11Header = static_cast<CBBlockFn11StoM>(Header);

      uint8_t EventCnt = Fn11Header.GetTaggedEventCount();
      uint8_t SeqNum = Fn11Header.GetDigitalSequenceNumber();
      uint8_t ModuleCount = Fn11Header.GetModuleCount();
      size_t MessageIndex = 1; // What block are we processing?
      LOGDEBUG("Digital Scan (new) processing EventCnt : "+std::to_string(EventCnt)+" ModuleCnt : "+std::to_string(ModuleCount)+" Sequence Number : " + std::to_string(SeqNum));
      LOGDEBUG("Digital Scan Data " + CBMessageAsString(CompleteCBMessage));

      //TODO: We may have to reorder the processing of the "now" module data, and the time tagged data, so that the ODC events go through in chronological order,
      //TODO: Run a test on MOSAIC to see what it does with out of order binary events...

      // Now take the returned values and store them into the points
      // Process any module data (if any)
      //NOTE: The module data on initial scan might have two blocks for the one address - the previous and current state????
      for (int m = 0; m < ModuleCount; m++)
      {
            if (MessageIndex < CompleteCBMessage.size())
            {
                  // The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
                  uint8_t Group = CompleteCBMessage[MessageIndex].GetByte(0);
                  uint8_t msecOffset = CompleteCBMessage[MessageIndex].GetByte(1); // Will always be 0 for Module blocks
                  uint16_t data = CompleteCBMessage[MessageIndex].GetSecondWord();

                  if (Group == 0 && msecOffset == 0)
                  {
                        // This is a status or if module address is 0 a flag block.
                        Group = CompleteCBMessage[MessageIndex].GetByte(2);
                        uint8_t ModuleFailStatus = CompleteCBMessage[MessageIndex].GetByte(3);
                        LOGDEBUG("Received a Fn 11 Status or Flag Block (addr=0) - We do not process - Module Address : " + std::to_string(Group) + " Fail Status : 0x" + to_hexstring(ModuleFailStatus));
                  }
                  else
                  {
                        LOGDEBUG("Received a Fn 11 Data Block - Module : " + std::to_string(Group) + " Data : 0x" + to_hexstring(data) + " Data : " + to_binstring(data));
                        CBTime eventtime = CBNow();

                        GenerateODCEventsFromDIGPayload(data, Group, eventtime);
                  }
            }
            else
            {
                  LOGERROR("Tried to read a block past the end of the message processing module data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteCBMessage.size()));
                  return false;
            }
            MessageIndex++;
      }

      if (EventCnt > 0) // Process Events (if any)
      {
            CBTime timebase = 0; // Moving time value for events received

            // Process Time/Date Data
            if (MessageIndex < CompleteCBMessage.size())
            {
                  timebase = static_cast<uint64_t>(CompleteCBMessage[MessageIndex].GetData()) * 1000; //CBTime msec since Epoch.
                  LOGDEBUG("Fn11 TimeDate Packet Local : " + to_timestringfromCBtime(timebase));
            }
            else
            {
                  LOGERROR("Tried to read a block past the end of the message processing time date data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteCBMessage.size()));
                  return false;
            }
            MessageIndex++;

            // Now we have to convert the remaining data blocks into an array of words and process them. This is due to the time offset blocks which are only 16 bits long.
            std::vector<uint16_t> ResponseWords;
            while (MessageIndex < CompleteCBMessage.size())
            {
                  ResponseWords.push_back(CompleteCBMessage[MessageIndex].GetFirstWord());
                  ResponseWords.push_back(CompleteCBMessage[MessageIndex].GetSecondWord());
                  MessageIndex++;
            }

            // Now process the response words.
            for (size_t blockindex = 0; blockindex < ResponseWords.size(); blockindex++)
            {
                  // If we are processing a data block and the high byte will be non-zero.
                  // If it is zero it could be either:
                  // A time offset word or
                  // If the whole word is zero, then it must be the first word of a Status block.

                  if (ResponseWords[blockindex] == 0)
                  {
                        // We have received a STATUS block, which has a following word.
                        blockindex++;
                        if (blockindex >= ResponseWords.size())
                        {
                              // We likely received an all zero padding block at the end of the message. Ignore this as an error
                              LOGDEBUG("Fn11 Zero padding end word detected - ignoring");
                              return true;
                        }
                        // This is a status or if module address is 0 a flag block.
                        uint8_t Group = (ResponseWords[blockindex+1] >> 8) & 0x0FF;
                        uint8_t ModuleFailStatus = ResponseWords[blockindex + 1] & 0x0FF;
                        LOGDEBUG("Received a Fn 11 Status or Flag Block (addr=0) - We do not process - Module Address : " + std::to_string(Group) + " Fail Status : 0x" + to_hexstring(ModuleFailStatus));
                  }
                  else if ((ResponseWords[blockindex] & 0xFF00) == 0)
                  {
                        // We have received a TIME BLOCK (offset) which is a single word.
                        uint16_t msecoffset = (ResponseWords[blockindex] & 0x00ff) * 256;
                        timebase += msecoffset;
                        LOGDEBUG("Fn11 TimeOffset : " + std::to_string(msecoffset) +" msec");
                  }
                  else
                  {
                        // We have received a DATA BLOCK which has a following word.
                        // The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
                        uint8_t Group = (ResponseWords[blockindex] >> 8) & 0x007f;
                        uint8_t msecoffset = ResponseWords[blockindex] & 0x00ff;
                        timebase += msecoffset; // Update the current tagged time
                        blockindex++;

                        if (blockindex >= ResponseWords.size())
                        {
                              // Index error
                              LOGERROR("Tried to access past the end of the response words looking for the second part of a data block " + CBMessageAsString(CompleteCBMessage));
                              return false;
                        }
                        uint16_t data = ResponseWords[blockindex];

                        LOGDEBUG("Fn11 TimeTagged Block - Module : " + std::to_string(Group) + " msec offset : " + std::to_string(msecoffset) + " Data : 0x" + to_hexstring(data));
                        GenerateODCEventsFromDIGPayload(data, Group,timebase);
                  }
            }
      }
      return true;
}
void CBMasterPort::GenerateODCEventsFromDIGPayload(const uint16_t &data, const uint8_t &Group, const CBTime &eventtime)
{
      LOGDEBUG("Master Generate Events,  Module : "+std::to_string(Group)+" Data : 0x" + to_hexstring(data));

      for (uint8_t idx = 0; idx < 16; idx++)
      {
            // When we set the value it tells us if we really did set the value, or it was already at that value.
            // Only fire the ODC event if the value changed.
            bool valuechanged = false;
            uint8_t bitvalue = (data >> (15 - idx)) & 0x0001;

            bool res = MyPointConf->PointTable.SetBinaryValueUsingCBIndex(Group, idx, bitvalue, valuechanged);

            if (res && valuechanged)
            {
                  size_t ODCIndex = 0;
                  if (MyPointConf->PointTable.GetBinaryODCIndexUsingCBIndex(Group, idx, ODCIndex))
                  {
                        QualityFlags qual = CalculateBinaryQuality(enabled, eventtime);
                        LOGDEBUG("Published Event - Binary Index " + std::to_string(ODCIndex) + " Value " + std::to_string(bitvalue));
                        auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(eventtime));
                        event->SetPayload<EventType::Binary>(bitvalue == 1);
                        PublishEvent(event);
                  }
                  else
                  {
                        // The point did not exist in the table
                  }
            }
      }
}
bool CBMasterPort::ProcessDOMReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Master Received an DOM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Master Got DOM response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool CBMasterPort::ProcessPOMReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Master Received an POM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Master Got POM response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool CBMasterPort::ProcessAOMReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Master Received an AOM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Master Got AOM response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool CBMasterPort::ProcessSetDateTimeReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received an SetDateTime response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Got SetDateTime response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool CBMasterPort::ProcessSetDateTimeNewReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received an SetDateTimeNew response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Got SetDateTimeNew response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool CBMasterPort::ProcessSystemSignOnReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received an SystemSignOn response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }
      CBBlockData resp(Header);
      if (!resp.IsValid())
      {
            LOGERROR("Received an SystemSignOn response that was not valid - ignoring - On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }
      LOGDEBUG("Got SystemSignOn response ");

      return (Header.GetFunctionCode() == SYSTEM_SIGNON_CONTROL);
}
bool CBMasterPort::ProcessFreezeResetReturn(const CBMessage_t& CompleteCBMessage)
{
      if (CompleteCBMessage.size() != 1)
      {
            LOGERROR("Received an Freeze-Reset response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
            return false;
      }

      LOGDEBUG("Got Freeze-Reset response ");

      return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool CBMasterPort::ProcessFlagScanReturn(const CBMessage_t& CompleteCBMessage)
{
      // The contents of the flag scan return can be contract dependent. There can be more than the single (default) block returned...
      // We are only looking at the standard megadata flags here SPU, STI and FUP
      // SPU - Bit 7, MegaDataFlags below - System Powered Up
      // STI - Bit 6, MegaDataFlags below - System Time Incorrect
      // FUP - Bit 5, MegaDataFlags below - File upload pending

      // If any bit in the 16 bit status word changes (in the RTU), then the RSF flag bit (present in some messages) will be set.
      // Executing a FlagScan will reset this bit in the RTU, so will then be 0 in those messages.

      // Use the block access method to get at the bits.
      CBBlockFn52StoM Header52(Header);

      bool SystemPoweredUpFlag = Header52.GetSystemPoweredUpFlag();
      bool SystemTimeIncorrectFlag = Header52.GetSystemTimeIncorrectFlag();

      LOGDEBUG("Got Flag Scan response, " + std::to_string(CompleteCBMessage.size()) + " blocks, System Powered Up Flag : " + std::to_string(SystemPoweredUpFlag) + ", System TimeIncorrect Flag : " + std::to_string(SystemTimeIncorrectFlag));

      if (SystemTimeIncorrectFlag)
      {
            // Find if there is a poll group time set command, if so, do that poll (send the time)
            for (auto pg : MyPointConf->PollGroups)
            {
                  if (pg.second.polltype == TimeSetCommand)
                  {
                        this->DoPoll(pg.second.ID);
                  }
                  ;
            }
      }

      if (SystemPoweredUpFlag)
      {
            // Launch all digital polls/scans
            for (auto pg : MyPointConf->PollGroups)
            {
                  if (pg.second.polltype == BinaryPoints)
                  {
                        this->DoPoll(pg.second.ID);
                  }
                  ;
            }

            // Launch all analog polls/scans
            for (auto pg : MyPointConf->PollGroups)
            {
                  if (pg.second.polltype == AnalogPoints)
                  {
                        this->DoPoll(pg.second.ID);
                  }
                  ;
            }
      }
      return (Header.GetFunctionCode() == SYSTEM_FLAG_SCAN);
}
*/
#pragma endregion

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void CBMasterPort::DoPoll(uint32_t payloadlocation)
{
	if (!enabled) return;
	LOGDEBUG("DoPoll : " + std::to_string(payloadlocation));

	switch (MyPointConf->PollGroups[payloadlocation].polltype)
	{
		case AnalogPoints:
		{
			ModuleMapType::iterator mait = MyPointConf->PollGroups[payloadlocation].ModuleAddresses.begin();

			// We will scan a maximum of 1 module, up to 16 channels. It might spill over into the next module if the module is a counter with only 8 channels.
			uint8_t Group = mait->first;
			uint8_t Channels = numeric_cast<uint8_t>(mait->second);

			if (MyPointConf->PollGroups[payloadlocation].ModuleAddresses.size() > 1)
			{
				LOGERROR("Analog Poll group " + std::to_string(payloadlocation) + " is configured for more than one CB address. Please create another poll group.");
			}

			// We need to do an analog unconditional on start up, until all the points have a valid value - even MISSINGVALUE for does not exist.
			// To do this we check the hasbeenset flag, which will be false on start up, and also set to false on comms lost event - kind of like a quality.
			bool UnconditionalCommandRequired = false;
			for (uint8_t idx = 0; idx < Channels; idx++)
			{
				uint16_t wordres;
				bool hasbeenset;
				bool res = MyPointConf->PointTable.GetAnalogValueUsingCBIndex(Group, idx, wordres, hasbeenset);
				if (res && !hasbeenset)
					UnconditionalCommandRequired = true;
			}

			if (UnconditionalCommandRequired || MyPointConf->PollGroups[payloadlocation].ForceUnconditional)
			{
				// Use Unconditional Request Fn 5
				LOGDEBUG("Poll Issued a Analog Unconditional Command");

//TODO:				CBBlockData commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL, Group, Channels, true);
//				QueueCBCommand(commandblock, nullptr);
			}
			else
			{
				LOGDEBUG("Poll Issued a Analog Delta Command");
				// Use a delta command Fn 6
//				CBBlockData commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_DELTA_SCAN, Group, Channels, true);
//				QueueCBCommand(commandblock, nullptr);
			}
		}
		break;

		case BinaryPoints:
		{

			// If sequence number is zero - it means we have just started up, or communications re-established. So we dont have a full copy
			// of the binary data (timetagged or otherwise). The outStation will use the zero sequnce number to send everything to initialise us. We
			// don't have to send an unconditional.

			ModuleMapType::iterator FirstModule = MyPointConf->PollGroups[payloadlocation].ModuleAddresses.begin();

			// Request Digital Unconditional
			uint8_t Group = FirstModule->first;
			// We expect the digital modules to be consecutive, or of there is a gap this will still work.
			uint8_t Modules = numeric_cast<uint8_t>(MyPointConf->PollGroups[payloadlocation].ModuleAddresses.size()); // Most modules we can get in one command - NOT channels!

			bool UnconditionalCommandRequired = false;
			for (uint8_t m = 0; m < Modules; m++)
			{
				for (uint8_t idx = 0; idx < 16; idx++)
				{
					bool hasbeenset;
					bool res = MyPointConf->PointTable.GetBinaryQualityUsingCBIndex(Group + m, idx, hasbeenset);
					if (res && !hasbeenset)
						UnconditionalCommandRequired = true;
				}
			}
			//				CBBlockData commandblock;

			// Also need to check if we already have all the values that this command would ask for..if not send unconditional.
			if (UnconditionalCommandRequired || MyPointConf->PollGroups[payloadlocation].ForceUnconditional)
			{
				// Use Unconditional Request Fn 12
				LOGDEBUG("Poll Issued a Digital Unconditional (new) Command");

				//					commandblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, Group, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
			}
			else
			{
				// Use a delta command Fn 11
				LOGDEBUG("Poll Issued a Digital Delta COS (new) Command");

				uint8_t TaggedEventCount = 0; // Assuming no timetagged points initially.

				// If we have timetagged points in the system, then we need to ask for them to be returned.
				if (MyPointConf->PollGroups[payloadlocation].TimeTaggedDigital == true)
					TaggedEventCount = 15; // The most we can ask for

//					commandblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, TaggedEventCount, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
			}

			//				QueueCBCommand(commandblock, nullptr); // No callback, does not originate from ODC
		}
		break;

		case  TimeSetCommand:
		{
			// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
			uint64_t currenttime = CBNow();

			LOGDEBUG("Poll Issued a TimeDate Command");
//			SendTimeDateChangeCommand(currenttime, nullptr);
		}
		break;

		case NewTimeSetCommand:
		{
			// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
			uint64_t currenttime = CBNow();

			LOGDEBUG("Poll Issued a NewTimeDate Command");
			int utcoffsetminutes = tz_offset();

//			SendNewTimeDateChangeCommand(currenttime, utcoffsetminutes, nullptr);
		}
		break;

		case SystemFlagScan:
		{
			// Send a flag scan command to the OutStation, (Fn 52)
			LOGDEBUG("Poll Issued a System Flag Scan Command");
//			SendSystemFlagScanCommand(nullptr);
		}
		break;

		default:
			LOGDEBUG("Poll will an unknown polltype : " + std::to_string(MyPointConf->PollGroups[payloadlocation].polltype));
	}
}

void CBMasterPort::ResetDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);
	DigitalCommandSequenceNumber = 0;
}
// Manage the access to the command sequence number. Very low possibility of contention, so use standard lock.
uint8_t CBMasterPort::GetAndIncrementDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);

	uint8_t retval = DigitalCommandSequenceNumber;

	if (DigitalCommandSequenceNumber == 0) DigitalCommandSequenceNumber++; // If we send 0, we will get a sequence number of 1 back. So need to move on to 2 for next command

	DigitalCommandSequenceNumber = (++DigitalCommandSequenceNumber > 15) ? 1 : DigitalCommandSequenceNumber;
	return retval;
}
void CBMasterPort::EnablePolling(bool on)
{
	if (on)
		PollScheduler->Start();
	else
		PollScheduler->Stop();
}
/*
void CBMasterPort::SendTimeDateChangeCommand(const uint64_t &currenttimeinmsec, SharedStatusCallback_t pStatusCallback)
{
      CBBlockData commandblock(MyConf->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
      CBBlockData datablock(static_cast<uint32_t>(currenttimeinmsec / 1000), true);
      CBMessage_t Cmd;
      Cmd.push_back(commandblock);
      Cmd.push_back(datablock);
      QueueCBCommand(Cmd, pStatusCallback);
}
void CBMasterPort::SendNewTimeDateChangeCommand(const uint64_t &currenttimeinmsec, int utcoffsetminutes, SharedStatusCallback_t pStatusCallback)
{
      CBBlockData commandblock(MyConf->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
      CBBlockData datablock(static_cast<uint32_t>(currenttimeinmsec / 1000));
      CBBlockData datablock2(static_cast<uint32_t>(utcoffsetminutes) << 16, true);
      CBMessage_t Cmd;
      Cmd.push_back(commandblock);
      Cmd.push_back(datablock);
      Cmd.push_back(datablock2);
      QueueCBCommand(Cmd, pStatusCallback);
}
void CBMasterPort::SendSystemFlagScanCommand(SharedStatusCallback_t pStatusCallback)
{
      CBBlockData commandblock(MyConf->mAddrConf.OutstationAddr);
      QueueCBCommand(commandblock, pStatusCallback);
}
*/
void CBMasterPort::SetAllPointsQualityToCommsLost()
{
	LOGDEBUG("CB Master setting quality to comms lost");

	auto eventbinary = std::make_shared<EventInfo>(EventType::BinaryQuality, 0, Name, QualityFlags::COMM_LOST);
	eventbinary->SetPayload<EventType::BinaryQuality>(QualityFlags::COMM_LOST);

	// Loop through all Binary points.
	MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			eventbinary->SetIndex(index);
			PublishEvent(eventbinary);
			Point.SetChangedFlag();
		});

	// Analogs

	auto eventanalog = std::make_shared<EventInfo>(EventType::AnalogQuality, 0, Name, QualityFlags::COMM_LOST);
	eventanalog->SetPayload<EventType::AnalogQuality>(QualityFlags::COMM_LOST);
	MyPointConf->PointTable.ForEachAnalogPoint([&](CBAnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			if (!MyPointConf->PointTable.ResetAnalogValueUsingODCIndex(index)) // Sets to MISSINGVALUE, time = 0, HasBeenSet to false
				LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));

			eventanalog->SetIndex(index);
			PublishEvent(eventanalog);
		});
	// Counters
	auto eventcounter = std::make_shared<EventInfo>(EventType::CounterQuality, 0, Name, QualityFlags::COMM_LOST);
	eventcounter->SetPayload<EventType::CounterQuality>(QualityFlags::COMM_LOST);

	MyPointConf->PointTable.ForEachCounterPoint([&](CBAnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			if (!MyPointConf->PointTable.ResetCounterValueUsingODCIndex(index)) // Sets to MISSINGVALUE, time = 0, HasBeenSet to false
				LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));

			eventcounter->SetIndex(index);
			PublishEvent(eventcounter);
		});
}

// When a new device connects to us through ODC (or an existing one reconnects), send them everything we currently have.
void CBMasterPort::SendAllPointEvents()
{
	// Quality of ONLINE means the data is GOOD.
	MyPointConf->PointTable.ForEachBinaryPoint([&](CBBinaryPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint8_t meas = Point.GetBinary();
			QualityFlags qual = CalculateBinaryQuality(enabled, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Binary, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Binary>(meas == 1);
			PublishEvent(event);
		});

	// Analogs
	MyPointConf->PointTable.ForEachAnalogPoint([&](CBAnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint16_t meas = Point.GetAnalog();

			// If the measurement is MISSINGVALUE - there is a problem in the CB OutStation for that point.
			QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Analog, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Analog>(std::move(meas));
			PublishEvent(event);
		});

	// Counters
	MyPointConf->PointTable.ForEachCounterPoint([&](CBAnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint16_t meas = Point.GetAnalog();
			// If the measurement is MISSINGVALUE - there is a problem in the CB OutStation for that point.
			QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Counter, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Counter>(std::move(meas));
			PublishEvent(event);
		});
}

// Binary quality only depends on our link status and if we have received data
QualityFlags CBMasterPort::CalculateBinaryQuality(bool enabled, CBTime time)
{
	//TODO: Change this to Quality being part of the point object
	return (enabled ? ((time == 0) ? QualityFlags::RESTART : QualityFlags::ONLINE) : QualityFlags::COMM_LOST);
}
// Use the measurement value and if we are enabled to determine what the quality value should be.
QualityFlags CBMasterPort::CalculateAnalogQuality(bool enabled, uint16_t meas, CBTime time)
{
	//TODO: Change this to Quality being part of the point object
	return (enabled ? (time == 0 ? QualityFlags::RESTART : ((meas == MISSINGVALUE) ? QualityFlags::LOCAL_FORCED : QualityFlags::ONLINE)) : QualityFlags::COMM_LOST);
}


// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
// Also we have the pass through commands with special port values defined.
void CBMasterPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
	switch (event->GetEventType())
	{
		case EventType::ControlRelayOutputBlock:
			return WriteObject(event->GetPayload<EventType::ControlRelayOutputBlock>(), numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::AnalogOutputInt16:
			return WriteObject(event->GetPayload<EventType::AnalogOutputInt16>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::AnalogOutputInt32:
			return WriteObject(event->GetPayload<EventType::AnalogOutputInt32>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::AnalogOutputFloat32:
			return WriteObject(event->GetPayload<EventType::AnalogOutputFloat32>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::AnalogOutputDouble64:
			return WriteObject(event->GetPayload<EventType::AnalogOutputDouble64>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();
			// This will be fired by (typically) an CBOutStation port on the "other" side of the ODC Event bus. blockindex.e. something upstream has connected
			// We should probably send all the points to the Outstation as we don't know what state the OutStation point table will be in.

			if (state == ConnectState::CONNECTED)
			{
				LOGDEBUG("Upstream (other side of ODC) port enabled - Triggering sending of current data ");
				// We don\92t know the state of the upstream data, so send event information for all points.
				SendAllPointEvents();
			}
			else if (state == ConnectState::DISCONNECTED)
			{
				// If we were an on demand connection, we would take down the connection . For CB we are using persistent connections only.
				// We have lost an ODC connection, so events we send don't go anywhere.
			}

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}
}


void CBMasterPort::WriteObject(const ControlRelayOutputBlock& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	uint8_t Group = 0;
	uint8_t Channel = 0;
	BinaryPointType PointType;
	bool exists = MyPointConf->PointTable.GetBinaryControlCBIndexUsingODCIndex(index, Group, Channel, PointType);

	if (!exists)
	{
		LOGDEBUG("Master received a DOM/POM ODC Change Command on a point that is not defined " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	std::string OnOffString = "ON";

	if (PointType == MCB)
	{
		// The main issue is that a DOM command writes 16 bits in one go, so we have to remember the output state of the other bits,
		// so that we only write the change that has been triggered.
		// We have to gather up the current state of those bits.

		bool ModuleFailed = false;                                                                    // Not used - yet
		uint16_t outputbits = MyPointConf->PointTable.CollectModuleBitsIntoWord(Group, ModuleFailed); // Reads from the digital input point list...

		if ((command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
		{
			// OFF Command
			outputbits &= ~(0x0001 << (15-Channel));
			OnOffString = "OFF";
		}
		else
		{
			// ON Command  --> ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
			outputbits |= (0x0001 << (15-Channel));
			OnOffString = "ON";
		}

		LOGDEBUG("Master received a DOM ODC Change Command - Index: " + std::to_string(index) +" - "+ OnOffString + "  Module/Channel " + std::to_string(Group) + "/" + std::to_string(Channel));
		SendDOMOutputCommand(MyConf->mAddrConf.OutstationAddr, Group, outputbits, pStatusCallback);
	}
	else if(PointType == MCC)
	{
		// POM Point
		// The POM output is a single output selection, value 0 to 15 which represents a TRIP 0-7 and CLOSE 8-15 for the up to 8 points in a POM module.
		// We don't have to remember state here as we are sending only one bit.
		uint8_t outputselection = 0;

		if ((command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
		{
			// OFF Command
			outputselection = Channel+8;
			OnOffString = "OFF";
		}
		else
		{
			// ON Command  --> ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
			outputselection = Channel;
			OnOffString = "ON";
		}

		LOGDEBUG("Master received a POM ODC Change Command - Index: " + std::to_string(index) + " - " + OnOffString + "  Module/Channel " + std::to_string(Group) + "/" + std::to_string(Channel));
//		SendPOMOutputCommand(MyConf->mAddrConf.OutstationAddr, Group, outputselection, pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received Binary Output ODC Event on a point not defined as DOM or POM " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
}

void CBMasterPort::WriteObject(const int16_t & command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// AOM Command
	LOGDEBUG("Master received a AOM ODC Change Command " + std::to_string(index));

//TODO: Finish AOM command	SendDOMOutputCommand(MyConf->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputFromSecondBlock(BlockData), pStatusCallback);

	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}


void CBMasterPort::WriteObject(const int32_t & command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// Other Magic point commands

	if (index == MyPointConf->POMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received POM Control Point command on the magic point through ODC " + std::to_string(index));

//		CBBlockData Header = CBBlockData(CBBlockData(numeric_cast<uint32_t>(command)));
//		SendPOMOutputCommand(MyConf->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputSelection(), pStatusCallback);
	}
	else if (index == MyPointConf->SystemSignOnPoint.second)
	{
		// Normally a Fn40
		LOGDEBUG("Master received System Sign On command on the magic point through ODC " + std::to_string(index));
//		CBBlockData CBcommand(numeric_cast<uint32_t>(command), true);
//		QueueCBCommand(CBcommand, pStatusCallback); // Single block send
	}
	else if (index == MyPointConf->FreezeResetCountersPoint.second)
	{
		// Normally a Fn16
		LOGDEBUG("Master received Freeze/Reset Counters command on the magic point through ODC " + std::to_string(index));
//		CBBlockData CBcommand(numeric_cast<uint32_t>(command),true); // The packet rx'd by the outstation and passed to us through ODC is sent out unchanged by the master...
//		QueueCBCommand(CBcommand, pStatusCallback);                       // Single block send
	}
	else if (index == MyPointConf->DOMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received DOM Control Point command on the magic point through ODC " + std::to_string(index));
		uint32_t PacketData = numeric_cast<uint32_t>(command);
//		CBBlockData Header = CBBlockData((PacketData >> 24) & 0x7F, (PacketData >> 16) & 0xFF);
//		CBBlockData BlockData = Header.GenerateSecondBlock(PacketData & 0xFFFF);

//
	}
	else
	{
		LOGDEBUG("Master received unknown AnalogOutputInt32 ODC Event " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
	}
}

void CBMasterPort::WriteObject(const float& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGERROR("On Master float Type is not implemented " + std::to_string(index));
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::WriteObject(const double& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	if (index == MyPointConf->TimeSetPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received a Time Change command on the magic point through ODC " + std::to_string(index));
//		uint64_t currenttime = static_cast<uint64_t>(command);

//		SendTimeDateChangeCommand(currenttime, pStatusCallback);
	}
	else if(index == MyPointConf->TimeSetPointNew.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received a New Time Change command on the magic point through ODC " + std::to_string(index));
//		uint64_t currenttime = static_cast<uint64_t>(command);
//		int utcoffsetminutes = tz_offset();
//		SendNewTimeDateChangeCommand(currenttime, utcoffsetminutes, pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received unknown double ODC Event " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
	}
}

void CBMasterPort::SendDOMOutputCommand(const uint8_t &StationAddress, const uint8_t &Group, const uint16_t &outputbits, const SharedStatusCallback_t &pStatusCallback)
{
//	CBBlockData commandblock(StationAddress, Group );
//	CBBlockData datablock = commandblock.GenerateSecondBlock(outputbits);
	CBMessage_t Cmd;
//	Cmd.push_back(commandblock);
//	Cmd.push_back(datablock);
	QueueCBCommand(Cmd, pStatusCallback);
}




