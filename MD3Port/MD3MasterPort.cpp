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

#include "MD3.h"
#include "MD3MasterPort.h"
#include "MD3Utility.h"
#include <array>
#include <chrono>
#include <thread>
#include <utility>


MD3MasterPort::MD3MasterPort(const std::string &aName, const std::string &aConfFilename, const Json::Value &aConfOverrides):
	MD3Port(aName, aConfFilename, aConfOverrides),
	PollScheduler(nullptr)
{
	std::string over = "None";
	if (aConfOverrides.isObject()) over = aConfOverrides.toStyledString();

	IsOutStation = false;

	LOGDEBUG("{} MD3Master Constructor {}  Overrides - {}", Name, aConfFilename, over);
}

MD3MasterPort::~MD3MasterPort()
{
	Disable();
	MD3Connection::RemoveMaster(pConnection,MyConf->mAddrConf.OutstationAddr);
	// The pConnection will be closed by this point, so is not holding any resources, and will be freed on program close when the static list is destroyed.
}

void MD3MasterPort::Enable()
{
	if (enabled.exchange(true)) return;
	try
	{
		MD3Connection::Open(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
	}
	catch (std::exception& e)
	{
		LOGERROR("{} Problem opening connection TCP : {}", Name, e.what());
		enabled = false;
		return;
	}
}
void MD3MasterPort::Disable()
{
	if (!enabled.exchange(false)) return;

	MD3Connection::Close(pConnection); // Any outstation can take the port down and back up - same as OpenDNP operation for multidrop
}

// Have to fire the SocketStateHandler for all other OutStations sharing this socket.
void MD3MasterPort::SocketStateHandler(bool state)
{
	if (!enabled.load()) return; // Port Disabled so dont process

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
	std::string ChannelID = MyConf->mAddrConf.ChannelID();

	PollScheduler = std::make_unique<ASIOScheduler>(*pIOS);

	MasterCommandProtectedData.CurrentCommandTimeoutTimer = pIOS->make_steady_timer();
	MasterCommandStrand = pIOS->make_strand();

	// Need a couple of things passed to the point table.
	MyPointConf->PointTable.Build(IsOutStation, MyPointConf->NewDigitalCommands, *pIOS);

	// Creates internally if necessary, returns a token for the connection
	pConnection = MD3Connection::AddConnection(pIOS, IsServer(), MyConf->mAddrConf.IP, MyConf->mAddrConf.Port, MyConf->mAddrConf.TCPConnectRetryPeriodms); //Static method


	MD3Connection::AddMaster(pConnection, MyConf->mAddrConf.OutstationAddr,
		std::bind(&MD3MasterPort::ProcessMD3Message, this, std::placeholders::_1),
		std::bind(&MD3MasterPort::SocketStateHandler, this, std::placeholders::_1));

	PollScheduler->Stop();
	PollScheduler->Clear();
	for (const auto& pg : MyPointConf->PollGroups)
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
		LOGERROR("{} MA - Tried to send an empty message to the TCP Port", Name);
		return;
	}
	LOGDEBUG("{} MA - Sending Message - {}",Name, MD3MessageAsString(CompleteMD3Message));

	// Done this way just to get context into log messages.
	MD3Port::SendMD3Message(CompleteMD3Message);
}

#ifdef _MSC_VER
#pragma region MasterCommandQueue
#endif

// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
// There is a fixed timeout function (below) which will queue the next command if we timeout.
// If the ProcessMD3Message callback gets the command it expects, it will send the next command in the queue.
// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
void MD3MasterPort::QueueMD3Command(const MD3Message_t &CompleteMD3Message, const SharedStatusCallback_t& pStatusCallback)
{
	MasterCommandStrand->dispatch([=]() // Tries to execute, if not able to will post. Note the calling thread must be one of the io_service threads.... this changes our tests!
		{
			if (MasterCommandProtectedData.MasterCommandQueue.size() < MasterCommandProtectedData.MaxCommandQueueSize)
			{
			      MasterCommandProtectedData.MasterCommandQueue.push(MasterCommandQueueItem(CompleteMD3Message, pStatusCallback)); // async
			}
			else
			{
			      LOGDEBUG("{} Tried to queue another MD3 Master Command when the command queue is full",Name);
			      PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED); // Failed...
			}

			// Will only send if we can - i.e. not currently processing a command
			UnprotectedSendNextMasterCommand(false);
		});
}
// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockData &SingleBlockMD3Message, const SharedStatusCallback_t& pStatusCallback)
{
	MD3Message_t CommandMD3Message;
	CommandMD3Message.push_back(SingleBlockMD3Message);
	QueueMD3Command(CommandMD3Message, pStatusCallback);
}
// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockFormatted &SingleBlockMD3Message, const SharedStatusCallback_t& pStatusCallback)
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
		pIOS->post([pStatusCallback, c]()
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
				LOGDEBUG("{} Sending Retry on command: {}, Retrys Remaining: {}", Name, std::to_string(MasterCommandProtectedData.CurrentFunctionCode), MasterCommandProtectedData.RetriesLeft);
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

				LOGDEBUG("{} Reached maximum number of retries on command: {}", Name, std::to_string(MasterCommandProtectedData.CurrentFunctionCode));
			}
		}

		if (!MasterCommandProtectedData.MasterCommandQueue.empty() && (MasterCommandProtectedData.ProcessingMD3Command != true))
		{
			// Send the next command if there is one and we are not retrying.

			MasterCommandProtectedData.ProcessingMD3Command = true;
			MasterCommandProtectedData.RetriesLeft = MyPointConf->MD3CommandRetries;

			MasterCommandProtectedData.CurrentCommand = MasterCommandProtectedData.MasterCommandQueue.front();
			MasterCommandProtectedData.MasterCommandQueue.pop();

			MasterCommandProtectedData.CurrentFunctionCode = MD3BlockFormatted(MasterCommandProtectedData.CurrentCommand.first[0]).GetFunctionCode();
			LOGDEBUG("{} Sending next command: {}", Name, std::to_string(MasterCommandProtectedData.CurrentFunctionCode));
		}

		// If either of the above situations need us to send a command, do so.
		if (MasterCommandProtectedData.ProcessingMD3Command == true)
		{
			SendMD3Message(MasterCommandProtectedData.CurrentCommand.first); // This should be the only place this is called for the MD3Master...

			// Start an async timed callback for a timeout - cancelled if we receive a good response.
			MasterCommandProtectedData.TimerExpireTime = std::chrono::milliseconds(MyPointConf->MD3CommandTimeoutmsec);
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
								      LOGDEBUG("{} MD3 Master Timeout valid - MD3 Function {}",Name,std::to_string(MasterCommandProtectedData.CurrentFunctionCode));

								      MasterCommandProtectedData.ProcessingMD3Command = false; // Only gets reset on success or timeout.

								      UnprotectedSendNextMasterCommand(true); // We already have the strand, so don't need the wrapper here
								}
								else
									LOGDEBUG("{} MD3 Master Timeout callback called, when we had already moved on to the next command",Name);
							});
					}
					else
					{
					      LOGDEBUG("{} MD3 Master Timeout callback cancelled",Name);
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

#ifdef _MSC_VER
#pragma endregion
#endif

#ifdef _MSC_VER
#pragma region MessageProcessing
#endif

// After we have sent a command, this is where we will end up when the OutStation replies.
// It is the callback for the ConnectionManager
// If we get the wrong answer, we could ditch that command, and move on to the next one,
// but then the actual reply might be in the following message and we would never re-sync.
// If we timeout on (some) commands, we can ask the OutStation to resend the last command response.
// We would have to limit how many times we could do this without giving up.
void MD3MasterPort::ProcessMD3Message(MD3Message_t &CompleteMD3Message)
{
	if (!enabled.load()) return; // Port Disabled so dont process

	// We know that the address matches in order to get here, and that we are in the correct INSTANCE of this class.

	//! Anywhere we find that we don't have what we need, return. If we succeed we send the next command at the end of this method.
	// If the timeout on the command is activated, then the next command will be sent - or resent.
	// Cant just use by reference, complete message and header will go out of scope...
	MasterCommandStrand->dispatch([&, CompleteMD3Message]()
		{

			if (CompleteMD3Message.size() == 0)
			{
			      LOGERROR("{} Received a Master to Station message with zero length!!! ",Name);
			      return;
			}

			MD3BlockFormatted Header = MD3BlockFormatted(CompleteMD3Message[0]);

			// If we have an error, we have to wait for the timeout to occur, there may be another packet in behind which is the correct one. If we bail now we may never re-synchronise.

			if (Header.IsMasterToStationMessage() != false)
			{
			      LOGERROR("{} Received a Master to Station message at the Master - ignoring - {} On Station Address - {}",Name, Header.GetFunctionCode() ,Header.GetStationAddress());
			      return;
			}
			if (Header.GetStationAddress() == 0)
			{
			      LOGERROR("{} Received broadcast return message - address 0 - ignoring - {} On Station Address - {}",Name, Header.GetFunctionCode(), Header.GetStationAddress());
			      return;
			}
			if (Header.GetStationAddress() != MyConf->mAddrConf.OutstationAddr)
			{
			      LOGERROR("{} Received a message from the wrong address - ignoring - {} On Station Address - {}", Name, Header.GetFunctionCode(),Header.GetStationAddress());
			      return;
			}

			LOGDEBUG("{} MD3 Master received a response to sending cmd {} of {}",Name, MasterCommandProtectedData.CurrentFunctionCode, Header.GetFunctionCode());

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
					{
					      success = ProcessDigitalScan(Header, CompleteMD3Message);
					      if (success)
					      {
					// We should trigger another scan command. Will continue until we get a DIGITAL_NO_CHANGE_REPLY or an error

					            uint8_t TaggedEventCount = 15; // Assuming there are. Will not send if there are not!
					            uint8_t Modules = 15;          // Get as many as we can...

					            auto commandblock = MD3BlockFn11MtoS(Header.GetStationAddress(), TaggedEventCount, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
					            QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
						}
					}
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
			#ifdef _MSC_VER
			#pragma warning(suppress: 26495)
			#endif
		});
}

// We have received data from an Analog command - could be  the result of Fn 5 or 6
// Store the decoded data into the point lists. Counter scan comes back in an identical format
// Return success or failure
bool MD3MasterPort::ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	uint32_t NumberOfDataBlocks = Channels / 2 + Channels % 2; // 2 --> 1, 3 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("MA - Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
		return false;
	}

	LOGDEBUG("Doing Analog Unconditional processing ");

	// Unload the analog values from the blocks
	std::vector<uint16_t> AnalogValues;
	int ChanCount = 0;
	for (uint32_t i = 0; i < NumberOfDataBlocks; i++)
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
	bool FirstModuleIsCounterModule = MyPointConf->PointTable.GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres,hasbeenset);
	MD3Time now = MD3NowUTC();

	for (uint8_t i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		uint8_t idx = FirstModuleIsCounterModule ? i % 8 : i;
		uint16_t maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress+1 : ModuleAddress;

		if (MyPointConf->PointTable.SetAnalogValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			LOGDEBUG("MA - Set Analog - Module {} Channel {} Value {}",maddress,idx, to_hexstring(AnalogValues[i]));
			size_t ODCIndex;
			if (MyPointConf->PointTable.GetAnalogODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("MA - Published Event - Analog - Index {} Value {}",ODCIndex, to_hexstring(AnalogValues[i]));

				auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible);
				event->SetPayload<EventType::Analog>(double(AnalogValues[i]));
				PublishEvent(event);
			}
		}
		else if (MyPointConf->PointTable.SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			LOGDEBUG("MA - Set Counter - Module {} Channel {} Value {}" , maddress, idx,to_hexstring(AnalogValues[i]));
			size_t ODCIndex;
			if (MyPointConf->PointTable.GetCounterODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("MA - Published Event - Counter - Index {} Value {}",ODCIndex, to_hexstring(AnalogValues[i]));
				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible);
				event->SetPayload<EventType::Counter>(uint32_t(AnalogValues[i]));
				PublishEvent(event);
			}
		}
		else
		{
			LOGERROR("MA - Fn5 Failed to set an Analog or Counter Value - {} On Station Address - {} Module : {} Channel : {}",Header.GetFunctionCode(),Header.GetStationAddress(),maddress, idx);
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

	uint8_t NumberOfDataBlocks = Channels / 4 + Channels % 4; // 2 --> 1, 5 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("MA - Received a message with the wrong number of blocks - ignoring - {} On Station Address - {}", Header.GetFunctionCode(),Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Doing Analog Delta processing ");

	// Unload the analog delta values from the blocks - 4 per block.
	std::vector<int8_t> AnalogDeltaValues;
	int ChanCount = 0;
	for (uint8_t i = 0; i < NumberOfDataBlocks; i++)
	{
		for (uint8_t j = 0; j < 4; j++)
		{
			AnalogDeltaValues.push_back(numeric_cast<char>(CompleteMD3Message[i + 1].GetByte(j)));
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
	bool FirstModuleIsCounterModule = MyPointConf->PointTable.GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres,hasbeenset);
	MD3Time now = MD3NowUTC();

	for (uint8_t i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		uint8_t idx = FirstModuleIsCounterModule ? i % 8 : i;
		uint16_t maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress + 1 : ModuleAddress;

		if (MyPointConf->PointTable.GetAnalogValueUsingMD3Index(maddress, idx, wordres,hasbeenset))
		{
			wordres += AnalogDeltaValues[i]; // Add the signed delta.
			MyPointConf->PointTable.SetAnalogValueUsingMD3Index(maddress, idx, wordres);

			LOGDEBUG("MA - Set Analog - Module {} Channel {} Value {}", maddress,idx,to_hexstring(wordres));

			size_t ODCIndex;
			if (MyPointConf->PointTable.GetAnalogODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
				LOGDEBUG("MA - Published Event - Analog Index {} Value {}", ODCIndex, to_hexstring(wordres));
				auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible
				event->SetPayload<EventType::Analog>(std::move(wordres));
				PublishEvent(event);
			}
		}
		else if (MyPointConf->PointTable.GetCounterValueUsingMD3Index(maddress, idx,wordres,hasbeenset))
		{
			wordres += AnalogDeltaValues[i]; // Add the signed delta.
			MyPointConf->PointTable.SetCounterValueUsingMD3Index(maddress, idx, wordres);

			LOGDEBUG("MA - Set Counter - Module {} Channel {} Value {}", maddress, idx, to_hexstring(wordres));

			size_t ODCIndex;
			if (MyPointConf->PointTable.GetCounterODCIndexUsingMD3Index(maddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateAnalogQuality(enabled,wordres, now);
				LOGDEBUG("MA - Published Event - Counter Index {} Value {}", ODCIndex, to_hexstring(wordres));
				auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible);
				event->SetPayload<EventType::Counter>(std::move(wordres));
				PublishEvent(event);
			}
		}
		else
		{
			LOGERROR("Fn6 Failed to set an Analog or Counter Value - {} On Station Address - {} Module : {} Channel : {}",Header.GetFunctionCode(), Header.GetStationAddress(), maddress,std::to_string(idx));
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
		LOGERROR("Received an analog no change with extra data - ignoring - {} On Station Address - {}", Header.GetFunctionCode(),Header.GetStationAddress());
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
		bool FirstModuleIsCounterModule = MyPointConf->PointTable.GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres, hasbeenset);

		MD3Time now = MD3NowUTC();

		for (uint8_t i = 0; i < Channels; i++)
		{
			// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
			// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
			// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
			uint8_t idx = FirstModuleIsCounterModule ? i % 8 : i;
			uint8_t maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress + 1 : ModuleAddress;

			if (MyPointConf->PointTable.GetAnalogValueUsingMD3Index(maddress, idx, wordres, hasbeenset))
			{
				MyPointConf->PointTable.SetAnalogValueUsingMD3Index(maddress, idx, wordres);

				LOGDEBUG("MA - Set Analog - Module {} Channel {} Value {}",maddress, idx, to_hexstring(wordres));

				size_t ODCIndex;
				if (MyPointConf->PointTable.GetAnalogODCIndexUsingMD3Index(maddress, idx, ODCIndex))
				{
					QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
					LOGDEBUG("MA - Published Event - Analog Index {} Value {}",ODCIndex, to_hexstring(wordres));
					auto event = std::make_shared<EventInfo>(EventType::Analog, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible
					event->SetPayload<EventType::Analog>(std::move(wordres));
					PublishEvent(event);
				}
			}
			else if (MyPointConf->PointTable.GetCounterValueUsingMD3Index(maddress, idx, wordres, hasbeenset))
			{
				// Get the value and set it again, but with a new time.
				MyPointConf->PointTable.SetCounterValueUsingMD3Index(maddress, idx, wordres); // This updates the time
				LOGDEBUG("MA - Set Counter - Module {} Channel {} Value {}",maddress,idx,to_hexstring(wordres));

				size_t ODCIndex;
				if (MyPointConf->PointTable.GetCounterODCIndexUsingMD3Index(maddress, idx, ODCIndex))
				{
					QualityFlags qual = CalculateAnalogQuality(enabled, wordres, now);
					LOGDEBUG("MA - Published Event - Counter Index {} Value {}",ODCIndex, to_hexstring(wordres));
					auto event = std::make_shared<EventInfo>(EventType::Counter, ODCIndex, Name, qual, static_cast<msSinceEpoch_t>(now)); // We don't get time info from MD3, so add it as soon as possible);
					event->SetPayload<EventType::Counter>(std::move(wordres));
					PublishEvent(event);
				}
			}
			else
			{
				LOGERROR("Fn5 Failed to set an Analog or Counter Time - {} On Station Address - {} Module : {} Channel : {}",Header.GetFunctionCode(), Header.GetStationAddress(), maddress,idx);
				return false;
			}
		}
	}
	return true;
}

bool MD3MasterPort::ProcessDigitalNoChangeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received a digital no change with extra data - ignoring - {} On Station Address - {}",Header.GetFunctionCode(), Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Doing Digital NoChange processing... ");

	// DIGITAL_NO_CHANGE_REPLY we do not update time on digital points

	return true;
}
bool MD3MasterPort::ProcessDigitalScan(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	MD3BlockFn11StoM Fn11Header = static_cast<MD3BlockFn11StoM>(Header);

	uint8_t EventCnt = Fn11Header.GetTaggedEventCount();
	uint8_t SeqNum = Fn11Header.GetDigitalSequenceNumber();
	uint8_t ModuleCount = Fn11Header.GetModuleCount();
	size_t MessageIndex = 1; // What block are we processing?
	LOGDEBUG("Digital Scan (new) processing EventCnt : {} ModuleCnt : {} Sequence Number : {}",EventCnt, ModuleCount,SeqNum);
	LOGDEBUG("Digital Scan Data {}", MD3MessageAsString(CompleteMD3Message));

	// COS means that we could send out of order ODC Binary events. We dont need to worry about this.
	// The outstation will not process an event that is "older" than the last event it received. (It will store it in the COS queue.

	// Now take the returned values and store them into the points
	// Process any module data (if any)
	//NOTE: The module data on initial scan might have two blocks for the one address - the previous and current state????
	for (int m = 0; m < ModuleCount; m++)
	{
		if (MessageIndex < CompleteMD3Message.size())
		{
			// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
			uint8_t ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(0);
			uint8_t msecOffset = CompleteMD3Message[MessageIndex].GetByte(1); // Will always be 0 for Module blocks
			uint16_t ModuleData = CompleteMD3Message[MessageIndex].GetSecondWord();

			if (ModuleAddress == 0 && msecOffset == 0)
			{
				// This is a status or if module address is 0 a flag block.
				ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(2);
				uint8_t ModuleFailStatus = CompleteMD3Message[MessageIndex].GetByte(3);
				LOGDEBUG("Received a Fn 11 Status or Flag Block (addr=0) - We do not process - Module Address : {} Fail Status : {}",ModuleAddress,to_hexstring(ModuleFailStatus));
			}
			else
			{
				LOGDEBUG("Received a Fn 11 Data Block - Module : {} Data : {} Data : {}",ModuleAddress,to_hexstring(ModuleData),to_binstring(ModuleData));
				MD3Time eventtime = MD3NowUTC();

				GenerateODCEventsFromMD3ModuleWord(ModuleData, ModuleAddress, eventtime);
			}
		}
		else
		{
			LOGERROR("Tried to read a block past the end of the message processing module data : {} Modules : {}",MessageIndex,CompleteMD3Message.size());
			return false;
		}
		MessageIndex++;
	}

	if (EventCnt > 0) // Process Events (if any)
	{
		MD3Time timebase = 0; // Moving time value for events received

		// Process Time/Date Data
		if (MessageIndex < CompleteMD3Message.size())
		{
			timebase = static_cast<uint64_t>(CompleteMD3Message[MessageIndex].GetData()) * 1000; //MD3Time msec since Epoch.
			LOGDEBUG("Fn11 TimeDate Packet Local : {}",to_LOCALtimestringfromMD3time(timebase));
		}
		else
		{
			LOGERROR("Tried to read a block past the end of the message processing time date data : {} Modules : {}",MessageIndex, CompleteMD3Message.size());
			return false;
		}
		MessageIndex++;

		// Now we have to convert the remaining data blocks into an array of words and process them. This is due to the time offset blocks which are only 16 bits long.
		std::vector<uint16_t> ResponseWords;
		while (MessageIndex < CompleteMD3Message.size())
		{
			ResponseWords.push_back(CompleteMD3Message[MessageIndex].GetFirstWord());
			ResponseWords.push_back(CompleteMD3Message[MessageIndex].GetSecondWord());
			MessageIndex++;
		}

		// Now process the response words.
		for (size_t i = 0; i < ResponseWords.size(); i++)
		{
			// If we are processing a data block and the high byte will be non-zero.
			// If it is zero it could be either:
			// A time offset word or
			// If the whole word is zero, then it must be the first word of a Status block.

			if (ResponseWords[i] == 0)
			{
				// We have received a STATUS block, which has a following word.
				i++;
				if (i >= ResponseWords.size())
				{
					// We likely received an all zero padding block at the end of the message. Ignore this as an error
					LOGDEBUG("Fn11 Zero padding end word detected - ignoring");
					return true;
				}
				// This is a status or if module address is 0 a flag block.
				uint8_t ModuleAddress = (ResponseWords[i+1] >> 8) & 0x0FF;
				uint8_t ModuleFailStatus = ResponseWords[i + 1] & 0x0FF;
				LOGDEBUG("Received a Fn 11 Status or Flag Block (addr=0) - We do not process - Module Address : {} Fail Status : {}",ModuleAddress, to_hexstring(ModuleFailStatus));
			}
			else if ((ResponseWords[i] & 0xFF00) == 0)
			{
				// We have received a TIME BLOCK (offset) which is a single word.
				uint16_t msecoffset = (ResponseWords[i] & 0x00ff) * 256;
				timebase += msecoffset;
				LOGDEBUG("Fn11 TimeOffset : {} msec", msecoffset);
			}
			else
			{
				// We have received a DATA BLOCK which has a following word.
				// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
				uint8_t ModuleAddress = (ResponseWords[i] >> 8) & 0x007f;
				uint8_t msecoffset = ResponseWords[i] & 0x00ff;
				timebase += msecoffset; // Update the current tagged time
				i++;

				if (i >= ResponseWords.size())
				{
					// Index error
					LOGERROR("Tried to access past the end of the response words looking for the second part of a data block {}",MD3MessageAsString(CompleteMD3Message));
					return false;
				}
				uint16_t ModuleData = ResponseWords[i];

				LOGDEBUG("Fn11 TimeTagged Block - Module : {} msec offset : {} Data : {}", ModuleAddress, msecoffset, to_hexstring(ModuleData));
				GenerateODCEventsFromMD3ModuleWord(ModuleData, ModuleAddress,timebase);
			}
		}
	}
	return true;
}
void MD3MasterPort::GenerateODCEventsFromMD3ModuleWord(const uint16_t &ModuleData, const uint8_t &ModuleAddress, const MD3Time &eventtime)
{
	LOGDEBUG("Master Generate Events,  Module : {} Data : {}",ModuleAddress, to_hexstring(ModuleData));

	for (uint8_t idx = 0; idx < 16; idx++)
	{
		// When we set the value it tells us if we really did set the value, or it was already at that value.
		// Only fire the ODC event if the value changed.
		bool valuechanged = false;
		uint8_t bitvalue = (ModuleData >> (15 - idx)) & 0x0001;

		bool res = MyPointConf->PointTable.SetBinaryValueUsingMD3Index(ModuleAddress, idx, bitvalue, valuechanged);

		if (res && valuechanged)
		{
			size_t ODCIndex = 0;
			if (MyPointConf->PointTable.GetBinaryODCIndexUsingMD3Index(ModuleAddress, idx, ODCIndex))
			{
				QualityFlags qual = CalculateBinaryQuality(enabled, eventtime);
				LOGDEBUG("Published Event - Binary Index {} Value {}",ODCIndex,bitvalue);
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
bool MD3MasterPort::ProcessDOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an DOM response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(), Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Master Got DOM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessPOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an POM response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(), Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Master Got POM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessAOMReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Master Received an AOM response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(), Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Master Got AOM response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool MD3MasterPort::ProcessSetDateTimeReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SetDateTime response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(),Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Got SetDateTime response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}
bool MD3MasterPort::ProcessSetDateTimeNewReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SetDateTimeNew response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(),Header.GetStationAddress());
		return false;
	}

	LOGDEBUG("Got SetDateTimeNew response ");

	return (Header.GetFunctionCode() == CONTROL_REQUEST_OK);
}

bool MD3MasterPort::ProcessSystemSignOnReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an SystemSignOn response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(),Header.GetStationAddress());
		return false;
	}
	MD3BlockFn40MtoS resp(Header);
	if (!resp.IsValid())
	{
		LOGERROR("Received an SystemSignOn response that was not valid - ignoring - On Station Address - {}", Header.GetStationAddress());
		return false;
	}
	LOGDEBUG("Got SystemSignOn response ");

	return (Header.GetFunctionCode() == SYSTEM_SIGNON_CONTROL);
}
bool MD3MasterPort::ProcessFreezeResetReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	if (CompleteMD3Message.size() != 1)
	{
		LOGERROR("Received an Freeze-Reset response longer than one block - ignoring - {} On Station Address - {}",Header.GetFunctionCode(),Header.GetStationAddress());
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

	// Use the block access method to get at the bits.
	MD3BlockFn52StoM Header52(Header);

	bool SystemPoweredUpFlag = Header52.GetSystemPoweredUpFlag();
	bool SystemTimeIncorrectFlag = Header52.GetSystemTimeIncorrectFlag();

	LOGDEBUG("Got Flag Scan response, {} blocks, System Powered Up Flag : {}, System TimeIncorrect Flag : {}",CompleteMD3Message.size(),SystemPoweredUpFlag,SystemTimeIncorrectFlag);

	if (SystemTimeIncorrectFlag)
	{
		// Find if there is a poll group time set command, if so, do that poll (send the time)
		for (const auto& pg : MyPointConf->PollGroups)
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
		for (const auto& pg : MyPointConf->PollGroups)
		{
			if (pg.second.polltype == BinaryPoints)
			{
				this->DoPoll(pg.second.ID);
			}
		}

		// Launch all analog polls/scans
		for (const auto& pg : MyPointConf->PollGroups)
		{
			if (pg.second.polltype == AnalogPoints)
			{
				this->DoPoll(pg.second.ID);
			}
		}
	}
	return (Header.GetFunctionCode() == SYSTEM_FLAG_SCAN);
}

#ifdef _MSC_VER
#pragma endregion
#endif

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void MD3MasterPort::DoPoll(uint32_t pollgroup)
{
	if (!enabled) return;
	LOGDEBUG("DoPoll : " + std::to_string(pollgroup));

	switch (MyPointConf->PollGroups[pollgroup].polltype)
	{
		case AnalogPoints:
		{
			auto mait = MyPointConf->PollGroups[pollgroup].ModuleAddresses.begin();

			// We will scan a maximum of 1 module, up to 16 channels. It might spill over into the next module if the module is a counter with only 8 channels.
			uint8_t ModuleAddress = mait->first;
			auto Channels = numeric_cast<uint8_t>(mait->second);

			if (MyPointConf->PollGroups[pollgroup].ModuleAddresses.size() > 1)
			{
				LOGERROR("Analog Poll group {} is configured for more than one MD3 address. Please create another poll group.",pollgroup);
			}

			// We need to do an analog unconditional on start up, until all the points have a valid value - even 0x8000 for does not exist.
			// To do this we check the hasbeenset flag, which will be false on start up, and also set to false on comms lost event - kind of like a quality.
			bool UnconditionalCommandRequired = false;
			for (uint8_t idx = 0; idx < Channels; idx++)
			{
				uint16_t wordres;
				bool hasbeenset;
				bool res = MyPointConf->PointTable.GetAnalogValueUsingMD3Index(ModuleAddress, idx, wordres, hasbeenset);
				if (res && !hasbeenset)
					UnconditionalCommandRequired = true;
			}

			if (UnconditionalCommandRequired || MyPointConf->PollGroups[pollgroup].ForceUnconditional)
			{
				// Use Unconditional Request Fn 5
				LOGDEBUG("Poll Issued a Analog Unconditional Command");

				MD3BlockFormatted commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL, ModuleAddress, Channels, true);
				QueueMD3Command(commandblock, nullptr);
			}
			else
			{
				LOGDEBUG("Poll Issued a Analog Delta Command");
				// Use a delta command Fn 6
				MD3BlockFormatted commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_DELTA_SCAN, ModuleAddress, Channels, true);
				QueueMD3Command(commandblock, nullptr);
			}
		}
		break;

		case CounterPoints: //TODO: Analog Poll - Handle Counters
		{
			auto mait = MyPointConf->PollGroups[pollgroup].ModuleAddresses.begin();

			// We will scan a maximum of 1 module, up to 16 channels. It might spill over into the next module if the module is a counter with only 8 channels.
			uint8_t ModuleAddress = mait->first;
			auto Channels = numeric_cast<uint8_t>(mait->second);

			if (MyPointConf->PollGroups[pollgroup].ModuleAddresses.size() > 1)
			{
				LOGERROR("Counter Poll group {} is configured for more than one MD3 address. Please create another poll group.", pollgroup);
			}

			// We need to do an analog unconditional on start up, until all the points have a valid value - even 0x8000 for does not exist.
			// To do this we check the hasbeenset flag, which will be false on start up, and also set to false on comms lost event - kind of like a quality.
			bool UnconditionalCommandRequired = false;
			for (uint8_t idx = 0; idx < Channels; idx++)
			{
				uint16_t wordres;
				bool hasbeenset;
				bool res = MyPointConf->PointTable.GetAnalogValueUsingMD3Index(ModuleAddress, idx, wordres, hasbeenset);
				if (res && !hasbeenset)
					UnconditionalCommandRequired = true;
			}

			if (UnconditionalCommandRequired || MyPointConf->PollGroups[pollgroup].ForceUnconditional)
			{
				// Use Unconditional Request Fn 5
				LOGDEBUG("Poll Issued a Analog Unconditional Command for Counter Values");

				MD3BlockFormatted commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL, ModuleAddress, Channels, true);
				QueueMD3Command(commandblock, nullptr);
			}
			else
			{
				LOGDEBUG("Poll Issued a Analog Delta Command for Counter Values");
				// Use a delta command Fn 6
				MD3BlockFormatted commandblock(MyConf->mAddrConf.OutstationAddr, true, ANALOG_DELTA_SCAN, ModuleAddress, Channels, true);
				QueueMD3Command(commandblock, nullptr);
			}
		}
		break;

		case BinaryPoints:
		{
			if (MyPointConf->NewDigitalCommands) // Old are 7,8,9,10 - New are 11 and 12
			{
				// If sequence number is zero - it means we have just started up, or communications re-established. So we dont have a full copy
				// of the binary data (timetagged or otherwise). The outStation will use the zero sequnce number to send everything to initialise us. We
				// don't have to send an unconditional.

				auto FirstModule = MyPointConf->PollGroups[pollgroup].ModuleAddresses.begin();

				// Request Digital Unconditional
				uint8_t ModuleAddress = FirstModule->first;
				// We expect the digital modules to be consecutive, or if there is a gap this will still work.
				auto Modules = numeric_cast<uint8_t>(MyPointConf->PollGroups[pollgroup].ModuleAddresses.size()); // Most modules we can get in one command - NOT channels 0 to 15!

				bool UnconditionalCommandRequired = false;
				for (uint8_t m = 0; m < Modules; m++)
				{
					for (uint8_t idx = 0; idx < 16; idx++)
					{
						bool hasbeenset;
						bool res = MyPointConf->PointTable.GetBinaryQualityUsingMD3Index(ModuleAddress + m, idx, hasbeenset);
						if (res && !hasbeenset)
							UnconditionalCommandRequired = true;
					}
				}
				MD3BlockFormatted commandblock;

				// Also need to check if we already have a value for every point that this command would ask for..if not send unconditional.
				if (UnconditionalCommandRequired || MyPointConf->PollGroups[pollgroup].ForceUnconditional)
				{
					// Use Unconditional Request Fn 12
					LOGDEBUG("Poll Issued a Digital Unconditional (new) Command");

					commandblock = MD3BlockFn12MtoS(MyConf->mAddrConf.OutstationAddr, ModuleAddress, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
				}
				else
				{
					// Use a delta command Fn 11
					LOGDEBUG("Poll Issued a Digital Delta COS (new) Command");

					uint8_t TaggedEventCount = 0; // Assuming no timetagged points initially.

					// If this poll is marked as timetagged, then we need to ask for them to be returned.
					if (MyPointConf->PollGroups[pollgroup].TimeTaggedDigital == true)
						TaggedEventCount = 15; // The most we can ask for

					commandblock = MD3BlockFn11MtoS(MyConf->mAddrConf.OutstationAddr, TaggedEventCount, GetAndIncrementDigitalCommandSequenceNumber(), Modules);
				}

				QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
			}
			else // Old digital commands
			{
				if (MyPointConf->PollGroups[pollgroup].ForceUnconditional)
				{
					// Use Unconditional Request Fn 7
					LOGDEBUG("Poll Issued a Digital Unconditional (old) Command");
					auto mait = MyPointConf->PollGroups[pollgroup].ModuleAddresses.begin();

					// Request Digital Unconditional
					uint8_t ModuleAddress = mait->first;
					uint8_t channels = 16; // Most we can get in one command
					MD3BlockFormatted commandblock(MyConf->mAddrConf.OutstationAddr, true, DIGITAL_UNCONDITIONAL_OBS, ModuleAddress, channels, true);

					QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
				}
				else
				{
					// Use a delta command Fn 8
					LOGERROR("UNIMPLEMENTED - Poll Issued a Digital Delta (old) Command");
				}
			}
		}
		break;

		case  TimeSetCommand:
		{
			// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
			uint64_t currenttime = MD3NowUTC();

			LOGDEBUG("Poll Issued a TimeDate Command");
			SendTimeDateChangeCommand(currenttime, nullptr);
		}
		break;

		case NewTimeSetCommand:
		{
			// Send a time set command to the OutStation, TimeChange command (Fn 43) UTC Time
			uint64_t currenttime = MD3NowUTC();

			LOGDEBUG("Poll Issued a NewTimeDate Command");
			int utcoffsetminutes = tz_offset();

			SendNewTimeDateChangeCommand(currenttime, utcoffsetminutes, nullptr);
		}
		break;

		case SystemFlagScan:
		{
			// Send a flag scan command to the OutStation, (Fn 52)
			LOGDEBUG("Poll Issued a System Flag Scan Command");
			SendSystemFlagScanCommand(nullptr);
		}
		break;

		default:
			LOGDEBUG("Poll with an unknown polltype : {}",MyPointConf->PollGroups[pollgroup].polltype);
	}
}

void MD3MasterPort::ResetDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);
	DigitalCommandSequenceNumber = 0;
}
// Manage the access to the command sequence number. Very low possibility of contention, so use standard lock.
uint8_t MD3MasterPort::GetAndIncrementDigitalCommandSequenceNumber()
{
	std::unique_lock<std::mutex> lck(DigitalCommandSequenceNumberMutex);

	uint8_t retval = DigitalCommandSequenceNumber;

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
void MD3MasterPort::SendTimeDateChangeCommand(const uint64_t &currenttimeinmsec, const SharedStatusCallback_t& pStatusCallback)
{
	MD3BlockFn43MtoS commandblock(MyConf->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
	MD3BlockData datablock(static_cast<uint32_t>(currenttimeinmsec / 1000), true);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendNewTimeDateChangeCommand(const uint64_t &currenttimeinmsec, int utcoffsetminutes, const SharedStatusCallback_t& pStatusCallback)
{
	MD3BlockFn44MtoS commandblock(MyConf->mAddrConf.OutstationAddr, currenttimeinmsec % 1000);
	MD3BlockData datablock(static_cast<uint32_t>(currenttimeinmsec / 1000));
	MD3BlockData datablock2(static_cast<uint32_t>(utcoffsetminutes) << 16, true);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	Cmd.push_back(datablock2);
	QueueMD3Command(Cmd, pStatusCallback);
}
void MD3MasterPort::SendSystemFlagScanCommand(SharedStatusCallback_t pStatusCallback)
{
	MD3BlockFn52MtoS commandblock(MyConf->mAddrConf.OutstationAddr);
	QueueMD3Command(commandblock, std::move(pStatusCallback));
}

void MD3MasterPort::SetAllPointsQualityToCommsLost()
{
	LOGDEBUG("MD3 Master setting quality to comms lost");

	auto eventbinary = std::make_shared<EventInfo>(EventType::BinaryQuality, 0, Name, QualityFlags::COMM_LOST);
	eventbinary->SetPayload<EventType::BinaryQuality>(QualityFlags::COMM_LOST);

	// Loop through all Binary points.
	MyPointConf->PointTable.ForEachBinaryPoint([&](MD3BinaryPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			eventbinary->SetIndex(index);
			PublishEvent(eventbinary);
			Point.SetChangedFlag();
		});

	// Analogs

	auto eventanalog = std::make_shared<EventInfo>(EventType::AnalogQuality, 0, Name, QualityFlags::COMM_LOST);
	eventanalog->SetPayload<EventType::AnalogQuality>(QualityFlags::COMM_LOST);
	MyPointConf->PointTable.ForEachAnalogPoint([&](MD3AnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			if (!MyPointConf->PointTable.ResetAnalogValueUsingODCIndex(index)) // Sets to 0x8000, time = 0, HasBeenSet to false
				LOGERROR("Tried to set the value for an invalid analog point index {}",index);

			eventanalog->SetIndex(index);
			PublishEvent(eventanalog);
		});
	// Counters
	auto eventcounter = std::make_shared<EventInfo>(EventType::CounterQuality, 0, Name, QualityFlags::COMM_LOST);
	eventcounter->SetPayload<EventType::CounterQuality>(QualityFlags::COMM_LOST);

	MyPointConf->PointTable.ForEachCounterPoint([&](MD3AnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			if (!MyPointConf->PointTable.ResetCounterValueUsingODCIndex(index)) // Sets to 0x8000, time = 0, HasBeenSet to false
				LOGERROR("Tried to set the value for an invalid analog point index {}",index);

			eventcounter->SetIndex(index);
			PublishEvent(eventcounter);
		});
}

// When a new device connects to us through ODC (or an existing one reconnects), send them everything we currently have.
void MD3MasterPort::SendAllPointEvents()
{
	// Quality of ONLINE means the data is GOOD.
	MyPointConf->PointTable.ForEachBinaryPoint([&](MD3BinaryPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint8_t meas = Point.GetBinary();
			QualityFlags qual = CalculateBinaryQuality(enabled, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Binary, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Binary>(meas == 1);
			PublishEvent(event);
		});

	// Analogs
	MyPointConf->PointTable.ForEachAnalogPoint([&](MD3AnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint16_t meas = Point.GetAnalog();

			// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
			QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Analog, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Analog>(std::move(meas));
			PublishEvent(event);
		});

	// Counters
	MyPointConf->PointTable.ForEachCounterPoint([&](MD3AnalogCounterPoint &Point)
		{
			uint32_t index = Point.GetIndex();
			uint16_t meas = Point.GetAnalog();
			// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
			QualityFlags qual = CalculateAnalogQuality(enabled, meas, Point.GetChangedTime());

			auto event = std::make_shared<EventInfo>(EventType::Counter, index, Name, qual, static_cast<msSinceEpoch_t>(Point.GetChangedTime()));
			event->SetPayload<EventType::Counter>(std::move(meas));
			PublishEvent(event);
		});
}

// Binary quality only depends on our link status and if we have received data
QualityFlags MD3MasterPort::CalculateBinaryQuality(bool enabled, MD3Time time)
{
	//TODO: Change this to Quality being part of the point object
	return (enabled ? ((time == 0) ? QualityFlags::RESTART : QualityFlags::ONLINE) : QualityFlags::COMM_LOST);
}
// Use the measurement value and if we are enabled to determine what the quality value should be.
QualityFlags MD3MasterPort::CalculateAnalogQuality(bool enabled, uint16_t meas, MD3Time time)
{
	//TODO: Change this to Quality being part of the point object
	return (enabled ? (time == 0 ? QualityFlags::RESTART : ((meas == 0x8000) ? QualityFlags::LOCAL_FORCED : QualityFlags::ONLINE)) : QualityFlags::COMM_LOST);
}


// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
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
			return WriteObject(event->GetPayload<EventType::ControlRelayOutputBlock>(), numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::AnalogOutputInt16:
			return WriteObject(event->GetPayload<EventType::AnalogOutputInt16>().first, numeric_cast<uint32_t>(event->GetIndex()), pStatusCallback);
		case EventType::ConnectState:
		{
			auto state = event->GetPayload<EventType::ConnectState>();
			// This will be fired by (typically) an MD3OutStation port on the "other" side of the ODC Event bus. i.e. something upstream has connected
			// We should probably send all the points to the Outstation as we don't know what state the OutStation point table will be in.

			if (state == ConnectState::CONNECTED)
			{
				LOGDEBUG("Upstream (other side of ODC) port enabled - Triggering sending of current data ");
				// We don\92t know the state of the upstream data, so send event information for all points.
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


void MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	uint8_t ModuleAddress = 0;
	uint8_t Channel = 0;
	BinaryPointType PointType;
	bool exists = MyPointConf->PointTable.GetBinaryControlMD3IndexUsingODCIndex(index, ModuleAddress, Channel, PointType);

	if (!exists)
	{
		LOGDEBUG("Master received a DOM/POM ODC Change Command on a point that is not defined {}",index);
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	std::string OnOffString = "ON";

	if (PointType == DOMOUTPUT)
	{
		// The main issue is that a DOM command writes 16 bits in one go, so we have to remember the output state of the other bits,
		// so that we only write the change that has been triggered.
		// We have to gather up the current state of those bits.

		bool ModuleFailed = false;                                                                            // Not used - yet
		uint16_t outputbits = MyPointConf->PointTable.CollectModuleBitsIntoWord(ModuleAddress, ModuleFailed); // Reads from the digital input point list...

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

		LOGDEBUG("Master received a DOM ODC Change Command - Index: {} - {}  Module/Channel {}/{}",index,OnOffString,ModuleAddress,Channel);
		SendDOMOutputCommand(MyConf->mAddrConf.OutstationAddr, ModuleAddress, outputbits, pStatusCallback);
	}
	else if(PointType == POMOUTPUT)
	{
		// POM Point
		// The POM output is a single output selection, value 0 to 15
		// We don't have to remember state here as we are sending only one bit.
		uint8_t outputselection = Channel;

		if ((command.functionCode == ControlCode::PULSE_OFF) || (command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
		{
			// OFF Command
			OnOffString = "OFF";
		}
		else
		{
			// ON Command  --> ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
			OnOffString = "ON";
		}

		LOGDEBUG("Master received a POM ODC Change Command - Index: {} - {}  Module/Channel{}/{}",index,OnOffString,ModuleAddress,Channel);
		SendPOMOutputCommand(MyConf->mAddrConf.OutstationAddr, ModuleAddress, outputselection, pStatusCallback);
	}
	else if (PointType == DIMOUTPUT)
	{
		// POM Point
		// The POM output is a single output selection, value 0 to 15 which represents a TRIP 0-7 and CLOSE 8-15 for the up to 8 points in a POM module.
		// We don't have to remember state here as we are sending only one bit.
		uint8_t outputselection = Channel;

		if ((command.functionCode == ControlCode::PULSE_OFF) || (command.functionCode == ControlCode::LATCH_OFF) || (command.functionCode == ControlCode::TRIP_PULSE_ON))
		{
			// OFF Command
			OnOffString = "OFF";
		}
		else
		{
			// ON Command  --> ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
			OnOffString = "ON";
		}
		DIMControlSelectionType action = DIMControlSelectionType::ON;

		switch (command.functionCode)
		{
			case ControlCode::PULSE_OFF:
				action = DIMControlSelectionType::TRIP;
				break;
			case ControlCode::TRIP_PULSE_ON:
				action = DIMControlSelectionType::TRIP;
				break;
			case ControlCode::LATCH_OFF:
				action = DIMControlSelectionType::OFF;
				break;

			case ControlCode::PULSE_ON:
				action = DIMControlSelectionType::CLOSE;
				break;
			case ControlCode::CLOSE_PULSE_ON:
				action = DIMControlSelectionType::CLOSE;
				break;
			case ControlCode::LATCH_ON:
				action = DIMControlSelectionType::ON;
				break;

			default: LOGERROR("{} - DIM Input Point Control does not support this ODC Control Value ",Name,ToString(command.functionCode));
				return;
		}

		LOGDEBUG("Master received a DIM ODC Change Command - Index: {} - {} - {} Module/Channel{}/{}", index, OnOffString, ToString(action), ModuleAddress, Channel);
		SendDIMOutputCommand(MyConf->mAddrConf.OutstationAddr, ModuleAddress, outputselection, action, 0, pStatusCallback);
	}
	else
	{
		LOGDEBUG("Master received Binary Output ODC Event on a point not defined as DOM, DIM or POM {}",index);
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
}

void MD3MasterPort::WriteObject(const int16_t & command, const uint32_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// AOM Command
	uint8_t ModuleAddress = 0;
	uint8_t Channel = 0;
	bool exists = MyPointConf->PointTable.GetAnalogControlMD3IndexUsingODCIndex(index, ModuleAddress, Channel);

	if (!exists)
	{
		LOGDEBUG("Master received an AOM or DIM Analog ODC Change Command on a point that is not defined {}", index);
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}
	DIMControlSelectionType action = DIMControlSelectionType::ON;

	LOGDEBUG("Master received a AOM ODC Change Command - Index: {} - {} Module/Channel{}/{}", index, ToString(action), ModuleAddress, Channel);

	SendDIMOutputCommand(MyConf->mAddrConf.OutstationAddr, ModuleAddress, Channel, action, 0, pStatusCallback);

	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
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
void MD3MasterPort::SendDIMOutputCommand(const uint8_t& StationAddress, const uint8_t& ModuleAddress, const uint8_t& outputselection, const DIMControlSelectionType controlselect, const uint16_t outputdata, const SharedStatusCallback_t& pStatusCallback)
{
	MD3BlockFn20MtoS commandblock(StationAddress, ModuleAddress, outputselection, controlselect);

	MD3Message_t Cmd;
	Cmd.push_back(commandblock);

	if (commandblock.GetControlSelection() == SETPOINT) // Analog Setpoint command
	{
		MD3BlockData datablock = commandblock.GenerateSecondBlock(false);
		Cmd.push_back(datablock);
		// Three block message
		MD3BlockData datablock2 = commandblock.GenerateThirdBlock(outputdata);
		Cmd.push_back(datablock2);
	}
	else
	{
		MD3BlockData datablock = commandblock.GenerateSecondBlock(true);
		Cmd.push_back(datablock);
	}

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
