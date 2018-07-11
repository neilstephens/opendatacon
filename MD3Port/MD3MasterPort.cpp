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

// Handle the many single block command messages better
void MD3MasterPort::QueueMD3Command(const MD3BlockFormatted &SingleBlockMD3Message, SharedStatusCallback_t pStatusCallback)
{
	MD3Message_t CommandMD3Message;
	CommandMD3Message.push_back(SingleBlockMD3Message);
	QueueMD3Command(CommandMD3Message, pStatusCallback);
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

			LOGDEBUG("MD3 Master received a response to sending cmd " + std::to_string(MasterCommandProtectedData.CurrentFunctionCode) + " of " + std::to_string(Header.GetFunctionCode()));

			bool success = false;

			// Now based on the Command Function, take action. Not all codes are expected or result in action
			switch (MasterCommandProtectedData.CurrentFunctionCode)
			{
				case ANALOG_UNCONDITIONAL: // What we sent and reply
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
					success = ProcessDigitalScan(Header, CompleteMD3Message);
					break;
				case DIGITAL_UNCONDITIONAL:
					LOGDEBUG("Doing Digital Unconditional (new) processing - which is the same as Digital Scan");
					success = ProcessDigitalScan(Header, CompleteMD3Message);
					break;
				case ANALOG_NO_CHANGE_REPLY:
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
					success = ProcessPOMReturn(Header, CompleteMD3Message);
					break;
				case DOM_TYPE_CONTROL:
					success = ProcessDOMReturn(Header, CompleteMD3Message);
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
					success = ProcessSetDateTimeReturn(Header, CompleteMD3Message);
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
			      MasterCommandProtectedData.CurrentCommandTimeoutTimer->cancel(); // Have to be careful the handler still might do something?
			      MasterCommandProtectedData.ProcessingMD3Command = false;         // Only gets reset on success or timeout.

			      // Execute the callback with a success code.
			      PostCallbackCall(MasterCommandProtectedData.CurrentCommand.second, CommandStatus::SUCCESS); // Does null check
			      UnprotectedSendNextMasterCommand(false);                                                    // We already have the strand, so don't need the wrapper here. Pass in that this is not a retry.
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
			LOGDEBUG("Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value " + to_hexstring(AnalogValues[i]));
			int intres;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("Published Event - Analog - Index " + std::to_string(intres) + " Value " + to_hexstring(AnalogValues[i]));
				PublishEvent(Analog(AnalogValues[i], qual, (opendnp3::DNPTime)now), intres); // We don’t get counter time information through MD3, so add it as soon as possible
			}
		}
		else if (SetCounterValueUsingMD3Index(maddress, idx, AnalogValues[i]))
		{
			// We have succeeded in setting the value
			LOGDEBUG("Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value " + to_hexstring(AnalogValues[i]));
			int intres;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, AnalogValues[i],now);
				LOGDEBUG("Published Event - Counter - Index " + std::to_string(intres) + " Value " + to_hexstring(AnalogValues[i]));
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
bool MD3MasterPort::ProcessAnalogDeltaScanReturn(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
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

			LOGDEBUG("Set Analog - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value " + to_hexstring(wordres));

			int intres;
			if (GetAnalogODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled, wordres, now);
				LOGDEBUG("Published Event - Analog Index " + std::to_string(intres) + " Value " + to_hexstring(wordres));
				PublishEvent(Analog(wordres, qual, (opendnp3::DNPTime)now), intres); // We don't get counter time information through MD3, so add it as soon as possible
			}
		}
		else if (GetCounterValueUsingMD3Index(maddress, idx,wordres,hasbeenset))
		{
			wordres += AnalogDeltaValues[i]; // Add the signed delta.
			SetCounterValueUsingMD3Index(maddress, idx, wordres);

			LOGDEBUG("Set Counter - Module " + std::to_string(maddress) + " Channel " + std::to_string(idx) + " Value " + to_hexstring(wordres));

			int intres;
			if (GetCounterODCIndexUsingMD3Index(maddress, idx, intres))
			{
				uint8_t qual = CalculateAnalogQuality(enabled,wordres, now);
				LOGDEBUG("Published Event - Counter Index " + std::to_string(intres) + " Value " + to_hexstring(wordres));
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

bool MD3MasterPort::ProcessDigitalScan(MD3BlockFormatted & Header, const MD3Message_t& CompleteMD3Message)
{
	MD3BlockFn11StoM Fn11Header = static_cast<MD3BlockFn11StoM>(Header);

	uint8_t EventCnt = Fn11Header.GetTaggedEventCount();
	uint8_t SeqNum = Fn11Header.GetDigitalSequenceNumber();
	uint8_t ModuleCount = Fn11Header.GetModuleCount();
	int MessageIndex = 1; // What block are we processing?
	LOGDEBUG("Digital Scan (new) processing Events : "+std::to_string(EventCnt)+" Modules : "+std::to_string(ModuleCount) );
	LOGDEBUG("Digital Scan Data " + MD3MessageAsString(CompleteMD3Message));

	// Now take the returned values and store them into the points
	// Process any module data (if any)
	//TODO: The module data on initial scan might have two blocks for the one address - the previous and current state????
	for (int m = 0; m < ModuleCount; m++)
	{
		if (MessageIndex < (int)CompleteMD3Message.size())
		{
			// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
			uint8_t ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(0);
			uint8_t msecOffset = CompleteMD3Message[MessageIndex].GetByte(1);
			uint16_t ModuleData = CompleteMD3Message[MessageIndex].GetSecondWord();

			if (ModuleAddress == 0 && msecOffset == 0)
			{
				// This is a status block.
				ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(2);
				uint8_t ModuleFailStatus = CompleteMD3Message[MessageIndex].GetByte(3);
				LOGDEBUG("Module : " + std::to_string(ModuleAddress) + " Fail Status : " + to_hexstring(ModuleFailStatus));
			}
			else
			{
				//TODO: We need to maintain a "memory" of the modules bits, so that we can trigger ODC events only on the changes..
				//TODO: Spit out binary 0101 string...
				LOGDEBUG("Module : " + std::to_string(ModuleAddress) + " Data : " + to_hexstring(ModuleData));
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
		// Process Time/Date Data
		if (MessageIndex < (int)CompleteMD3Message.size())
		{
			//TODO: Check word order in timedate data?
			time_t timebase = (time_t)(CompleteMD3Message[MessageIndex].GetData()) * 1000; //MD3Time msec since Epoch.
			std::string LocalTime = std::asctime(std::localtime(&timebase));
			std::string GMTime = std::asctime(std::gmtime(&timebase));
			LOGDEBUG("TimeDate Packet GMT : " + GMTime + "  Local : " + LocalTime);
		}
		else
		{
			LOGERROR("Tried to read a block past the end of the message processing time date data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteMD3Message.size()));
			return false;
		}
		MessageIndex++;

		//TODO: Now we have to convert the remaining data blocks into an array of words and process them. This is due to the time offset blocks which are only 16 bits long. We can also get a 32 bit flag block.
		for (int i = 0; i < EventCnt; i++)
		{
			if (MessageIndex < (int)CompleteMD3Message.size())
			{
				// The data blocks are the same for time tagged and "normal". Module Address (byte), msec offset(byte) and 16 bits of data.
				uint8_t ModuleAddress = CompleteMD3Message[MessageIndex].GetByte(0);
				uint8_t msecOffset = CompleteMD3Message[MessageIndex].GetByte(1);
				uint16_t ModuleData = CompleteMD3Message[MessageIndex].GetSecondWord();

				if (ModuleAddress == 0)
				{
					// This is a time offset block.
					uint8_t msecOffset = CompleteMD3Message[MessageIndex].GetByte(4);
					LOGDEBUG("TimeOffset : " + std::to_string(msecOffset));
				}
				else
				{
					//TODO: We need to maintain a "memory" of the modules bits, so that we can trigger ODC events only on the changes..
					//TODO: Spit out binary 0101 string...
					LOGDEBUG("Module : " + std::to_string(ModuleAddress) + " Offset : " + std::to_string(msecOffset) + " Data : " + to_hexstring(ModuleData));
				}
			}
			else
			{
				LOGERROR("Tried to read a block past the end of the message processing time tagged data : " + std::to_string(MessageIndex) + " Modules : " + std::to_string(CompleteMD3Message.size()));
				return false;
			}
			MessageIndex++;
		}
	}
	return true;
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

// Check that what we got is one that is expected for the current Function we are processing.
bool MD3MasterPort::AllowableResponseToFunctionCode(uint8_t CurrentFunctionCode, uint8_t FunctionCode)
{
	// Only some of these we should receive at the Master!
	bool result = false;
	bool nonrespondcode = false;

	switch (CurrentFunctionCode)
	{
		case ANALOG_UNCONDITIONAL: // Command and reply
			result = (FunctionCode == ANALOG_UNCONDITIONAL);
			break;
		case ANALOG_DELTA_SCAN: // Command and reply
			result = (FunctionCode == ANALOG_DELTA_SCAN) || (FunctionCode == ANALOG_UNCONDITIONAL) || (FunctionCode == ANALOG_NO_CHANGE_REPLY);
			break;
		case DIGITAL_UNCONDITIONAL_OBS:
			result = (FunctionCode == DIGITAL_UNCONDITIONAL_OBS) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY);
			break;
		case DIGITAL_DELTA_SCAN:
			result = (FunctionCode == DIGITAL_DELTA_SCAN) || (FunctionCode == DIGITAL_UNCONDITIONAL_OBS) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY);
			break;
		case HRER_LIST_SCAN:
			result = (FunctionCode == HRER_LIST_SCAN);
			break;
		case DIGITAL_CHANGE_OF_STATE:
			result = (FunctionCode == DIGITAL_CHANGE_OF_STATE) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY);
			break;
		case DIGITAL_CHANGE_OF_STATE_TIME_TAGGED:
			result = (FunctionCode == DIGITAL_CHANGE_OF_STATE_TIME_TAGGED) || (FunctionCode == DIGITAL_NO_CHANGE_REPLY);
			break;
		case DIGITAL_UNCONDITIONAL:
			result = (FunctionCode == DIGITAL_UNCONDITIONAL) || (DIGITAL_CHANGE_OF_STATE_TIME_TAGGED); // The format of both commands is the same. Spec does not indicate a DCOS message, but we get them!
			break;
		case ANALOG_NO_CHANGE_REPLY:
			nonrespondcode = true;
			break;
		case DIGITAL_NO_CHANGE_REPLY:
			nonrespondcode = true;
			break;
		case CONTROL_REQUEST_OK:
			nonrespondcode = true;
			break;
		case FREEZE_AND_RESET:
			result = (FunctionCode == CONTROL_REQUEST_OK); // || (FunctionCode == CONTROL_OR_SCAN_REQUEST_REJECTED);
			break;
		case POM_TYPE_CONTROL:
			result = (FunctionCode == CONTROL_REQUEST_OK); // || (FunctionCode == CONTROL_OR_SCAN_REQUEST_REJECTED);
			break;
		case DOM_TYPE_CONTROL:
			result = (FunctionCode == CONTROL_REQUEST_OK); // || (FunctionCode == CONTROL_OR_SCAN_REQUEST_REJECTED);
			break;
		case INPUT_POINT_CONTROL:
			nonrespondcode = true;
			break;
		case RAISE_LOWER_TYPE_CONTROL:
			nonrespondcode = true;
			break;
		case AOM_TYPE_CONTROL:
			result = (FunctionCode == CONTROL_REQUEST_OK); // || (FunctionCode == CONTROL_OR_SCAN_REQUEST_REJECTED);
			break;
		case CONTROL_OR_SCAN_REQUEST_REJECTED:
			// Master Only
			nonrespondcode = true;
			break;
		case COUNTER_SCAN:
			result = (FunctionCode == COUNTER_SCAN);
			break;
		case SYSTEM_SIGNON_CONTROL:
			result = (FunctionCode == SYSTEM_SIGNON_CONTROL);
			break;
		case SYSTEM_SIGNOFF_CONTROL:
			nonrespondcode = true;
			break;
		case SYSTEM_RESTART_CONTROL: // There is no response to this command..
			nonrespondcode = true;
			break;
		case SYSTEM_SET_DATETIME_CONTROL:
			result = (FunctionCode == CONTROL_REQUEST_OK); // Only Success.. || (FunctionCode == CONTROL_OR_SCAN_REQUEST_REJECTED);
			break;
		case FILE_DOWNLOAD:
			nonrespondcode = true;
			break;
		case FILE_UPLOAD:
			nonrespondcode = true;
			break;
		case SYSTEM_FLAG_SCAN:
			result = (FunctionCode == SYSTEM_FLAG_SCAN);
			break;
		case LOW_RES_EVENTS_LIST_SCAN:
			nonrespondcode = true;
			break;
		default:
			LOGERROR("Illegal Function Code Received - " + std::to_string(FunctionCode));
			break;
	}
	if (nonrespondcode)
		LOGERROR("Received a function code at Master that we cannot respond to (slave only?) : " + std::to_string(FunctionCode));

	return result;
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

		// Request Analog Unconditional
		int ModuleAddress = mait->first;
		int Channels = mait->second;

		if (MyPointConf()->PollGroups[pollgroup].ModuleAddresses.size() > 1)
		{
			LOGERROR("Analog Poll group "+std::to_string(pollgroup)+" is configured for more than one MD3 address. Please create another poll group.");
		}

		// We need to do an analog unconditional on start up, until all the points have a valid value - even 0x8000 for does not exist.
		// To do this we check the hasbeenset flag, which will be false on start up, and aslo set to false on comms lost event - kind of like a quality.
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
			// So now need to work out the start Module address and the number of channels to scan (can be Analog or Counter)
			// For an analog, only one command to get the maximum of 16 channels. For counters it might be two modules that we can get with one command.
			LOGDEBUG("Poll Issued a Analog Unconditional Command");

			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_UNCONDITIONAL, ModuleAddress, Channels, true);

			QueueMD3Command(commandblock,nullptr);
		}
		else
		{
			LOGDEBUG("Poll Issued a Analog Delta Command");
			// Use a delta command Fn 6
			MD3BlockFormatted commandblock(MyConf()->mAddrConf.OutstationAddr, true, ANALOG_DELTA_SCAN, ModuleAddress, Channels, true);
		}
	}

	if (MyPointConf()->PollGroups[pollgroup].polltype == BinaryPoints)
	{
		if (MyPointConf()->NewDigitalCommands) // Old are 7,8,9,10 - New are 11 and 12
		{
			if (MyPointConf()->PollGroups[pollgroup].ForceUnconditional)
			{
				// Use Unconditional Request Fn 12
				//TODO:  Handle for than one DIM in a poll group...
				LOGDEBUG("Poll Issued a Digital Unconditional (new) Command");
				ModuleMapType::iterator FirstModule = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.begin();

				// Request Digital Unconditional
				int ModuleAddress = FirstModule->first;
				// We expect the digital modules to be consecutive, or of there is a gap this will still work.
				int Modules = MyPointConf()->PollGroups[pollgroup].ModuleAddresses.size(); // Most modules we can get in one command - NOT channels!

				//TODO: Need a random value on startup? Or 0
				int sequencecount = 2; // This need to be maintained globally and is mod 16, but not 0.

				MD3BlockFormatted commandblock = MD3BlockFn12MtoS(MyConf()->mAddrConf.OutstationAddr, ModuleAddress, sequencecount, Modules);

				QueueMD3Command(commandblock, nullptr); // No callback, does not originate from ODC
			}
			else
			{
				// Use a delta command Fn 11
				LOGDEBUG("Poll Issued a Digital Delta (new) Command");
			}
		}
		else
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
		// Send a time set command to the OutStation, TimeChange command (Fn 43)
		uint64_t currenttime = MD3Now(); //TODO: Timeset command is UTC or local time?

		LOGDEBUG("Poll Issued a TimeDate Command");
		SendTimeDateChangeCommand(currenttime, nullptr);
	}
}
void MD3MasterPort::EnablePolling(bool on)
{
	if (on)
		PollScheduler->Start();
	else
		PollScheduler->Stop();
}
void MD3MasterPort::SendTimeDateChangeCommand(const uint64_t &currenttime, SharedStatusCallback_t pStatusCallback)
{
	MD3BlockFn43MtoS commandblock(MyConf()->mAddrConf.OutstationAddr, currenttime % 1000);
	MD3BlockData datablock((uint32_t)(currenttime / 1000), true);
	MD3Message_t Cmd;
	Cmd.push_back(commandblock);
	Cmd.push_back(datablock);
	QueueMD3Command(Cmd, pStatusCallback);
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
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
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

	PostCallbackCall(pStatusCallback, CommandStatus::SUCCESS);
}

//Implement some IOHandler - parent MD3Port implements the rest to return NOT_SUPPORTED
void MD3MasterPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }
void MD3MasterPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) { EventT(arCommand, index, SenderName, pStatusCallback); }


// So we have received an event, which for the Master will result in a write to the Outstation, so the command is a Binary Output or Analog Output
// see all 5 possible definitions above.
// We will have to translate from the float values to the uint16_t that MD3 actually handles, and then it is only a 12 bit number.
// Also we have the pass through commands with special port values defined.
template<typename T>
inline void MD3MasterPort::EventT(T& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
		return;
	}

	// We now have to send a command out to an outstation, add the callback function pointer to the command and return.
	// The callback will be called on timeout or success.
	WriteObject(arCommand, index, pStatusCallback);
}

template<>
void MD3MasterPort::WriteObject(const ControlRelayOutputBlock& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	//TODO: On start up - we must read the current state of the outputs to initialise the system, so any commands we send subsequently are correct.

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
			outputselection = Channel;
			OnOffString = "OFF";
		}
		else
		{
			// ON Command  --> ControlCode::PULSE_CLOSE || ControlCode::PULSE || ControlCode::LATCH_ON
			outputselection = Channel+8;
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

template<>
void MD3MasterPort::WriteObject(const AnalogOutputInt16& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// AOM Command
	LOGDEBUG("Master received a AOM ODC Change Command " + std::to_string(index));
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

template<>
void MD3MasterPort::WriteObject(const AnalogOutputInt32& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	// Other Magic point commands
	if (index == MyPointConf()->POMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received POM Control Point command on the magic point through ODC " + std::to_string(index));
		uint32_t blockdata = static_cast<uint32_t>(command.value);
		MD3BlockFn17MtoS Header = MD3BlockFn17MtoS(MD3BlockData(blockdata));

		SendPOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputSelection(), pStatusCallback);
		return;
	}

	LOGDEBUG("Master received unknown AnalogOutputInt32 ODC Event " + std::to_string(index));
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

template<>
void MD3MasterPort::WriteObject(const AnalogOutputFloat32& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	LOGERROR("On Master AnalogOutputFloat32 Type is not implemented " + std::to_string(index));
	PostCallbackCall(pStatusCallback, CommandStatus::UNDEFINED);
}

template<>
void MD3MasterPort::WriteObject(const AnalogOutputDouble64& command, const uint16_t &index, const SharedStatusCallback_t &pStatusCallback)
{
	if (index == MyPointConf()->TimeSetPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received a Time Change command on the magic point through ODC " + std::to_string(index));
		uint64_t currenttime = static_cast<uint64_t>(command.value);

		SendTimeDateChangeCommand(currenttime, pStatusCallback);
		return;
	}
	else if (index == MyPointConf()->DOMControlPoint.second) // Is this out magic time set point?
	{
		LOGDEBUG("Master received DOM Control Point command on the magic point through ODC " + std::to_string(index));
		uint64_t blockdata = static_cast<uint64_t>(command.value);
		uint32_t firstblock = (blockdata >> 32);
		uint32_t secondblock = (blockdata & 0xFFFFFFFF);

		MD3BlockFn19MtoS Header = MD3BlockFn19MtoS(MD3BlockData(firstblock));
		MD3BlockData BlockData(secondblock);

		SendDOMOutputCommand(MyConf()->mAddrConf.OutstationAddr, Header.GetModuleAddress(), Header.GetOutputFromSecondBlock(BlockData), pStatusCallback);
		return;
	}

	LOGDEBUG("Master received unknown AnalogOutputDouble64 ODC Event " + std::to_string(index));
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



