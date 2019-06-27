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

	LOGDEBUG("CBMaster Constructor - {} - {} Overrides - {}",aName, aConfFilename, over);
}

CBMasterPort::~CBMasterPort()
{
	Disable();
	CBConnection::RemoveMaster(pConnection,MyConf->mAddrConf.OutstationAddr);
}

void CBMasterPort::Enable()
{
	if (enabled) return;
	try
	{
		CBConnection::Open(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop

		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection TCP : {} : {}",Name, e.what());
		return;
	}
}
void CBMasterPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	CBConnection::Close(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
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
	if (PollScheduler == nullptr)
		PollScheduler.reset(new ASIOScheduler(*pIOS));

	MasterCommandProtectedData.CurrentCommandTimeoutTimer.reset(new Timer_t(*pIOS));
	MasterCommandStrand.reset(new asio::io_context::strand(*pIOS));

	// Need a couple of things passed to the point table.
	MyPointConf->PointTable.Build(IsOutStation, *pIOS);

	// Creates internally if necessary, returns a token for the connection
	pConnection = CBConnection::AddConnection(pIOS, IsServer(), MyConf->mAddrConf.IP,MyConf->mAddrConf.Port, MyPointConf->IsBakerDevice, MyConf->mAddrConf.TCPConnectRetryPeriodms); //Static method

	CBConnection::AddMaster(pConnection,MyConf->mAddrConf.OutstationAddr,
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
	LOGDEBUG("MA - Sending Message - {}", CBMessageAsString(CompleteCBMessage));

	// Done this way just to get context into log messages.
	CBPort::SendCBMessage(CompleteCBMessage);
}

#ifdef _MSC_VER
#pragma region MasterCommandQueue
#endif

// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
// There is a fixed timeout function (below) which will queue the next command if we timeout.
// If the ProcessCBMessage callback gets the command it expects, it will send the next command in the queue.
// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
void CBMasterPort::QueueCBCommand(const CBMessage_t &CompleteCBMessage, SharedStatusCallback_t pStatusCallback)
{
	MasterCommandStrand->dispatch([=]() // Tries to execute, if not able to will post.
		{
			if (MasterCommandProtectedData.MasterCommandQueue.size() < MasterCommandProtectedData.MaxCommandQueueSize)
			{
			      MasterCommandProtectedData.MasterCommandQueue.push(MasterCommandQueueItem(CompleteCBMessage, pStatusCallback)); // async
			}
			else
			{
			      LOGDEBUG("Tried to queue another CB Master PendingCommand when the command queue is full");
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

CBMessage_t CBMasterPort::GetResendMessage()
{
	CBMessage_t CompleteCBMessage;
	auto firstblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, MASTER_SUB_FUNC_REPEAT_PREVIOUS_TRANSMISSION, FUNC_MASTER_STATION_REQUEST, 0, true);
	CompleteCBMessage.push_back(firstblock);
	return CompleteCBMessage;
}
// There is a strand protected version of this command, as we can use it in another (already) protected area.
// We will retry the required number of times, and then move onto the next command. If a retry we send exactly the same command as previously, the Outstation  will
// check the sequence numbers on those commands that support it and reply accordingly.
void CBMasterPort::UnprotectedSendNextMasterCommand(bool timeoutoccured)
{
	bool DoResendCommand = false;


	if (!MasterCommandProtectedData.ProcessingCBCommand)
	{
		// Do we need to resend a command?
		if (timeoutoccured)
		{
			if (MasterCommandProtectedData.RetriesLeft-- > 0)
			{
				MasterCommandProtectedData.ProcessingCBCommand = true;

				//TODO: Do we resend the original command, or do we ask the RTU to send us the last response again???
				// It depends if you think that the outbound message got there - if so ask for the response again. If not, send the command again....
				// If you want a resend command and not send the same command again, allow the following line.
				// DoResendCommand = true;

				LOGDEBUG("Sending Retry on command :{}",GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode))
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

				LOGDEBUG("Reached maximum number of retries on command :{}", GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode))
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
			LOGDEBUG("Sending next command : Fn {}, St {}, Gr {}, 1B {}", GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode),
				std::to_string(MasterCommandProtectedData.CurrentCommand.first[0].GetStationAddress()),
				std::to_string(MasterCommandProtectedData.CurrentCommand.first[0].GetGroup()),
				to_binstring(MasterCommandProtectedData.CurrentCommand.first[0].GetB()));
		}

		// If either of the above situations need us to send a command, do so.
		if (MasterCommandProtectedData.ProcessingCBCommand == true)
		{
			if (DoResendCommand)
				SendCBMessage(GetResendMessage());
			else
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
								      LOGDEBUG("CB Master Timeout valid - CB Function {}", GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode));

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
#ifdef _MSC_VER
#pragma endregion

#pragma region MessageProcessing
#endif
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

			LOGDEBUG("CB Master received a response to sending cmd {} On Station Address - {}", GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode),std::to_string(Header.GetStationAddress()));

			// If we have an error, we have to wait for the timeout to occur, there may be another packet in behind which is the correct one. If we bail now we may never re-synchronise.
			if (Header.GetStationAddress() == 0)
			{
			      LOGERROR("Received broadcast return message - address 0 - ignoring - {} On Station Address - {}", GetFunctionCodeName(Header.GetFunctionCode()),std::to_string(Header.GetStationAddress()));
			      return;
			}
			if (Header.GetStationAddress() != MyConf->mAddrConf.OutstationAddr)
			{
			      LOGERROR("Received a message from the wrong address - ignoring - {} On Station Address - {}", GetFunctionCodeName(Header.GetFunctionCode()), std::to_string(Header.GetStationAddress()));
			      return;
			}
			if (Header.GetFunctionCode() != MasterCommandProtectedData.CurrentFunctionCode)
			{
			      LOGERROR("Received a message with the wrong (non-matching) function code - ignoring - {} On Station Address - {}", GetFunctionCodeName(Header.GetFunctionCode()),std::to_string(Header.GetStationAddress()));
			      return;
			}

			bool success = false;
			bool NotImplemented = false;

			// Now based on the PendingCommand Function (Not the function code we got back!!), take action. Not all codes are expected or result in action
			switch (MasterCommandProtectedData.CurrentFunctionCode)
			{
				case FUNC_SCAN_DATA:
					success = ProcessScanRequestReturn(CompleteCBMessage); // Fn - 0
					break;
				case FUNC_EXECUTE_COMMAND:
					success = CheckResponseHeaderMatch(Header, MasterCommandProtectedData.CurrentCommand.first[0]);
					break;
				case FUNC_TRIP:
					success = CheckResponseHeaderMatch(Header, MasterCommandProtectedData.CurrentCommand.first[0]);
					break;
				case FUNC_SETPOINT_A:
					success = CheckResponseHeaderMatch(Header, MasterCommandProtectedData.CurrentCommand.first[0]);
					break;
				case FUNC_CLOSE:
					success = CheckResponseHeaderMatch(Header, MasterCommandProtectedData.CurrentCommand.first[0]);
					break;
				case FUNC_SETPOINT_B:
					success = CheckResponseHeaderMatch(Header, MasterCommandProtectedData.CurrentCommand.first[0]);
					break;
				case FUNC_RESET:
					NotImplemented = true;
					break;
				case FUNC_MASTER_STATION_REQUEST:
					LOGDEBUG("Received Master Station Request Response - Sub Code {}, Station {}", GetSubFunctionCodeName(Header.GetGroup()), std::to_string(Header.GetStationAddress()));
					success = true; // We dont need to check what we get back...
					break;
				case FUNC_SEND_NEW_SOE:
					success = ProcessSOEScanRequestReturn(Header, CompleteCBMessage); // Fn - 10
					break;
				case FUNC_REPEAT_SOE:
					success = ProcessSOEScanRequestReturn(Header, CompleteCBMessage); // Fn - 10
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
					LOGERROR("Unknown Message Function - {} On Station Address - {}", GetFunctionCodeName(Header.GetFunctionCode()) , std::to_string(Header.GetStationAddress()));
					break;
			}

			if (NotImplemented == true)
			{
			      LOGERROR("PendingCommand Function NOT Implemented - {} On Station Address - {}", GetFunctionCodeName(Header.GetFunctionCode()),std::to_string(Header.GetStationAddress()));
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
			      LOGERROR("PendingCommand Response failed - Received - {},  Expecting {}  On Station Address - {}",
					GetFunctionCodeName(Header.GetFunctionCode()), GetFunctionCodeName(MasterCommandProtectedData.CurrentFunctionCode), std::to_string(Header.GetStationAddress()));
			}
			#ifdef _MSC_VER
			#pragma warning(suppress: 26495)
			#endif
		});
}


// We have received data from a Scan command. Now decode...
// Return success or failure
bool CBMasterPort::ProcessScanRequestReturn(const CBMessage_t& CompleteCBMessage)
{
	uint8_t Group = CompleteCBMessage[0].GetGroup();

	uint8_t NumberOfBlocks = numeric_cast<uint8_t>(CompleteCBMessage.size());

	LOGDEBUG("Scan Data processing ");

	// For each of the payloads, find the matching points, save the values and trigger the Events.
	// There is always Block 1, payload B no matter what.

	auto payloadlocation = PayloadLocationType(1, PayloadABType::PositionB);

	ProccessScanPayload(CompleteCBMessage[0].GetB(), Group,  payloadlocation);

	for (uint8_t blockindex = 1; blockindex < NumberOfBlocks; blockindex++)
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
	CBTime now = CBNow();

	LOGDEBUG("MA - Group - {} Processing Payload - {} Value 0x{}", std::to_string(group),payloadlocation.to_string() ,to_hexstring(data));

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

			LOGDEBUG("MA - Published Event - Analog - Index {} Value 0x{}", ODCIndex,to_hexstring(data));

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

				LOGDEBUG("MA - Published Event - Counter - Index {} Value 0x{}", std::to_string(ODCIndex), to_hexstring(data));

				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from CB, so add it as soon as possible);
				event->SetPayload<EventType::Counter>(std::move(data));
				PublishEvent(event);

				FoundMatch = true;
			});
	}
	if (!FoundMatch)
	{
		MyPointConf->PointTable.ForEachMatchingBinaryPoint(group, payloadlocation, [&](CBBinaryPoint& pt)
			{
				uint8_t ch = pt.GetChannel();

				// We have a matching point, set a flag to indicate we have a match, save and trigger an event
				switch (pt.GetPointType())
				{
					case DIG:
						{
						      uint8_t bitvalue = (data >> (12 - ch)) & 0x0001;
						      LOGDEBUG("MA - DIG Block Received - Chan {} - Value {}", ch, bitvalue);

						      SendBinaryEvent(pt, bitvalue, now);

						      FoundMatch = true;
						}
						break;

					case MCA:
						{
						                                                                 // The Change state cannot be handled in ODC, it will be handled by the actual value changes
						      uint8_t bitvalue = (data >> (10 - (ch - 1) * 2)) & 0x0001; // Bit 11 is COS, Bit 10 is Value. Bit 1 is COS, Bit 0 is value
						      uint8_t cos = (data >> (11 - (ch - 1) * 2)) & 0x0001;
						      LOGDEBUG("MA - MCA Block Received - Chan {} - Value {} - COS {}", ch, bitvalue, cos);

						      SendBinaryEvent(pt, bitvalue, now);

						      FoundMatch = true;
						}
						break;

					case MCB:
						{
						                                                                 // The Change state cannot be handled in ODC, it will be handled by the actual value changes
						      uint8_t bitvalue = (data >> (10 - (ch - 1) * 2)) & 0x0001; // Bit 11 is COS, Bit 10 is Value. Bit 1 is COS, Bit 0 is value
						      uint8_t cos = (data >> (11 - (ch - 1) * 2)) & 0x0001;
						      LOGDEBUG("MA - MCB Block Received - Chan {} - Value {} - COS {}", ch, bitvalue, cos);

						      SendBinaryEvent(pt, bitvalue, now);

						      FoundMatch = true;
						}
						break;

					case MCC:
						{
						                                                                 // The Change state cannot be handled in ODC, it will be handled by the actual value changes
						      uint8_t bitvalue = (data >> (10 - (ch - 1) * 2)) & 0x0001; // Bit 11 is COS, Bit 10 is Value. Bit 1 is COS, Bit 0 is value
						      uint8_t cos = (data >> (11 - (ch - 1) * 2)) & 0x0001;
						      LOGDEBUG("MA - MCC Block Received - Chan {} - Value {} - COS {}", ch, bitvalue, cos);

						      SendBinaryEvent(pt, bitvalue, now);

						      FoundMatch = true;
						}
						break;

					default:
						LOGERROR("We received an un-handled digital point type - Group " + std::to_string(group) + " Payload Location " + payloadlocation.to_string());
						break;
				}
			});
	}
	if (!FoundMatch)
	{
		// See if it is a StatusByte we need to handle - there is only one status byte, but it could be requested in several groups. So deal with it whenever it comes back
		MyPointConf->PointTable.ForEachMatchingStatusByte(group, payloadlocation, [&](void)
			{
				// We have a matching status byte, set a flag to indicate we have a match.
				LOGDEBUG("Received a Status Byte at : {} - {}", std::to_string(group), payloadlocation.to_string());
				//TODO: Not sure what we are going to do with the status byte - YET
				FoundMatch = true;
			});
	}
	if (!FoundMatch)
	{
		LOGDEBUG("Failed to find a payload for: {} Setting to zero", payloadlocation.to_string());
	}
}

void CBMasterPort::SendBinaryEvent(CBBinaryPoint & pt, uint8_t &bitvalue, const CBTime &now)
{
	// Only process if the value has changed - otherwise have lots of needless events.
	if ((pt.GetBinary() != bitvalue) || (pt.GetHasBeenSet() == false))
	{
		pt.SetBinary(bitvalue, now); // Sets the has been set flag!

		uint32_t ODCIndex = pt.GetIndex();

		QualityFlags qual = QualityFlags::ONLINE; // CalculateBinaryQuality(enabled, now); //TODO: Handle quality better?
		LOGDEBUG("Published Event - Binary Index {} Value {}", std::to_string(ODCIndex), std::to_string(bitvalue));
		auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now));
		event->SetPayload<EventType::Binary>(bitvalue == 1);
		PublishEvent(event);
	}
}

bool CBMasterPort::ProcessSOEScanRequestReturn(const CBBlockData& ReceivedHeader, const CBMessage_t& CompleteCBMessage)
{
	if (CompleteCBMessage.size() == 1)
	{
		LOGDEBUG("SOE Scan Data processing - No SOE Data to process");
		return true;
	}

	LOGDEBUG("SOE Scan Data processing - Blocks {}", CompleteCBMessage.size());

	uint32_t UsedBits = 0;
	std::array<bool, MaxSOEBits> BitArray;

	if (!ConvertSOEMessageToBitArray(CompleteCBMessage, BitArray, UsedBits))
		return false;

	// Convert the BitArray to SOE events, and call our lambda for each
	ForEachSOEEventInBitArray(BitArray, UsedBits, [&](SOEEventFormat& soeevnt)
		{
			// Now use the data in the SOE Event to fire off an ODC event..
			// Find the Point in our database...using SOE Group and Number
			// uint8_t ScanGroup = soeevnt.Group;
			uint8_t SOEIndex = soeevnt.Number;

			size_t ODCIndex = 0;

			if (MyPointConf->PointTable.GetBinaryODCIndexUsingSOE(SOEIndex, ODCIndex))
			{
			      uint8_t bitvalue = soeevnt.ValueBit;

			// In the processing loop that calls us, we have converted the (possibly) delta time records to full h:m:s:msec records.
			// We just then need to add the current day to the value to make an ODC compatible time...

			      // Deal with midnight roll over in the SOE CB Time records
			// The SOE Event Hour should be <= to the current hour - i.e. the event was in the past.
			// If it is not, it must be a midnight rollover. i.e. Event Hour == 23, Current Hour == 0
			// So if this occurs, the day must have been yesterday...

			      CBTime Now = CBNow();

			      if (GetHour(Now) > soeevnt.Hour)
			      {
			// The Event hour is in the future relative to the current hour, so it must have been yesterday...
			// Subtract 1 day from the Now time, then we should be good.
			            Now = Now - CBTimeOneDay;
				}
			      CBTime changedtime = GetDayStartTime(Now) + soeevnt.GetTotalMsecTime();

			      QualityFlags qual = QualityFlags::ONLINE; // CalculateBinaryQuality(enabled, now); //TODO: Handle quality better?
			      LOGDEBUG("Published Binary SOE Event -  SOE Index {} ODC Index {} Bit Value {} Time {}", SOEIndex, ODCIndex, bitvalue, to_timestringfromCBtime(changedtime));
			      auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(changedtime));
			      event->SetPayload<EventType::Binary>(bitvalue == 1);
			      PublishEvent(event);
			}
			else
			{
			      LOGERROR("Received an Binary SOE Event Record, but we dont have a matching point definition... Number {}", SOEIndex);
			}
		});

	return true;
}

bool CBMasterPort::ConvertSOEMessageToBitArray(const CBMessage_t& CompleteCBMessage, std::array<bool, MaxSOEBits> &BitArray, uint32_t &UsedBits )
{
	uint8_t NumberOfBlocks = numeric_cast<uint8_t>(CompleteCBMessage.size());

	if (NumberOfBlocks == 1)
	{
		LOGERROR("Only Received one SOE Message Block, insufficient data, minimum of 2 required");
		return false;
	}

	// The SOE data is built into a stream of bits (that may not be block aligned) and then it is stuffed 12 bits at a time into the available Payload locations - up to 31.
	// First section format:
	// 1 - 3 bits group #, 7 bits point number, Status(value) bit, Quality Bit (unused), Time Bit - changes what comes next
	// T==1 Time - 27 bits, Hour (0-23, 5 bits), Minute (0-59, 6 bits), Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// T==0 Hours and Minutes same as previous event, 16 bits - Second (0-59, 6 bits), Millisecond (0-999, 10 bits)
	// Last bit L, when set indicates last record.
	// So the data can be 13+27+1 bits = 41 bits, or 13+16+1 = 30 bits.

	// The maximum number of bits we can send is 12 * 31 = 372.

	// Take each of the payload 12 bit blocks, and combine them into a bit array. Block 0 Payload A is group address and other data. Start at block B
	UsedBits = 0;

	for (uint8_t blocknum = 0; blocknum < NumberOfBlocks; blocknum++)
	{
		if (blocknum != 0)
		{
			uint16_t payloadA = CompleteCBMessage[blocknum].GetA();
			for (int i = 11; i >= 0; i--)
			{
				BitArray[UsedBits++] = TestBit(payloadA, i);
			}
		}

		uint16_t payloadB = CompleteCBMessage[blocknum].GetB();
		for (int i = 11; i >= 0; i--)
		{
			BitArray[UsedBits++] = TestBit(payloadB, i);
		}
	}
	return true;
}

void CBMasterPort::ForEachSOEEventInBitArray(std::array<bool, MaxSOEBits> &BitArray, uint32_t &UsedBits, std::function<void(SOEEventFormat &soeevnt)> fn)
{
	// We now have the data in the bit array, now we have to decode into the SOE blocks - 30 or 41 bits long.
	uint32_t startbit = 0;
	uint32_t newstartbit = 0;
	CBTime LastEventTime = 0; // msec representing the last hour/min/sec/msec value received from an event.
	// First event must have all 4 values. Subsequent events may only be seconds/msec
	do
	{
		// Create the Event, it will tell us how many bits it consumed.
		startbit = newstartbit;

		// Returns IsLastRecord flag, also if  we sucessfully got an Event from the bit stream.
		bool Success = false;
		SOEEventFormat Event(BitArray, startbit, UsedBits, newstartbit, LastEventTime, Success); // Will always expand to a full time (h:M:S:ms) for every record.
		LastEventTime = Event.GetTotalMsecTime();

		if (Success)
		{
			fn(Event);
		}
		else
		{
			LOGERROR("The SOEEventFormat bitarray parser failed.. StartBit {}, NewStartBit {}", startbit, newstartbit);
			return;
		}
		if (Event.LastEventFlag)
		{
			uint32_t RemainingBits = UsedBits - newstartbit;
			if (RemainingBits > 24)
			{
				LOGDEBUG("SOEEventFormat We have more than 2 sections of data remaining after last SOE Event. {} bits of data available", RemainingBits);
			}
			// Less than 24 bits is ok, it was just necessary padding.
			return;
		}
	} while ((newstartbit+30) < UsedBits); // Only keep going if there is space for another message.
}

// Checks what we got back against what we sent. We know the function code and address have been checked.
bool CBMasterPort::CheckResponseHeaderMatch(const CBBlockData& ReceivedHeader, const CBBlockData& SentHeader)
{
	if (ReceivedHeader.GetGroup() != SentHeader.GetGroup())
	{
		LOGDEBUG("Returned Header mismatch on Group {}, {}", ReceivedHeader.GetGroup(),SentHeader.GetGroup());
		return false;
	}
	if (ReceivedHeader.GetB() != SentHeader.GetB())
	{
		LOGDEBUG("Returned Header mismatch on B Data {}, {}", ReceivedHeader.GetB(),SentHeader.GetB());
		return false;
	}

	LOGDEBUG("Returned Header match Sent Header");
	return true;
}

#ifdef _MSC_VER
#pragma endregion
#endif

void CBMasterPort::DoPoll(uint32_t PollID)
{
	if (!enabled) return;
	LOGDEBUG("DoPoll : {}", std::to_string(PollID));

	switch (MyPointConf->PollGroups[PollID].polltype)
	{
		case Scan:
		{
			// We will scan a single Group. Payload can be up to 31 payload blocks.
			uint8_t Group = MyPointConf->PollGroups[PollID].group;
			SendF0ScanCommand(Group, nullptr);
		}
		break;

		case  TimeSetCommand:
		{
			// Send a time set command to the OutStation
			SendFn9TimeUpdate(nullptr);
			LOGDEBUG("Poll Issued a TimeDate Update Command");
		}
		break;
		case  SOEScan:
		{
			// Send a time set command to the OutStation
			uint8_t Group = MyPointConf->PollGroups[PollID].group;
			SendFn10SOEScanCommand(Group, nullptr);
			LOGDEBUG("Poll Issued a SOE Scan Command");
		}
		break;

		//case SystemFlagScan:
		//{
		//TODO: Remaining Poll types for Conitel/Baker
		//}
		//break;

		default:
			LOGDEBUG("Poll will an unknown polltype : {}", std::to_string(MyPointConf->PollGroups[PollID].polltype));
			break;
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

void CBMasterPort::SendF0ScanCommand(uint8_t group, SharedStatusCallback_t pStatusCallback)
{
	CBBlockData sendcommandblock(MyConf->mAddrConf.OutstationAddr, group, FUNC_SCAN_DATA, 0, true);
	QueueCBCommand(sendcommandblock, pStatusCallback);
}
void CBMasterPort::SendFn9TimeUpdate(SharedStatusCallback_t pStatusCallback)
{
	CBMessage_t CompleteCBMessage;

	CBPort::BuildUpdateTimeMessage(MyConf->mAddrConf.OutstationAddr, CBNow(), CompleteCBMessage);

	QueueCBCommand(CompleteCBMessage, pStatusCallback);
}

void CBMasterPort::SendFn10SOEScanCommand(uint8_t group, SharedStatusCallback_t pStatusCallback)
{
	CBBlockData sendcommandblock(MyConf->mAddrConf.OutstationAddr, group, FUNC_SEND_NEW_SOE, 0, true);
	QueueCBCommand(sendcommandblock, pStatusCallback);
}
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
	bool exists = MyPointConf->PointTable.GetBinaryControlCBIndexUsingODCIndex(index, Group, Channel);

	if (!exists)
	{
		LOGDEBUG("Master received a Binary Control ODC Change Command on a point that is not defined {}", std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	std::string OnOffString = "ON";

	if ((command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
	{
		OnOffString = "OFF";

		SendDigitalControlOffCommand(MyConf->mAddrConf.OutstationAddr, Group, Channel, pStatusCallback);
	}
	else
	{
		OnOffString = "ON";

		SendDigitalControlOnCommand(MyConf->mAddrConf.OutstationAddr, Group, Channel, pStatusCallback);
	}
	LOGDEBUG("Master received a Binary Control Output Command - Index: {} - {} Group {} Channel {}", index, OnOffString,std::to_string(index), std::to_string(Group), std::to_string(Channel));
}

void CBMasterPort::WriteObject(const int16_t &command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGDEBUG("Master received an ANALOG CONTROL Change Command {}", std::to_string(index));

	uint8_t Group = 0;
	uint8_t Channel = 0;
	bool exists = MyPointConf->PointTable.GetAnalogControlCBIndexUsingODCIndex(index, Group, Channel);

	if (!exists)
	{
		LOGDEBUG("Master received an Analog Control ODC Change Command on a point that is not defined {}",std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	assert((Channel >= 1) && (Channel <= 2));

	uint16_t BData = numeric_cast<uint16_t>(command);
	uint8_t FunctionCode = FUNC_SETPOINT_A;

	if (Channel == 2)
		FunctionCode = FUNC_SETPOINT_B;

	auto pStatusCallbacklocal = std::make_shared<std::function<void(CommandStatus)>>([&, pStatusCallback, Group](CommandStatus command_stat)
		{
			LOGDEBUG("Internal Callback on ANALOG CONTROL command result : {}", std::to_string(static_cast<int>(command_stat)));

			if (command_stat == CommandStatus::SUCCESS)
			{
			// Then queue the execute command
			      LOGDEBUG("Queue EXECUTE command in response to successful ANALOG CONTROL command");
			      CBBlockData executeblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, Group, FUNC_EXECUTE_COMMAND, 0, true);
			      QueueCBCommand(executeblock, pStatusCallback);
			}
		});

	CBBlockData commandblock = CBBlockData(MyConf->mAddrConf.OutstationAddr, Group, FunctionCode, BData, true);
	QueueCBCommand(commandblock, pStatusCallbacklocal);

	// The execute command is sent if the initial command works - see lambda above
}


void CBMasterPort::WriteObject(const int32_t & command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGDEBUG("Master received unknown AnalogOutputInt32 ODC Event - Index {}, Value {}",index,command);
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::WriteObject(const float& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGERROR("On Master float Type is not implemented - Index {}, Value {}",index,command);
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::WriteObject(const double& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{

	LOGDEBUG("Master received unknown double ODC Event - Index {}, Value {}",index,command);
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void CBMasterPort::SendDigitalControlOnCommand(const uint8_t &StationAddress, const uint8_t &Group, const uint16_t &Channel, const SharedStatusCallback_t &pStatusCallback)
{
	assert((Channel >= 1) && (Channel <= 12));
	uint16_t BData = numeric_cast<uint16_t>(1 << (12 - Channel));

	auto pStatusCallbacklocal = std::make_shared<std::function<void(CommandStatus)>>([&, pStatusCallback, Group, StationAddress](CommandStatus command_stat)
		{
			LOGDEBUG("Internal Callback on CONTROL ON command result : {}", std::to_string(static_cast<int>(command_stat)));

			if (command_stat == CommandStatus::SUCCESS)
			{
			// Then queue the execute command
			      LOGDEBUG("Queue EXECUTE command in response to successful CONTROL ON command");
			      CBBlockData executeblock = CBBlockData(StationAddress, Group, FUNC_EXECUTE_COMMAND, 0, true);
			      QueueCBCommand(executeblock, pStatusCallback);
			}
		});

	CBBlockData commandblock = CBBlockData(StationAddress, Group, FUNC_CLOSE, BData, true); // Trip is OPEN or OFF
	QueueCBCommand(commandblock, pStatusCallbacklocal);

	// The execute command is sent if the initial command works - see lambda above
}
void CBMasterPort::SendDigitalControlOffCommand(const uint8_t &StationAddress, const uint8_t &Group, const uint16_t &Channel, const SharedStatusCallback_t &pStatusCallback)
{
	assert((Channel >= 1) && (Channel <= 12));
	uint16_t BData = numeric_cast<uint16_t>(1 << (12 - Channel));

	auto pStatusCallbacklocal = std::make_shared<std::function<void(CommandStatus)>>([&, pStatusCallback, Group, StationAddress](CommandStatus command_stat)
		{
			LOGDEBUG("Internal Callback on CONTROL OFF command result : {}", std::to_string(static_cast<int>(command_stat)));

			if (command_stat == CommandStatus::SUCCESS)
			{
			// Then queue the execute command
			      LOGDEBUG("Queue EXECUTE command in response to successful CONTROL OFF command");
			      CBBlockData executeblock = CBBlockData(StationAddress, Group, FUNC_EXECUTE_COMMAND, 0, true);
			      QueueCBCommand(executeblock, pStatusCallback);
			}
		});

	CBBlockData commandblock = CBBlockData(StationAddress, Group, FUNC_TRIP, BData, true); // Trip is OPEN or OFF
	QueueCBCommand(commandblock, pStatusCallbacklocal);

	// The execute command is sent if the initial command works - see lambda above
}




