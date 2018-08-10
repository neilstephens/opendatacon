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
* MD3MasterPort.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#include <thread>
#include <chrono>
#include <array>
#include <opendnp3/app/MeasurementTypes.h>

#include "MD3.h"
#include "MD3Engine.h"
#include "MD3MasterPort.h"


MD3MasterPort::MD3MasterPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
	MD3Port(aName, aConfFilename, aConfOverrides),
	PollScheduler(nullptr)
{
	std::string over = "None";
	if (aConfOverrides.isObject()) over = aConfOverrides.toStyledString();

	LOGDEBUG("MD3Master Constructor - " + aName + " - " + aConfFilename + " Overrides - " + over);
}

MD3MasterPort::~MD3MasterPort()
{
	Disable();
	pConnection->RemoveMaster(MyConf()->mAddrConf.OutstationAddr);
	// The pConnection will be closed by this point, so is not holding any resources, and will be freed on program close when the static list is destroyed.
}

void MD3MasterPort::Enable()
{
	if (enabled) return;
	try
	{
		if (pConnection.get() == nullptr)
			throw std::runtime_error("Connection manager uninitialised");

		pConnection->Open(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop

		enabled = true;
	}
	catch (std::exception& e)
	{
		LOGERROR("Problem opening connection TCP : " + Name + " : " + e.what());
		return;
	}
}
void MD3MasterPort::Disable()
{
	if (!enabled) return;
	enabled = false;

	if (pConnection.get() == nullptr)
		return;
	pConnection->Close(); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void MD3MasterPort::SocketStateHandler(bool state)
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

		ClearMD3CommandQueue(); // Remove all waiting commands and callbacks

		PublishEvent(std::move(ConnectState::DISCONNECTED));
		msg = Name + ": Connection closed.";
	}
	LOGINFO(msg);
}

// Will be change to Build only. No live reload
void MD3MasterPort::Build()
{
	std::string ChannelID = MyConf()->mAddrConf.ChannelID();

	if (PollScheduler == nullptr)
		PollScheduler.reset(new ASIOScheduler(*pIOS));

	MasterCommandProtectedData.CurrentCommandTimeoutTimer.reset(new Timer_t(*pIOS));
	MasterCommandStrand.reset(new asio::strand(*pIOS));

	pConnection = MD3Connection::GetConnection(ChannelID); //Static method

	if (pConnection == nullptr)
	{
		pConnection.reset(new MD3Connection(pIOS, IsServer(), MyConf()->mAddrConf.IP,
			std::to_string(MyConf()->mAddrConf.Port), this, true, MyConf()->TCPConnectRetryPeriodms)); // Retry period cannot be different for multidrop outstations

		MD3Connection::AddConnection(ChannelID, pConnection); //Static method
	}

	pConnection->AddMaster(MyConf()->mAddrConf.OutstationAddr,
		std::bind(&MD3MasterPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3MasterPort::SocketStateHandler, this, std::placeholders::_1));

	PollScheduler->Stop();
	PollScheduler->Clear();
	for (auto pg : MyPointConf()->PollGroups)
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

void MD3MasterPort::SendMD3Message(const MD3Message_t &CompleteMD3Message)
{
	if (CompleteMD3Message.size() == 0)
	{
		LOGERROR("MA - Tried to send an empty message to the TCP Port");
		return;
	}
	LOGDEBUG("MA - Sending Message - " + MD3MessageAsString(CompleteMD3Message));

	// Done this way just to get context into log messages.
	MD3Port::SendMD3Message(CompleteMD3Message);
}

#pragma region MasterCommandQueue

// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
// There is a fixed timeout function (below) which will queue the next command if we timeout.
// If the ProcessMD3Message callback gets the command it expects, it will send the next command in the queue.
// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
void MD3MasterPort::QueueMD3Command(const MD3Message_t &CompleteMD3Message, SharedStatusCallback_t pStatusCallback)
{
	MasterCommandStrand->dispatch([=]() // Tries to execute, if not able to will post. Note the calling thread must be one of the io_service threads.... this changes our tests!
		{
			if (MasterCommandProtectedData.MasterCommandQueue.size() < MasterCommandProtectedData.MaxCommandQueueSize)
			{
			      MasterCommandProtectedData.MasterCommandQueue.push(MasterCommandQueueItem(CompleteMD3Message, pStatusCallback)); // async
			}
			else
			{
			      LOGDEBUG("Tried to queue another MD3 Master Command when the command queue is full");
			      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED); // Failed...
			}

			// Will only send if we can - i.e. not currently processing a command
			UnprotectedSendNextMasterCommand(false);
		});
}
// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockData &SingleBlockMD3Message, SharedStatusCallback_t pStatusCallback)
{
	MD3Message_t CommandMD3Message;
	CommandMD3Message.push_back(SingleBlockMD3Message);
	QueueMD3Command(CommandMD3Message, pStatusCallback);
}
// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockFormatted &SingleBlockMD3Message, SharedStatusCallback_t pStatusCallback)
{
	MD3Message_t CommandMD3Message;
	CommandMD3Message.push_back(SingleBlockMD3Message);
	QueueMD3Command(CommandMD3Message, pStatusCallback);
}

// Just schedule the callback, don't want to do it in a strand protected section.
void MD3MasterPort::PostCallbackCall(const odc::SharedStatusCallback_t &pStatusCallback, CommandStatus c)
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
void MD3MasterPort::SendNextMasterCommand()
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
void MD3MasterPort::UnprotectedSendNextMasterCommand(bool timeoutoccured)
{
	if (!MasterCommandProtectedData.ProcessingMD3Command)
	{
		if (timeoutoccured)
		{
			// Do we need to resend a command?
			if (MasterCommandProtectedData.RetriesLeft-- > 0)
			{
				MasterCommandProtectedData.ProcessingMD3Command = true;
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

		if (!MasterCommandProtectedData.MasterCommandQueue.empty() && (MasterCommandProtectedData.ProcessingMD3Command != true))
		{
			// Send the next command if there is one and we are not retrying.

			MasterCommandProtectedData.ProcessingMD3Command = true;
			MasterCommandProtectedData.RetriesLeft = MyPointConf()->MD3CommandRetries;

			MasterCommandProtectedData.CurrentCommand = MasterCommandProtectedData.MasterCommandQueue.front();
			MasterCommandProtectedData.MasterCommandQueue.pop();

			MasterCommandProtectedData.CurrentFunctionCode = ((MD3BlockFormatted)MasterCommandProtectedData.CurrentCommand.first[0]).GetFunctionCode();
			LOGDEBUG("Sending next command :" + std::to_string(MasterCommandProtectedData.CurrentFunctionCode))
		}

		// If either of the above situations need us to send a command, do so.
		if (MasterCommandProtectedData.ProcessingMD3Command == true)
		{
			SendMD3Message(MasterCommandProtectedData.CurrentCommand.first); // This should be the only place this is called for the MD3Master...

			// Start an async timed callback for a timeout - cancelled if we receive a good response.
			MasterCommandProtectedData.TimerExpireTime = std::chrono::milliseconds(MyPointConf()->MD3CommandTimeoutmsec);
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
								      LOGDEBUG("MD3 Master Timeout valid - MD3 Function " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode));

								      MasterCommandProtectedData.ProcessingMD3Command = false; // Only gets reset on success or timeout.

								      UnprotectedSendNextMasterCommand(true); // We already have the strand, so don't need the wrapper here
								}
								else
									LOGDEBUG("MD3 Master Timeout callback called, when we had already moved on to the next command");
							});
					}
					else
					{
					      LOGDEBUG("MD3 Master Timeout callback cancelled");
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
void MD3MasterPort::ClearMD3CommandQueue()
{
	MasterCommandStrand->dispatch([&]()
		{
			MD3Message_t NextCommand;
			while (!MasterCommandProtectedData.MasterCommandQueue.empty())
			{
			      MasterCommandProtectedData.MasterCommandQueue.pop();
			}
			MasterCommandProtectedData.CurrentFunctionCode = 0;
			MasterCommandProtectedData.ProcessingMD3Command = false;
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
void MD3MasterPort::ProcessMD3Message(MD3Message_t &CompleteMD3Message)
{
	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.

	//! Anywhere we find that we don't have what we need, return. If we succeed we send the next command at the end of this method.
	// If the timeout on the command is activated, then the next command will be sent - or resent.
	// Cant just use by reference, complete message and header will go out of scope...
	MasterCommandStrand->dispatch([&, CompleteMD3Message]()
		{

			if (CompleteMD3Message.size() == 0)
			{
			      LOGERROR("Received a Master to Station message with zero length!!! ");
			      return;
			}

			MD3BlockFormatted Header = MD3BlockFormatted(CompleteMD3Message[0]);

			// If we have an error, we have to wait for the timeout to occur, there may be another packet in behind which is the correct one. If we bail now we may never re-synchronise.

			if (Header.IsMasterToStationMessage() != false)
			{
			      LOGERROR("Received a Master to Station message at the Master - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}
			if ((Header.GetStationAddress() != 0) && (Header.GetStationAddress() != MyConf()->mAddrConf.OutstationAddr))
			{
			      LOGERROR("Received a message from the wrong address - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}
			if (Header.GetStationAddress() == 0)
			{
			      LOGERROR("Received broadcast return message - address 0 - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}

			LOGDEBUG("MD3 Master received a response to sending cmd " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode) + " of " + std::to_string(Header.GetFunctionCode()));

			bool success = false;

			// Now based on the Command Function (Not the function code we got back!!), take action. Not all codes are expected or result in action
			switch (MasterCommandProtectedData.CurrentFunctionCode)
			{
				case ANALOG_UNCONDITIONAL: // What we sent and reply
					if (Header.GetFunctionCode() == ANALOG_UNCONDITIONAL)
						success = ProcessAnalogUnconditionalReturn(Header, CompleteMD3Message);
					break;

				case ANALOG_DELTA_SCAN: // Command and reply
					if (Header.GetFunctionCode() == ANALOG_UNCONDITIONAL)
						success = ProcessAnalogUnconditionalReturn(Header, CompleteMD3Message);
					else if (Header.GetFunctionCode() == ANALOG_DELTA_SCAN)
						success = ProcessAnalogDeltaScanReturn(Header, CompleteMD3Message);
					else if (Header.GetFunctionCode() == ANALOG_NO_CHANGE_REPLY)
						success = ProcessAnalogNoChangeReturn(Header, CompleteMD3Message);
					break;

				case DIGITAL_UNCONDITIONAL_OBS:
					// if ((FunctionCode == DIGITAL_UNCONDITIONAL_OBS) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY))
					LOGERROR("Fn7 - Old Style Digital - Digital Unconditional - IS NOT IMPLEMENTED");
					break;
				case DIGITAL_DELTA_SCAN:
					// if ((FunctionCode == DIGITAL_DELTA_SCAN) || (FunctionCode == DIGITAL_UNCONDITIONAL_OBS) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY))
					LOGERROR("Fn8 - Old Style Digital - Digital Delta Scan - IS NOT IMPLEMENTED");
					break;
				case HRER_LIST_SCAN:
					// if (FunctionCode == HRER_LIST_SCAN)
					LOGERROR("Fn9 - Old Style Digital - Digital HRER Scan - IS NOT IMPLEMENTED");
					break;
				case DIGITAL_CHANGE_OF_STATE:
					// if ((FunctionCode == DIGITAL_CHANGE_OF_STATE) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY))
					LOGERROR("Fn10 - Old Style Digital - Digital COS Scan - IS NOT IMPLEMENTED");
					break;
				case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
					if (Header.GetFunctionCode() == DIGITAL_NO_CHANGE_REPLY)
						success = ProcessDigitalNoChangeReturn(Header, CompleteMD3Message);
					else if (Header.GetFunctionCode() == DIGITAL_CHANGE_OF_STATE_TIME_TAGGED)
						success = ProcessDigitalScan(Header, CompleteMD3Message);
					break;
				case DIGITAL_UNCONDITIONAL:
					if (Header.GetFunctionCode() == DIGITAL_NO_CHANGE_REPLY)
						success = ProcessDigitalNoChangeReturn(Header, CompleteMD3Message);
					else if ((Header.GetFunctionCode() == DIGITAL_UNCONDITIONAL) || (Header.GetFunctionCode() == DIGITAL_CHANGE_OF_STATE_TIME_TAGGED))
					{
					      LOGDEBUG("Doing Digital Unconditional (new) processing - which is the same as Digital Scan");
					      success = ProcessDigitalScan(Header, CompleteMD3Message);
					}
					break;
				case ANALOG_NO_CHANGE_REPLY:
					LOGERROR("Master Should Never Send this Command - Fn 13 - ANALOG_NO_CHANGE_REPLY");
					break;
				case DIGITAL_NO_CHANGE_REPLY:
					LOGERROR("Master Should Never Send this Command - Fn 14 - DIGITAL_NO_CHANGE_REPLY");
					break;
				case CONTROL_REQUEST_OK:
					LOGERROR("Master Should Never Send this Command - Fn 15 - CONTROL_REQUEST_OK");
					break;
				case FREEZE_AND_RESET:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessFreezeResetReturn(Header, CompleteMD3Message);
					break;
				case POM_TYPE_CONTROL:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessPOMReturn(Header, CompleteMD3Message);
					break;
				case DOM_TYPE_CONTROL:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessDOMReturn(Header, CompleteMD3Message);
					break;
				case INPUT_POINT_CONTROL:
					LOGERROR("Fn20 - INPUT_POINT_CONTROL - IS NOT IMPLEMENTED");
					break;
				case RAISE_LOWER_TYPE_CONTROL:
					LOGERROR("Fn21 - RAISE_LOWER_TYPE_CONTROL - IS NOT IMPLEMENTED");
					break;
				case AOM_TYPE_CONTROL:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessAOMReturn(Header, CompleteMD3Message);
					break;
				case CONTROL_OR_SCAN_REQUEST_REJECTED:
					LOGERROR("Master Should Never Send this Command - Fn 30 - CONTROL_OR_SCAN_REQUEST_REJECTED");
					break;
				case COUNTER_SCAN:
					// if (Header.GetFunctionCode() == COUNTER_SCAN)
					LOGERROR("Fn31 - COUNTER_SCAN - IS NOT IMPLEMENTED - USE ANALOG SCAN TO READ COUNTERS");
					break;
				case SYSTEM_SIGNON_CONTROL:
					if (Header.GetFunctionCode() == SYSTEM_SIGNON_CONTROL)
						success = ProcessSystemSignOnReturn(Header, CompleteMD3Message);
					break;
				case SYSTEM_SIGNOFF_CONTROL:
					LOGERROR("Fn41 - SYSTEM_SIGNOFF_CONTROL - IS NOT IMPLEMENTED");
					break;
				case SYSTEM_RESTART_CONTROL:
					LOGERROR("Fn42 - SYSTEM_RESTART_CONTROL - IS NOT IMPLEMENTED");
					break;
				case SYSTEM_SET_DATETIME_CONTROL:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessSetDateTimeReturn(Header, CompleteMD3Message);
					break;
				case SYSTEM_SET_DATETIME_CONTROL_NEW:
					if (Header.GetFunctionCode() == CONTROL_REQUEST_OK)
						success = ProcessSetDateTimeNewReturn(Header, CompleteMD3Message);
					break;
				case FILE_DOWNLOAD:
					LOGERROR("Fn50 - RAISE_LOWER_TYPE_CONTROL - IS NOT IMPLEMENTED");
					break;
				case FILE_UPLOAD:
					LOGERROR("Fn51 - RAISE_LOWER_TYPE_CONTROL - IS NOT IMPLEMENTED");
					break;
				case SYSTEM_FLAG_SCAN:
					if (Header.GetFunctionCode() == SYSTEM_FLAG_SCAN)
						success = ProcessFlagScanReturn(Header, CompleteMD3Message);
					break;
				case LOW_RES_EVENTS_LIST_SCAN:
					LOGERROR("Fn60 - LOW_RES_EVENTS_LIST_SCAN - IS NOT IMPLEMENTED");
					break;
				default:
					LOGERROR("Unknown Message Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
					break;
			}

			if (success) // Move to the next command. Only other place we do this is in the timeout.
			{
			      MasterCommandProtectedData.CurrentCommandTimeoutTimer->cancel(); // Have to be careful the handler still might do something?
			      MasterCommandProtectedData.ProcessingMD3Command = false;         // Only gets reset on success or timeout.

			      // Execute the callback with a success code.
			      PostCallbackCall(MasterCommandProtectedData.CurrentCommand.second, CommandStatus::SUCCESS); // Does null check
			      UnprotectedSendNextMasterCommand(false);                                                    // We already have the strand, so don't need the wrapper here. Pass in that this is not a retry.
			}
			else
			{
			      LOGERROR("Command Response failed - Received - " + std::to_string(Header.GetFunctionCode()) +
					" Expecting " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
			}
		});
}

// We have received data from an Analog command - could be  the result of Fn 5 or 6
// Store the decoded data into the point lists. Counter scan comes back in an identical format
// Return success or failure
bool MD3MasterPort::ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	int NumberOfDataBlocks = Channels / 2 + Channels % 2; // 2 --> 1, 3 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("MA - Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Doing Analog Unconditional processing ");

	// Unload the analog values from the blocks
	std::vector<uint16_t> AnalogValues;
	int ChanCount = 0;
	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		AnalogValues.push_back(CompleteMD3Message[i + 1].GetFirstWord());
		ChanCount++;

		// The last block may only have one reading in it. The second might be filler.
		if (ChanCount < Channels)
		{
			AnalogValues.push_back(CompleteMD3Message[i + 1].GetSecondWord());
			ChanCount++;
		}
	}

	// Now take the returned values and store them into the points
	uint16_t wordres = 0;
	bool hasbeenset;

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres,hasbeenset);
	MD3Time now = MD3Now();

	for (int i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		int idx = FirstModuleIsCounterModule ? i % 8 : i;
		int maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress+1 : ModuleAddress;

		if (SetAnalogValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			LOGDEBUG("MA - Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(AnalogValues[i]));
			size_t ODCIndex;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("MA - Published Event - Analog - Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(AnalogValues[i]));

				auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, (msSinceEpoch_t)now); // We don't get time info from MD3, so add it as soon as possible);
				event->SetPayload<EventType::Analog>(std::move(AnalogValues[i]));
				PublishEvent(event);
			}
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			LOGDEBUG("MA - Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(AnalogValues[i]));
			size_t ODCIndex;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("MA - Published Event - Counter - Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(AnalogValues[i]));
				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, (msSinceEpoch_t)now); // We don't get time info from MD3, so add it as soon as possible);
				event->SetPayload<EventType::Counter>(std::move(AnalogValues[i]));
				PublishEvent(event);
			}
		}
		else
		{
			LOGERROR("MA - Fn5 Failed to set an Analog or Counter Value - " + std::to_string(Header.GetFunctionCode())
				+ " On Station Address - " + std::to_string(Header.GetStationAddress())
				+ " Module : " + std::to_string(maddress) + " Channel : " + std::to_string(idx));
			return false;
		}
	}
	return true;
}

// Fn 6. This is only one of three possible responses to the Fn 6 command from the Master. The others are 5 (Unconditional) and 13 (No change)
// The data that comes back is up to 16 8 bit signed values representing the change from the last sent value.
// If there was a fault at the OutStation an Unconditional response will be sent instead.
// If a channel is missing or in fault, the value will be 0, so it can still just be added to the value (even if it is 0x8000 - the fault value).
// Return success or failure
bool MD3MasterPort::ProcessAnalogDeltaScanReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	int NumberOfDataBlocks = Channels / 4 + Channels % 4; // 2 --> 1, 5 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("MA - Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) +
			" On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Doing Analog Delta processing ");

	// Unload the analog delta values from the blocks - 4 per block.
	std::vector<int8_t> AnalogDeltaValues;
	int ChanCount = 0;
	for (int i = 0; i < NumberOfDataBlocks; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			AnalogDeltaValues.push_back(CompleteMD3Message[i + 1].GetByte(j)); // Test unsigned/signed conversion here...
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
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres,hasbeenset);
	MD3Time now = MD3Now();

	for (int i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		int idx = FirstModuleIsCounterModule ? i % 8 : i;
		int maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress + 1 : ModuleAddress;

		if (GetAnalogValueUsingMD3Index(maddress, idx, wordres,hasbeenset))
		{
			wordres += AnalogDeltaValues[i];                     // Add the signed delta.
			SetAnalogValueUsingMD3Index(maddress, idx, wordres); //TODO Do all SetMethods need to have a time field as well? With a magic number (Say 10 which is in the past) as default which means no change?

			LOGDEBUG("MA - Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

			size_t ODCIndex;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
				LOGDEBUG("MA - Published Event - Analog Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
				auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, (opendnp3::DNPTime)now); // We don't get time info from MD3, so add it as soon as possible
				event->SetPayload<EventType::Analog>(std::move(wordres));
				PublishEvent(event);
			}
		}
		else if (GetCounterValueUsingMD3Index(maddress, idx,wordres,hasbeenset))
		{
			wordres += AnalogDeltaValues[i]; // Add the signed delta.
			SetCounterValueUsingMD3Index(maddress, idx, wordres);

			LOGDEBUG("MA - Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value 0x" + to_hexstring(wordres));

			size_t ODCIndex;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled,wordres, now);
				LOGDEBUG("MA - Published Event - Counter Index " + std::to_string(ODCIndex) + " Value 0x" + to_hexstring(wordres));
				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, (msSinceEpoch_t)now); // We don't get time info from MD3, so add it as soon as possible);
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
bool MD3MasterPort::ProcessAnalogNoChangeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an analog no change with extra data - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Doing Analog NoChange processing ");

	// Now take the returned values and store them into the points
	uint16_t wordres = 0;
	bool hasbeenset;

	//TODO: ANALOG_NO_CHANGE_REPLY do we update the times on the points that we asked to be updated?

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres,hasbeenset);
	MD3Time now = MD3Now();

	for (int i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		int idx = FirstModuleIsCounterModule ? i % 8 : i;
		int maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress + 1 : ModuleAddress;
		/*
		if (SetAnalogValueUsingMD3Index(maddress, idx, ))
		{
		      // We have succeeded in setting the value
		      int intres;
		      if (GetAnalogODCIndexUsingMD3Index(maddress, idx, intres))
		      {
		            QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i], now);
		                  auto event = std::make_shared<EventInfo>(EventType::Analog, intres, Name, qual, (msSinceEpoch_t)now); // We don't get time info from MD3, so add it as soon as possible);
		                  event->SetPayload<EventType::Analog>(std::move(AnalogValues[i]));
		                  PublishEvent(event);
		      }
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
		      // We have succeeded in setting the value
		      int intres;
		      if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
		      {
		            QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i], now);
		                  auto event = std::make_shared<EventInfo>(EventType::Counter, intres, Name, qual, (msSinceEpoch_t)now); // We don't get time info from MD3, so add it as soon as possible);
		                  event->SetPayload<EventType::Counter>(std::move(AnalogValues[i]));
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
		*/
	}
	return true;
}

bool MD3MasterPort::ProcessDigitalNoChangeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received a digital no change with extra data - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Doing Digital NoChange processing... ");

	//TODO: DIGITAL_NO_CHANGE_REPLY do we update the times on the points that we asked to be updated?

	MD3Time now = MD3Now();

	return true;
}
bool MD3MasterPort::ProcessDigitalScan(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	MD3BlockFn11StoM Fn11Header = static_cast<MD3BlockFn11StoM>(Header);

	uint8_t EventCnt = Fn11Header.GetTaggedEventCount();
	uint8_t SeqNum = Fn11Header.GetDigitalSequenceNumber();
	uint8_t ModuleCount = Fn11Header.GetModuleCount();
	int MessageIndex = 1; // What block are we processing?
	LOGDEBUG("Digital Scan (new) processing EventCnt : "+std::to_string(EventCnt)+" ModuleCnt : "+std::to_string(ModuleCount)+" Sequence Number : " + std::to_string(SeqNum));
	LOGDEBUG("Digital Scan Data " + MD3MessageAsString(CompleteMD3Message));

	//TODO: We may have to reorder the processing of the "now" module data, and the time tagged data, so that the ODC events go through in chronological order,
	//TODO: Run a test on MOSAIC to see what it does with out of order binary events...

	// Now take the returned values and store them into the points
	// Process any module data (if any)
	//NOTE: The module data on initial scan might have two blocks for the one address - the previous and current state????
	for (int m = 0; m < ModuleCount; m++)
	{
		if (MessageIndex < (int)CompleteMD3Message.size())
		{
			// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
			uint8_t ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(0);
			uint8_t msecOffset = CompleteMD3Message[MessageIndex].GetByte(1); // Will always be 0 for Module blocks
			uint16_t ModuleData = CompleteMD3Message[MessageIndex].GetSecondWord();

			if (ModuleAddress == 0 && msecOffset == 0)
			{
				// This is a status block.
				ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(2);
				uint8_t ModuleFailStatus = CompleteMD3Message[MessageIndex].GetByte(3);
				LOGDEBUG("Received a Fn 11 Status Block - Module Address : " + std::to_string(ModuleAddress) + " Fail Status : 0x" + to_hexstring(ModuleFailStatus));
			}
			else
			{
				LOGDEBUG("Received a Fn 11 Data Block - Module : " + std::to_string(ModuleAddress) + " Data : 0x" + to_hexstring(ModuleData) + " Data : " + to_binstring(ModuleData));
				MD3Time eventtime = MD3Now();

				GenerateODCEventsFromMD3ModuleWord(ModuleData, ModuleAddress, eventtime);
			}
		}
		else
		{
			LOGERROR("Tried to read a block past the end of the message processing module data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteMD3Message.size()));
			return false;
		}
		MessageIndex++;
	}

	if (EventCnt > 0) // Process Events (if any)
	{
		MD3Time timebase = 0; // Moving time value for events received

		// Process Time/Date Data
		if (MessageIndex < (int)CompleteMD3Message.size())
		{
			timebase = (uint64_t)CompleteMD3Message[MessageIndex].GetData() * 1000; //MD3Time msec since Epoch.
			LOGDEBUG("Fn11 TimeDate Packet Local : " + to_timestringfromMD3time(timebase));
		}
		else
		{
			LOGERROR("Tried to read a block past the end of the message processing time date data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteMD3Message.size()));
			return false;
		}
		MessageIndex++;

		// Now we have to convert the remaining data blocks into an array of words and process them. This is due to the time offset blocks which are only 16 bits long.
		std::vector<uint16_t> ResponseWords;
		while (MessageIndex < (int)CompleteMD3Message.size())
		{
			ResponseWords.push_back(CompleteMD3Message[MessageIndex].GetFirstWord());
			ResponseWords.push_back(CompleteMD3Message[MessageIndex].GetSecondWord());
			MessageIndex++;
		}

		// Now process the response words.
		for (int i = 0; i < (int)ResponseWords.size(); i++)
		{
			// If we are processing a data block and the high byte will be non-zero.
			// If it is zero it could be either:
			// A time offset word or
			// If the whole word is zero, then it must be the first word of a Status block.

			if (ResponseWords[i] == 0)
			{
				// We have received a STATUS block, which has a following word.
				i++;
				if (i >= (int)ResponseWords.size())
				{
					// We likely received an all zero padding block at the end of the message. Ignore this as an error
					LOGDEBUG("Fn11 Zero padding end word detected - ignoring");
					return true;
				}
				//TODO: Handle status block returned in Fn11 processing.
				LOGDEBUG("Fn11 Status Block detected - not handled yet!!!");
			}
			else if ((ResponseWords[i] & 0xFF00) == 0)
			{
				// We have received a TIME BLOCK (offset) which is a single word.
				int msecoffset = (ResponseWords[i] & 0x00ff) * 256;
				timebase += msecoffset;
				LOGDEBUG("Fn11 TimeOffset : " + std::to_string(msecoffset) +" msec");
			}
			else
			{
				// We have received a DATA BLOCK which has a following word.
				// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
				uint8_t ModuleAddress = (ResponseWords[i] >> 8) & 0x007f;
				uint8_t msecoffset = ResponseWords[i] & 0x00ff;
				timebase += msecoffset; // Update the current tagged time
				i++;

				if (i >= (int)ResponseWords.size())
				{
					// Index error
					LOGERROR("Tried to access past the end of the response words looking for the second part of a data block " + MD3MessageAsString(CompleteMD3Message));
					return false;
				}
				uint16_t ModuleData = ResponseWords[i];

				LOGDEBUG("Fn11 TimeTagged Block - Module : " + std::to_string(ModuleAddress) + " msec offset : " + std::to_string(msecoffset) + " Data : 0x" + to_hexstring(ModuleData));
				GenerateODCEventsFromMD3ModuleWord(ModuleData, ModuleAddress,timebase);
			}
		}
	}
	return true;
}
void MD3MasterPort::GenerateODCEventsFromMD3ModuleWord(const uint16_t &ModuleData, const uint8_t &ModuleAddress, const MD3Time &eventtime)
{
	LOGDEBUG("Master Generate Events,  Module : "+std::to_string(ModuleAddress)+" Data : 0x" + to_hexstring(ModuleData));

	for (uint8_t idx = 0; idx < 16; idx++)
	{
		// When we set the value it tells us if we really did set the value, or it was already at that value.
		// Only fire the ODC event if the value changed.
		bool valuechanged = false;
		uint8_t bitvalue = (ModuleData >> (15 - idx)) & 0x0001;

		bool res = SetBinaryValueUsingMD3Index(ModuleAddress, idx, bitvalue, valuechanged);

		if (res && valuechanged)
		{
			size_t ODCIndex = 0;
			if (GetBinaryODCIndexUsingMD3Index(ModuleAddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateBinaryQuality(enabled, eventtime);
				LOGDEBUG("Published Event - Binary Index " + std::to_string(ODCIndex) + " Value " + std::to_string(bitvalue));
				auto event = std::make_shared<EventInfo>(EventType::Binary, ODCIndex, Name, qual, (msSinceEpoch_t)eventtime);
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
bool MD3MasterPort::ProcessDOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an DOM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Master Got DOM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessPOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an POM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Master Got POM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessAOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an AOM response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Master Got AOM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool MD3MasterPort::ProcessSetDateTimeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SetDateTime response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Got SetDateTime response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessSetDateTimeNewReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SetDateTimeNew response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Got SetDateTimeNew response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool MD3MasterPort::ProcessSystemSignOnReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetStationAddress();

	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SystemSignOn response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}
	MD3BlockFn40MtoS resp(Header);
	if (!resp.IsValid())
	{
		LOGERROR("Received an SystemSignOn response that was not valid - ignoring - On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}
	LOGDEBUG("Got SystemSignOn response ");

	return (Header.GetFunctionCode() == SYSTEM_SIGNON_CONTROL);
}
bool MD3MasterPort::ProcessFreezeResetReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an Freeze-Reset response longer than one block - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Got Freeze-Reset response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool MD3MasterPort::ProcessFlagScanReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	// The contents of the flag scan return can be contract dependent. There can be more than the single (default) block returned...
	// We are only looking at the standard megadata flags here SPU, STI and FUP
	// SPU - Bit 7, MegaDataFlags below - System Powered Up
	// STI - Bit 6, MegaDataFlags below - System Time Incorrect
	// FUP - Bit 5, MegaDataFlags below - File upload pending

	// If any bit in the 16 bit status word changes (in the RTU), then the RSF flag bit (present in some messages) will be set.
	// Executing a FlagScan will reset this bit in the RTU, so will then be 0 in those messages.

	//uint8_t ContractFlags = Header.GetByte(3);
	//uint8_t MegaDataFlags = Header.GetByte(2);

	// Use the block access method to get at the bits.
	MD3BlockFn52StoM Header52(Header);

	bool SystemPoweredUpFlag = Header52.GetSystemPoweredUpFlag();
	bool SystemTimeIncorrectFlag = Header52.GetSystemTimeIncorrectFlag();

	//TODO: Trigger a full scan if we detect system powered up flag
	//TODO: Trigger a time send if we detect an incorrect system time flag

	LOGDEBUG("Got Flag Scan response, " + std::to_string(CompleteMD3Message.size()) + " blocks, System Powered Up Flag : "+std::to_string(SystemPoweredUpFlag) + ", System TimeIncorrect Flag : " + std::to_string(SystemTimeIncorrectFlag));

	return (Header.GetFunctionCode() == SYSTEM_FLAG_SCAN);
}

#pragma endregion

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void MD3MasterPort::DoPoll(uint32_t pollgroup)
{
	if (!enabled) return;
	LOGDEBUG("DoPoll : " + std::to_string(pollgroup));

	if (MyPointConf()->PollGroups[pollgroup].polltype == AnalogPoints)
	{
		ModuleMapType::iterator mait = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

		// We will scan a maximum of 1 module, up to 16 channels. It might spill over into the next module if the module is a counter with only 8 channels.
		int ModuleAddress = mait->first;
		int Channels = mait->second;

		if (MyPointConf()->PollGroups[pollgroup].ModuleAddresses.size() > 1)
		{
			LOGERROR("Analog Poll group "+std::to_string(pollgroup)+" is configured for more than one MD3 address. Please create another poll group.");
		}

		// We need to do an analog unconditional on start up, until all the points have a valid value - even 0x8000 for does not exist.
		//TODO: To do this we check the hasbeenset flag, which will be false on start up, and also set to false on comms lost event - kind of like a quality.
		bool UnconditionalCommandRequired = false;
		for (int idx = 0; idx < Channels; idx++)
		{
			uint16_t wordres;
			bool hasbeenset;
			bool res = GetAnalogValueUsingMD3Index(ModuleAddress, idx, wordres, hasbeenset);
			if (res && !hasbeenset)
				UnconditionalCommandRequired = true;
		}

		if (UnconditionalCommandRequired || MyPointConf()->PollGroups[pollgroup].ForceUnconditional)
		{
			// Use Unconditional Request Fn 5
			LOGDEBUG("Poll Issued a Analog Unconditional Command");

			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL, ModuleAddress, Channels, true);
			QueueMD3Command(commandblock,nullptr);
		}
		else
		{
			LOGDEBUG("Poll Issued a Analog Delta Command");
			// Use a delta command Fn 6
			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_DELTA_SCAN, ModuleAddress, Channels, true);
			QueueMD3Command(commandblock, nullptr);
		}
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == BinaryPoints)
	{
		if (MyPointConf()->NewDigitalCommands) // Old are 7,8,9,10 - New are 11 and 12
		{
			// If sequence number is zero - it means we have just started up, or communications re-established. So we dont have a full copy
			// of the binary data (timetagged or otherwise). The outStation will use the zero sequnce number to send everything to initialise us. We
			// don't have to send an unconditional.

			ModuleMapType::iterator FirstModule = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

			// Request Digital Unconditional
			int ModuleAddress = FirstModule->first;
			// We expect the digital modules to be consecutive, or of there is a gap this will still work.
			int Modules = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.size(); // Most modules we can get in one command - NOT channels!

			bool UnconditionalCommandRequired = false;
			for (int m = 0; m < Modules; m++)
			{
				for (int idx = 0; idx < 16; idx++)
				{
					bool hasbeenset;
					bool res = GetBinaryQualityUsingMD3Index(ModuleAddress + m, idx, hasbeenset);
					if (res && !hasbeenset)
						UnconditionalCommandRequired = true;
				}
			}
			MD3BlockFormatted commandblock;

			// Also need to check if we already have all the values that this command would ask for..if not send unconditional.
			if (UnconditionalCommandRequired || MyPointConf()->PollGroups[pollgroup].ForceUnconditional)
			{
				// Use Unconditional Request Fn 12
				//TODO:  Handle for than one DIM in a poll group...
				LOGDEBUG("Poll Issued a Digital Unconditional (new) Command");

				commandblock = MD3BlockFn12MtoS(MyConf()->mAddrConf.OutstationAddr, ModuleAddress, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
			}
			else
			{
				// Use a delta command Fn 11
				LOGDEBUG("Poll Issued a Digital Delta COS (new) Command");

				uint8_t TaggedEventCount = 0; // Assuming no timetagged points initially.

				// If we have timetagged points in the system, then we need to ask for them to be returned.
				if (MyPointConf()->PollGroups[pollgroup].TimeTaggedDigital == true)
					TaggedEventCount = 15; // The most we can ask for

				commandblock = MD3BlockFn11MtoS(MyConf()->mAddrConf.OutstationAddr, TaggedEventCount, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
			}

			QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
		}
		else // Old digital commands
		{
			if (MyPointConf()->PollGroups[pollgroup].ForceUnconditional)
			{
				// Use Unconditional Request Fn 7
				//TODO:  Handle for than one DIM in a poll group...
				LOGDEBUG("Poll Issued a Digital Unconditional (old) Command");
				ModuleMapType::iterator mait = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

				// Request Digital Unconditional
				int ModuleAddress = mait->first;
				int channels = 16; // Most we can get in one command
				MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, DIGITAL_UNCONDITIONAL_OBS, ModuleAddress, channels, true);

				QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
			}
			else
			{
				// Use a delta command Fn 8
				LOGDEBUG("Poll Issued a Digital Delta (old) Command");
			}
		}
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == TimeSetCommand)
	{
		// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
		uint64_t currenttime = MD3Now();

		LOGDEBUG("Poll Issued a TimeDate Command");
		SendTimeDateChangeCommand(currenttime, nullptr);
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == NewTimeSetCommand)
	{
		// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
		uint64_t currenttime = MD3Now();

		LOGDEBUG("Poll Issued a NewTimeDate Command");
		int utcoffsetminutes = tz_offset();

		SendNewTimeDateChangeCommand(currenttime, utcoffsetminutes, nullptr);
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == SystemFlagScan)
	{
		// Send a flag scan command to the OutStation, (Fn 52)
		LOGDEBUG("Poll Issued a System Flag Scan Command");
		SendSystemFlagScanCommand(nullptr);
	}
}
void MD3MasterPort::ResetDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);
	DigitalCommandSequenceNumber = 0;
}
// Manage the access to the command sequence number. Very low possibility of contention, so use standard lock.
int MD3MasterPort::GetAndIncrementDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);

	int retval = DigitalCommandSequenceNumber;

	if (DigitalCommandSequenceNumber == 0) DigitalCommandSequenceNumber++; // If we send 0, we will get a sequence number of 1 back. So need to move on to 2 for next command

	DigitalCommandSequenceNumber = (++DigitalCommandSequenceNumber > 15) ? 1 : DigitalCommandSequenceNumber;
	return retval;
}
void MD3MasterPort::EnablePolling(bool on)
{
	if (on)
		PollScheduler->Start();
	else
		PollScheduler->Stop();
}
void MD3MasterPort::SendTimeDateChangeCommand(const uint64_t &currenttimeinmsec, SharedStatusCallback_t pStatusCallback)
{
	MD3BlockFn43MtoS commandblock(MyConf()->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
	MD3BlockData datablock((uint32_t)(currenttimeinmsec / 1000), true);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendNewTimeDateChangeCommand(const uint64_t &currenttimeinmsec, int utcoffsetminutes, SharedStatusCallback_t pStatusCallback)
{
	MD3BlockFn44MtoS commandblock(MyConf()->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
	MD3BlockData datablock((uint32_t)(currenttimeinmsec / 1000));
	MD3BlockData datablock2((uint32_t)(utcoffsetminutes << 16), true);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	Cmd.push_back(datablock2);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendSystemFlagScanCommand(SharedStatusCallback_t pStatusCallback)
{
	MD3BlockFn52MtoS commandblock(MyConf()->mAddrConf.OutstationAddr);
	QueueMD3Command(commandblock, pStatusCallback);
}

void MD3MasterPort::SetAllPointsQualityToCommsLost()
{
	LOGDEBUG("MD3 Master setting quality to comms lost");

	auto eventbinary = std::make_shared<EventInfo>(EventType::BinaryQuality, 0, Name, QualityFlags::COMM_LOST);
	eventbinary->SetPayload<EventType::BinaryQuality>(QualityFlags::COMM_LOST);

	// Loop through all Binary points.
	for (auto const &Point : MyPointConf()->BinaryODCPointMap)
	{
		int index = Point.first;
		eventbinary->SetIndex(index);
		PublishEvent(eventbinary);
	}
	// Analogs

	auto eventanalog = std::make_shared<EventInfo>(EventType::AnalogQuality, 0, Name, QualityFlags::COMM_LOST);
	eventanalog->SetPayload<EventType::AnalogQuality>(QualityFlags::COMM_LOST);
	//TODO: Set all analog points to notset - should this be merged with quality? so that we can determine when we need to send an unconditional command.
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		if (!SetAnalogValueUsingODCIndex(index, (uint16_t)0x8000))
			LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));

		eventanalog->SetIndex(index);
		PublishEvent(eventanalog);
	}
	// Counters
	auto eventcounter = std::make_shared<EventInfo>(EventType::CounterQuality, 0, Name, QualityFlags::COMM_LOST);
	eventcounter->SetPayload<EventType::CounterQuality>(QualityFlags::COMM_LOST);
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		if (!SetCounterValueUsingODCIndex(index, (uint16_t)0x8000))
			LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));

		eventcounter->SetIndex(index);
		PublishEvent(eventcounter);
	}
	/* Not applicable...// Binary Control/Output
	auto event = std::make_shared<EventInfo>(EventType::BinaryOutputStatusQuality, 0, Name, QualityFlags::COMM_LOST);
	event->SetPayload<EventType::BinaryOutputStatusQuality>(QualityFlags::COMM_LOST);
	for (auto const &Point : MyPointConf()->BinaryControlODCPointMap)
	{
	      int index = Point.first;

	      event->SetIndex(index);
	      PublishEvent(event);
	}*/
}

// When a new device connects to us through ODC (or an existing one reconnects), send them everything we currently have.
void MD3MasterPort::SendAllPointEvents()
{
	//TODO: SJE Set a quality of RESTART if we have just started up but not yet received information for a point. Not sure if super usefull...

	// Quality of ONLINE means the data is GOOD.
	for (auto const &Point : MyPointConf()->BinaryODCPointMap)
	{
		int index = Point.first;
		uint8_t meas = Point.second->Binary;
		QualityFlags qual = CalculateBinaryQuality(enabled, Point.second->ChangedTime);

		auto event = std::make_shared<EventInfo>(EventType::Binary, index, Name, qual, (msSinceEpoch_t)Point.second->ChangedTime);
		event->SetPayload<EventType::Binary>(meas == 1);
		PublishEvent(event);
	}

	// Analogs
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.second->ChangedTime);

		auto event = std::make_shared<EventInfo>(EventType::Analog, index, Name, qual, (msSinceEpoch_t)Point.second->ChangedTime);
		event->SetPayload<EventType::Analog>(std::move(meas));
		PublishEvent(event);
	}
	// Counters
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.second->ChangedTime);

		auto event = std::make_shared<EventInfo>(EventType::Counter, index, Name, qual, (msSinceEpoch_t)Point.second->ChangedTime);
		event->SetPayload<EventType::Counter>(std::move(meas));
		PublishEvent(event);
	}
}

// Binary quality only depends on our link status and if we have received data
QualityFlags MD3MasterPort::CalculateBinaryQuality(bool enabled, MD3Time time)
{
	return (enabled ? ((time == 0) ? QualityFlags::RESTART : QualityFlags::ONLINE) : QualityFlags::COMM_LOST);
}
// Use the measurement value and if we are enabled to determine what the quality value should be.
QualityFlags MD3MasterPort::CalculateAnalogQuality(bool enabled, uint16_t meas, MD3Time time)
{
	return (enabled ? (time == 0 ? QualityFlags::RESTART : ((meas == 0x8000) ? QualityFlags::LOCAL_FORCED : QualityFlags::ONLINE)) : QualityFlags::COMM_LOST);
}


// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
// Also we have the pass through commands with special port values defined.
void MD3MasterPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
	switch (event->GetEventType())
	{
		case EventType::ControlRelayOutputBlock:
			return WriteObject(event->GetPayload<EventType::ControlRelayOutputBlock>(), (uint16_t)event->GetIndex(), pStatusCallback);
		case EventType::AnalogOutputInt16:
			return WriteObject(event->GetPayload<EventType::AnalogOutputInt16>().first, (uint16_t)event->GetIndex(), pStatusCallback);
		case EventType::AnalogOutputInt32:
			return WriteObject(event->GetPayload<EventType::AnalogOutputInt32>().first, (uint16_t)event->GetIndex(), pStatusCallback);
		case EventType::AnalogOutputFloat32:
			return WriteObject(event->GetPayload<EventType::AnalogOutputFloat32>().first, (uint16_t)event->GetIndex(), pStatusCallback);
		case EventType::AnalogOutputDouble64:
			return WriteObject(event->GetPayload<EventType::AnalogOutputDouble64>().first, (uint16_t)event->GetIndex(), pStatusCallback);
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();
			// This will be fired by (typically) an MD3OutStation port on the "other" side of the ODC Event bus. i.e. something upstream has connected
			// We should probably send all the points to the Outstation as we don't know what state the OutStation point table will be in.

			if (state == ConnectState::CONNECTED)
			{
				LOGDEBUG("Upstream (other side of ODC) port enabled - Triggering sending of current data ");
				// We don’t know the state of the upstream data, so send event information for all points.
				SendAllPointEvents();
			}
			else if (state == ConnectState::DISCONNECTED)
			{
				// If we were an on demand connection, we would take down the connection . For MD3 we are using persistent connections only.
				// We have lost an ODC connection, so events we send don't go anywhere.
			}

			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}
}


void MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	uint8_t ModuleAddress = 0;
	uint8_t Channel = 0;
	BinaryPointType PointType;
	bool exists = GetBinaryControlMD3IndexUsingODCIndex(index, ModuleAddress, Channel, PointType);

	if (!exists)
	{
		LOGDEBUG("Master received a DOM/POM ODC Change Command on a point that is not defined " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	std::string OnOffString = "ON";

	if (PointType == DOMOUTPUT)
	{
		// The main issue is that a DOM command writes 16 bits in one go, so we have to remember the output state of the other bits,
		// so that we only write the change that has been triggered.
		// We have to gather up the current state of those bits.

		bool ModuleFailed = false;                                                    // Not used - yet
		uint16_t outputbits = CollectModuleBitsIntoWord(ModuleAddress, ModuleFailed); // Reads from the digital input point list...

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

		LOGDEBUG("Master received a DOM ODC Change Command - Index: " + std::to_string(index) +" - "+ OnOffString + "  Module/Channel " + std::to_string(ModuleAddress) + "/" + std::to_string(Channel));
		SendDOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, ModuleAddress, outputbits, pStatusCallback);
	}
	else if(PointType == POMOUTPUT)
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

		LOGDEBUG("Master received a POM ODC Change Command - Index: " + std::to_string(index) + " - " + OnOffString + "  Module/Channel " + std::to_string(ModuleAddress) + "/" + std::to_string(Channel));
		SendPOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, ModuleAddress, outputselection, pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received Binary Output ODC Event on a point not defined as DOM or POM " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
}

void MD3MasterPort::WriteObject(const int16_t & command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// AOM Command
	LOGDEBUG("Master received a AOM ODC Change Command " + std::to_string(index));

//TODO: Finish AOM command	SendDOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputFromSecondBlock(BlockData), pStatusCallback);

	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}


void MD3MasterPort::WriteObject(const int32_t & command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// Other Magic point commands

	if (index == MyPointConf()->POMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received POM Control Point command on the magic point through ODC " + std::to_string(index));

		MD3BlockFn17MtoS Header = MD3BlockFn17MtoS(MD3BlockData(command));
		SendPOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputSelection(), pStatusCallback);
	}
	else if (index == MyPointConf()->SystemSignOnPoint.second)
	{
		// Normally a Fn40
		LOGDEBUG("Master received System Sign On command on the magic point through ODC " + std::to_string(index));
		MD3BlockFormatted MD3command(command, true);
		QueueMD3Command(MD3command, pStatusCallback); // Single block send
	}
	else if (index == MyPointConf()->FreezeResetCountersPoint.second)
	{
		// Normally a Fn16
		LOGDEBUG("Master received Freeze/Reset Counters command on the magic point through ODC " + std::to_string(index));
		MD3BlockFormatted MD3command(command,true);   // The packet rx'd by the outstation and passed to us through ODC is sent out unchanged by the master...
		QueueMD3Command(MD3command, pStatusCallback); // Single block send
	}
	else if (index == MyPointConf()->DOMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received DOM Control Point command on the magic point through ODC " + std::to_string(index));
		uint32_t PacketData = static_cast<uint32_t>(command);
		MD3BlockFn19MtoS Header = MD3BlockFn19MtoS((PacketData >> 24) & 0x7F, (PacketData >> 16) & 0xFF);
		MD3BlockData BlockData = Header.GenerateSecondBlock(PacketData & 0xFFFF);

		SendDOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputFromSecondBlock(BlockData), pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received unknown AnalogOutputInt32 ODC Event " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
	}
}

void MD3MasterPort::WriteObject(const float& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGERROR("On Master float Type is not implemented " + std::to_string(index));
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

void MD3MasterPort::WriteObject(const double& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	if (index == MyPointConf()->TimeSetPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received a Time Change command on the magic point through ODC " + std::to_string(index));
		uint64_t currenttime = static_cast<uint64_t>(command);

		SendTimeDateChangeCommand(currenttime, pStatusCallback);
	}
	else if(index == MyPointConf()->TimeSetPointNew.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received a New Time Change command on the magic point through ODC " + std::to_string(index));
		uint64_t currenttime = static_cast<uint64_t>(command);
		int utcoffsetminutes = tz_offset();
		SendNewTimeDateChangeCommand(currenttime, utcoffsetminutes, pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received unknown double ODC Event " + std::to_string(index));
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
	}
}

void MD3MasterPort::SendDOMOutputCommand(const uint8_t &StationAddress, const uint8_t &ModuleAddress, const uint16_t &outputbits, const SharedStatusCallback_t &pStatusCallback)
{
	MD3BlockFn19MtoS commandblock(StationAddress, ModuleAddress );
	MD3BlockData datablock = commandblock.GenerateSecondBlock(outputbits);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendPOMOutputCommand(const uint8_t &StationAddress, const uint8_t &ModuleAddress, const uint8_t &outputselection, const SharedStatusCallback_t &pStatusCallback)
{
	MD3BlockFn17MtoS commandblock(StationAddress, ModuleAddress, outputselection);
	MD3BlockData datablock = commandblock.GenerateSecondBlock();

	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendAOMOutputCommand(const uint8_t &StationAddress, const uint8_t &ModuleAddress, const uint8_t &Channel, const uint16_t &value, const SharedStatusCallback_t &pStatusCallback)
{
	MD3BlockFn23MtoS commandblock(StationAddress, ModuleAddress, Channel);
	MD3BlockData datablock = commandblock.GenerateSecondBlock(value);

	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
}



