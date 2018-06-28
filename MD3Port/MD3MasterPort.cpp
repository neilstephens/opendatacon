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
{}

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
		PublishEvent(ConnectState::CONNECTED, 0);
		msg = Name + ": Connection established.";
		//TODO: Need to trigger a full scan to get all values....or does out poll function work it out for us...
	}
	else
	{
		PollScheduler->Stop();

		SetAllPointsQualityToCommsLost(); // All the connected points need their quality set to comms lost.

		ClearMD3CommandQueue(); // Remove all waiting commands and callbacks

		PublishEvent(ConnectState::DISCONNECTED, 0);
		msg = Name + ": Connection closed.";
	}
	LOGINFO(msg);
}


void MD3MasterPort::BuildOrRebuild()
{
	//TODO: Do we re-read the conf file - so we can do a live reload? - How do we kill all the sockets and connections properly?
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
	//	PollScheduler->Start(); Is started and stopped in the socket state handler
}


#pragma region MasterCommandQueue

// We can only send one command at a time (until we have a timeout or success), so queue them up so we process them in order.
// There is a fixed timeout function (below) which will queue the next command if we timeout.
// If the ProcessMD3Message callback gets the command it expects, it will send the next command in the queue.
// If the callback gets an error it will be ignored which will result in a timeout and the next command being sent.
// This is necessary if somehow we get an old command sent to us, or a left over broadcast message.
// Only issue is if we do a broadcast message and can get information back from multiple sources... These commands are probably not used, and we will ignore them anyway.
void MD3MasterPort::QueueMD3Command(const MasterCommandQueueItem &CompleteMD3Message)
{
	MasterCommandStrand->dispatch([=]() // Tries to execute, if not able to will post. Note the calling thread must be one of the io_service threads.... this changes our tests!
		{
			if (MasterCommandProtectedData.MasterCommandQueue.size() < MasterCommandProtectedData.MaxCommandQueueSize)
			{
			      MasterCommandProtectedData.MasterCommandQueue.push(CompleteMD3Message); // async
			}
			else
				LOGDEBUG("Tried to queue another MD3 Master Command when the command queue is full");

			// Will only send if we can - i.e. not currently processing a command
			UnprotectedSendNextMasterCommand(false);
		});
}

// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockFormatted &SingleBlockMD3Message)
{
	std::vector<MD3BlockData> CommandMD3Message;
	CommandMD3Message.push_back(SingleBlockMD3Message);
	QueueMD3Command(CommandMD3Message);
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
				// Have had multiple retries fail, so mark everything as if we have lost comms!
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

			MasterCommandProtectedData.CurrentFunctionCode = ((MD3BlockFormatted)MasterCommandProtectedData.CurrentCommand[0]).GetFunctionCode();
			LOGDEBUG("Sending next command :" + std::to_string(MasterCommandProtectedData.CurrentFunctionCode))
		}

		// If either of the above situations need us to send a command, do so.
		if (MasterCommandProtectedData.ProcessingMD3Command == true)
		{
			SendMD3Message(MasterCommandProtectedData.CurrentCommand); // This should be the only place this is called for the MD3Master...

			// Start an async timed callback for a timeout - cancelled if we receive a good response.
			MasterCommandProtectedData.TimerExpireTime = std::chrono::milliseconds(MyPointConf()->MD3CommandTimeoutmsec);
			MasterCommandProtectedData.CurrentCommandTimeoutTimer->expires_from_now(MasterCommandProtectedData.TimerExpireTime);

			std::chrono::milliseconds endtime = MasterCommandProtectedData.TimerExpireTime;

			MasterCommandProtectedData.CurrentCommandTimeoutTimer->async_wait([&,endtime](asio::error_code err_code)
				{
					LOGDEBUG("MD3 Master Timeout callback called");

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
			std::vector<MD3BlockData> NextCommand;
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
void MD3MasterPort::ProcessMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
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

			MD3BlockFormatted Header = CompleteMD3Message[0];

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

			// For a given command we can have multiple return codes. So check what we got...
			if (!AllowableResponseToFunctionCode(MasterCommandProtectedData.CurrentFunctionCode, Header.GetFunctionCode()))
			{
			      LOGERROR("Received a Function Code we were not expecting - ignoring - " + std::to_string(Header.GetFunctionCode()) +
					" Expecting " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
			      return;
			}

			bool success = false;

			// Now based on the Command Function, take action. Not all codes are expected or result in action
			switch (Header.GetFunctionCode())
			{
				case ANALOG_UNCONDITIONAL: // Command and reply
					success = ProcessAnalogUnconditionalReturn(Header, CompleteMD3Message);
					break;
				case ANALOG_DELTA_SCAN: // Command and reply
					success = ProcessAnalogDeltaScanReturn(Header, CompleteMD3Message);
					break;
				case DIGITAL_UNCONDITIONAL_OBS:
					//	DoDigitalUnconditionalObs(Header);
					break;
				case DIGITAL_DELTA_SCAN:
					//	DoDigitalChangeOnly(Header);
					break;
				case HRER_LIST_SCAN:
					//		DoDigitalHRER(static_cast<MD3BlockFn9&>(Header), CompleteMD3Message);
					break;
				case DIGITAL_CHANGE_OF_STATE:
					//	DoDigitalCOSScan(static_cast<MD3BlockFn10&>(Header));
					break;
				case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
					//	DoDigitalScan(static_cast<MD3BlockFn11MtoS&>(Header));
					break;
				case DIGITAL_UNCONDITIONAL:
					//	DoDigitalUnconditional(static_cast<MD3BlockFn12MtoS&>(Header));
					break;
				case ANALOG_NO_CHANGE_REPLY:
					success = ProcessAnalogNoChangeReturn(Header, CompleteMD3Message);
					break;
				case DIGITAL_NO_CHANGE_REPLY:
					// Master Only
					break;
				case CONTROL_REQUEST_OK:
					// Master Only
					break;
				case FREEZE_AND_RESET:
					//	DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
					break;
				case POM_TYPE_CONTROL:
					//	DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
					break;
				case DOM_TYPE_CONTROL:
					//	DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
					break;
				case INPUT_POINT_CONTROL:
					break;
				case RAISE_LOWER_TYPE_CONTROL:
					break;
				case AOM_TYPE_CONTROL:
					//	DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
					break;
				case CONTROL_OR_SCAN_REQUEST_REJECTED:
					// Master Only
					break;
				case COUNTER_SCAN:
					//	DoCounterScan(Header);
					break;
				case SYSTEM_SIGNON_CONTROL:
					//	DoSystemSignOnControl(static_cast<MD3BlockFn40&>(Header));
					break;
				case SYSTEM_SIGNOFF_CONTROL:
					break;
				case SYSTEM_RESTART_CONTROL:
					break;
				case SYSTEM_SET_DATETIME_CONTROL:
					//	DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
					break;
				case FILE_DOWNLOAD:
					break;
				case FILE_UPLOAD:
					break;
				case SYSTEM_FLAG_SCAN:
					//	DoSystemFlagScan(Header, CompleteMD3Message);
					break;
				case LOW_RES_EVENTS_LIST_SCAN:
					break;
				default:
					LOGERROR("Unknown Message Function - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
					break;
			}

			if (success) // Move to the next command. Only other place we do this is in the timeout.
			{
			      //TODO: I have a concern about cancelling a timer based async_wait and then starting it again almost straight away, when will the timer function be called, and will our flags be still in the cancel state???
			      MasterCommandProtectedData.CurrentCommandTimeoutTimer->cancel(); // Have to be careful the handler still might do something?
			      MasterCommandProtectedData.ProcessingMD3Command = false;         // Only gets reset on success or timeout.
			      UnprotectedSendNextMasterCommand(false);                         // We already have the strand, so don't need the wrapper here. Pass in that this is not a retry.
			}
		});
}

// We have received data from an Analog command - could be  the result of Fn 5 or 6
// Store the decoded data into the point lists. Counter scan comes back in an identical format
// Return success or failure
bool MD3MasterPort::ProcessAnalogUnconditionalReturn(MD3BlockFormatted & Header, const std::vector<MD3BlockData>& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	int NumberOfDataBlocks = Channels / 2 + Channels % 2; // 2 --> 1, 3 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) + " On Station Address - " + std::to_string(Header.GetStationAddress()));
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

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres);
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
			int intres;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				PublishEvent(Analog(AnalogValues[i], qual, (opendnp3::DNPTime)now), intres); // We don’t get counter time information through MD3, so add it as soon as possible
			}
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			int intres;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				PublishEvent(Counter(AnalogValues[i], qual, (opendnp3::DNPTime)now), intres); // We don’t get analog time information through MD3, so add it as soon as possible
			}
		}
		else
		{
			LOGERROR("Fn5 Failed to set an Analog or Counter Value - " + std::to_string(Header.GetFunctionCode())
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
bool MD3MasterPort::ProcessAnalogDeltaScanReturn(MD3BlockFormatted & Header, const std::vector<MD3BlockData>& CompleteMD3Message)
{
	uint8_t ModuleAddress = Header.GetModuleAddress();
	uint8_t Channels = Header.GetChannels();

	int NumberOfDataBlocks = Channels / 4 + Channels % 4; // 2 --> 1, 5 -->2

	if (NumberOfDataBlocks != CompleteMD3Message.size() - 1)
	{
		LOGERROR("Received a message with the wrong number of blocks - ignoring - " + std::to_string(Header.GetFunctionCode()) +
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

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres);
	MD3Time now = MD3Now();

	for (int i = 0; i < Channels; i++)
	{
		// Code to adjust the ModuleAddress and index if the first module is a counter module (8 channels)
		// 16 channels will cover two counters or one counter and 1/2 an analog, or one analog (16 channels).
		// We assume that Analog and Counter modules cannot have the same module address - which we think is a safe assumption.
		int idx = FirstModuleIsCounterModule ? i % 8 : i;
		int maddress = (FirstModuleIsCounterModule && i > 8) ? ModuleAddress + 1 : ModuleAddress;

		if (GetAnalogValueUsingMD3Index(maddress, idx, wordres))
		{
			wordres += AnalogDeltaValues[i];                     // Add the signed delta.
			SetAnalogValueUsingMD3Index(maddress, idx, wordres); //TODO Do all SetMethods need to have a time field as well? With a magic number (Say 10 which is in the past) as default which means no change?

			int intres;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, wordres, now);
				PublishEvent(Analog(wordres, qual, (opendnp3::DNPTime)now), intres); // We don't get counter time information through MD3, so add it as soon as possible
			}
		}
		else if (GetCounterValueUsingMD3Index(maddress, idx,wordres))
		{
			wordres += AnalogDeltaValues[i]; // Add the signed delta.
			SetCounterValueUsingMD3Index(maddress, idx, wordres);

			int intres;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled,wordres, now);
				PublishEvent(Counter(wordres, qual, (opendnp3::DNPTime)now), intres); // We don't get analog time information through MD3, so add it as soon as possible
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
bool MD3MasterPort::ProcessAnalogNoChangeReturn(MD3BlockFormatted & Header, const std::vector<MD3BlockData>& CompleteMD3Message)
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

	//TODO: ANALOG_NO_CHANGE_REPLY do we update the times on the points that we asked to be updated?

	// Search to see if the first value is a counter or analog
	bool FirstModuleIsCounterModule = GetCounterValueUsingMD3Index(ModuleAddress, 0, wordres);
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
		            uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i], now);
		            PublishEvent(Analog(AnalogValues[i], qual, (opendnp3::DNPTime)now), intres); // We don’t get counter time information through MD3, so add it as soon as possible
		      }
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
		      // We have succeeded in setting the value
		      int intres;
		      if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
		      {
		            uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i], now);
		            PublishEvent(Counter(AnalogValues[i], qual, (opendnp3::DNPTime)now), intres); // We don’t get analog time information through MD3, so add it as soon as possible
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

// Check that what we got is one that is expected for the current Function we are processing.
bool MD3MasterPort::AllowableResponseToFunctionCode(uint8_t CurrentFunctionCode, uint8_t FunctionCode)
{
	// Only some of these we should receive at the Master!
	bool result = false;

	switch (CurrentFunctionCode)
	{
		case ANALOG_UNCONDITIONAL: // Command and reply
			result = (FunctionCode == ANALOG_UNCONDITIONAL);
			break;
		case ANALOG_DELTA_SCAN: // Command and reply
			result = (FunctionCode == ANALOG_DELTA_SCAN) || (FunctionCode == ANALOG_UNCONDITIONAL) || (FunctionCode == ANALOG_NO_CHANGE_REPLY);
			break;
		case DIGITAL_UNCONDITIONAL_OBS:
			//	DoDigitalUnconditionalObs(Header);
			break;
		case DIGITAL_DELTA_SCAN:
			//	DoDigitalChangeOnly(Header);
			break;
		case HRER_LIST_SCAN:
			//		DoDigitalHRER(static_cast<MD3BlockFn9&>(Header), CompleteMD3Message);
			break;
		case DIGITAL_CHANGE_OF_STATE:
			//	DoDigitalCOSScan(static_cast<MD3BlockFn10&>(Header));
			break;
		case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
			//	DoDigitalScan(static_cast<MD3BlockFn11MtoS&>(Header));
			break;
		case DIGITAL_UNCONDITIONAL:
			//	DoDigitalUnconditional(static_cast<MD3BlockFn12MtoS&>(Header));
			break;
		case ANALOG_NO_CHANGE_REPLY:
			//TODO: ANALOG_NO_CHANGE_REPLY do we update the times on the points that we asked to be updated?
			// Master Only to receive.
			break;
		case DIGITAL_NO_CHANGE_REPLY:
			// Master Only
			break;
		case CONTROL_REQUEST_OK:
			// Master Only
			break;
		case FREEZE_AND_RESET:
			//	DoFreezeResetCounters(static_cast<MD3BlockFn16MtoS&>(Header));
			break;
		case POM_TYPE_CONTROL:
			//	DoPOMControl(static_cast<MD3BlockFn17MtoS&>(Header), CompleteMD3Message);
			break;
		case DOM_TYPE_CONTROL:
			//	DoDOMControl(static_cast<MD3BlockFn19MtoS&>(Header), CompleteMD3Message);
			break;
		case INPUT_POINT_CONTROL:
			break;
		case RAISE_LOWER_TYPE_CONTROL:
			break;
		case AOM_TYPE_CONTROL:
			//	DoAOMControl(static_cast<MD3BlockFn23MtoS&>(Header), CompleteMD3Message);
			break;
		case CONTROL_OR_SCAN_REQUEST_REJECTED:
			// Master Only
			break;
		case COUNTER_SCAN:
			//	DoCounterScan(Header);
			break;
		case SYSTEM_SIGNON_CONTROL:
			//	DoSystemSignOnControl(static_cast<MD3BlockFn40&>(Header));
			break;
		case SYSTEM_SIGNOFF_CONTROL:
			break;
		case SYSTEM_RESTART_CONTROL:
			break;
		case SYSTEM_SET_DATETIME_CONTROL:
			//	DoSetDateTime(static_cast<MD3BlockFn43MtoS&>(Header), CompleteMD3Message);
			break;
		case FILE_DOWNLOAD:
			break;
		case FILE_UPLOAD:
			break;
		case SYSTEM_FLAG_SCAN:
			//	DoSystemFlagScan(Header, CompleteMD3Message);
			break;
		case LOW_RES_EVENTS_LIST_SCAN:
			break;
		default:
			LOGERROR("Illegal Function Code Received - " + std::to_string(FunctionCode));
			break;
	}

	return result;
}

#pragma endregion

// We will be called at the appropriate time to trigger an Unconditional or Delta scan
// For digital scans there are two formats we might use. Set in the conf file.
void MD3MasterPort::DoPoll(uint32_t pollgroup)
{
	if(!enabled) return;

	if (MyPointConf()->PollGroups[pollgroup].polltype == AnalogPoints)
	{
		if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
		{
			// Use Unconditional Request Fn 5
			// So now need to work out the start Module address and the number of channels to scan (can be Analog or Counter)
			// For an analog, only one command to get the maximum of 16 channels. For counters it might be two modules that we can get with one command.

			ModuleMapType::iterator mait = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

			// Request Analog Unconditional, Station 0x7C, Module 0x20, 16 Channels
			int ModuleAddress = mait->first;
			int channels = 16; // Most we can get in one command
			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL,ModuleAddress, channels, true);

			QueueMD3Command(commandblock);
		}
		else
		{
			// Use a delta command Fn 6
		}
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == BinaryPoints)
	{
		if (MyPointConf()->NewDigitalCommands) // Old are 7,8,9,10 - New are 11 and 12
		{
			if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
			{
				// Use Unconditional Request Fn 12
			}
			else
			{
				// Use a delta command Fn 11
			}
		}
		else
		{
			if (MyPointConf()->PollGroups[pollgroup].UnconditionalRequired)
			{
				// Use Unconditional Request Fn 7
			}
			else
			{
				// Use a delta command Fn 8
			}
		}
	}
}

void MD3MasterPort::SetAllPointsQualityToCommsLost()
{
	LOGDEBUG("MD3 Master setting quality to comms lost");
	// Loop through all Binary points.
	for (auto const &Point : MyPointConf()->BinaryODCPointMap)
	{
		int index = Point.first;
		PublishEvent(BinaryQuality::COMM_LOST, index);
	}
	// Analogs
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		if (!SetAnalogValueUsingODCIndex(index, (uint16_t)0x8000))
			LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));

		PublishEvent(AnalogQuality::COMM_LOST, index);
	}
	// Counters
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		if (!SetCounterValueUsingODCIndex(index, (uint16_t)0x8000))
			LOGERROR("Tried to set the value for an invalid analog point index " + std::to_string(index));
		PublishEvent(CounterQuality::COMM_LOST, index);
	}
	// Binary Control/Output
	for (auto const &Point : MyPointConf()->BinaryControlODCPointMap)
	{
		int index = Point.first;
		PublishEvent(BinaryOutputStatusQuality::COMM_LOST, index);
	}
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
		uint8_t qual = CalculateBinaryQuality(enabled, Point.second->ChangedTime);
		PublishEvent(Binary( meas == 1, qual, (opendnp3::DNPTime)Point.second->ChangedTime),index);
	}

	// Binary Control/Output - the status of which we show as a binary - on our other end we look for the index in both binary lists
	//TODO: SJE Check that we need to report BinaryOutput status, or if it is just assumed?
	for (auto const &Point : MyPointConf()->BinaryControlODCPointMap)
	{
		int index = Point.first;
		uint8_t meas = Point.second->Binary;
		uint8_t qual = CalculateBinaryQuality(enabled, Point.second->ChangedTime);
		PublishEvent(Binary(meas == 1, qual, (opendnp3::DNPTime)Point.second->ChangedTime), index);
	}
	// Analogs
	for (auto const &Point : MyPointConf()->AnalogODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		uint8_t qual = CalculateAnalogQuality(enabled, meas, Point.second->ChangedTime);
		PublishEvent(Analog(meas, qual, (opendnp3::DNPTime)Point.second->ChangedTime), index);
	}
	// Counters
	for (auto const &Point : MyPointConf()->CounterODCPointMap)
	{
		int index = Point.first;
		uint16_t meas = Point.second->Analog;
		// If the measurement is 0x8000 - there is a problem in the MD3 OutStation for that point.
		uint8_t qual = CalculateAnalogQuality(enabled, meas, Point.second->ChangedTime);
		PublishEvent(Counter(meas, qual, (opendnp3::DNPTime)Point.second->ChangedTime), index);
	}
}

// Binary quality only depends on our link status and if we have received data
uint8_t MD3MasterPort::CalculateBinaryQuality(bool enabled, MD3Time time)
{
	return (uint8_t)(enabled ? ((time == 0) ? BinaryQuality::RESTART : BinaryQuality::ONLINE) : BinaryQuality::COMM_LOST);
}
// Use the measurement value and if we are enabled to determine what the quality value should be.
uint8_t MD3MasterPort::CalculateAnalogQuality(bool enabled, uint16_t meas, MD3Time time)
{
	return (uint8_t)(enabled ? (time == 0 ? AnalogQuality::RESTART : ((meas == 0x8000) ? AnalogQuality::LOCAL_FORCED : AnalogQuality::ONLINE)) : AnalogQuality::COMM_LOST);
}

// This will be fired by (typically) an MD3OutStation port on the "other" side of the ODC Event bus.
// We should probably send all the points to the Outstation as we don't know what state the OutStation point table will be in.
void MD3MasterPort::ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	//something upstream has connected
	if(state == ConnectState::CONNECTED)
	{
		LOGDEBUG("Upstream (other side of ODC) port enabled - Triggering sending of current data ");
		// We don’t know the state of the upstream data, so send event information for all points.
		SendAllPointEvents();
	}
	else // ConnectState::DISCONNECTED
	{
		// If we were an on demand connection, we would take down the connection . For MD3 we are using persistent connections only.
		// We have lost an ODC connection, so events we send don't go anywhere.

	}

	(*pStatusCallback)(CommandStatus::SUCCESS);
}

//Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
void MD3MasterPort::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }


// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
// see all 5 possible definitions above.
// We will have to translate from the float values to the uint16_t that MD3 actually handles, and then it is only a 12 bit number.
template<typename T>
inline void MD3MasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	// We now have to (most likely) send a command out to an outstation and wait for a response
	// We can launch the command here, create a lamda to be called in the event of timeout or response.
	// At that time we will call the callback function.
	// For now we just return so whatever sent the ODC Event can get on with things. It will have set up the callback we will call when the time is right.
	//	cmd_promise->set_value(WriteObject(arCommand, index));
	/*
	auto lambda = capture( std::move(cmd_promise),
	[=]( std::unique_ptr<std::promise<CommandStatus>> & cmd_promise ) {
	cmd_promise->set_value(WriteObject(arCommand, index));
	} );
	pIOS->post([&](){ lambda(); });
	*/
	// For now, do the callback anyway!
	(*pStatusCallback)(CommandStatus::UNDEFINED);

	return;
}

/*
template<>
CommandStatus MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, uint16_t index)
{
      if (
            (command.functionCode == ControlCode::NUL) ||
            (command.functionCode == ControlCode::UNDEFINED)
            )
      {
            return CommandStatus::FORMAT_ERROR;
      }

      // MD3 function code 0x01 (read coil status)
      MD3ReadGroup<Binary>* TargetRange = GetRange(index);
      if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

      int rc;
      if (
            (command.functionCode == ControlCode::LATCH_OFF) ||
            (command.functionCode == ControlCode::TRIP_PULSE_ON)
            )
      {
//		rc = MD3_write_bit(mb, index, false);
      }
      else
      {
            //ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
//		rc = MD3_write_bit(mb, index, true);
      }

      // If the index is part of a non-zero pollgroup, queue a poll task for the group
      if (TargetRange->pollgroup > 0)
            pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write bit");
      return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt16& command, uint16_t index)
{
      MD3ReadGroup<Binary>* TargetRange = GetRange(index);
      if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

      // If the index is part of a non-zero pollgroup, queue a poll task for the group
      if (TargetRange->pollgroup > 0)
            pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
      return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputInt32& command, uint16_t index)
{
      MD3ReadGroup<Binary>* TargetRange = GetRange(index);
      if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

      // If the index is part of a non-zero pollgroup, queue a poll task for the group
      if (TargetRange->pollgroup > 0)
            pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
      return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputFloat32& command, uint16_t index)
{
      MD3ReadGroup<Binary>* TargetRange = GetRange(index);
      if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

      // If the index is part of a non-zero pollgroup, queue a poll task for the group
      if (TargetRange->pollgroup > 0)
            pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
      return CommandStatus::SUCCESS;
}

template<>
CommandStatus MD3MasterPort::WriteObject(const AnalogOutputDouble64& command, uint16_t index)
{
      MD3ReadGroup<Binary>* TargetRange = GetRange(index);
      if (TargetRange == nullptr) return CommandStatus::UNDEFINED;

//	int rc = MD3_write_register(mb, index, command.value);

      // If the index is part of a non-zero pollgroup, queue a poll task for the group
      if (TargetRange->pollgroup > 0)
            pIOS->post([=](){ DoPoll(TargetRange->pollgroup); });

//	if (rc == -1) return HandleWriteError(errno, "write register");
      return CommandStatus::SUCCESS;
}
*/





