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
* MD3Port.cpp
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#include "MD3Port.h"
#include <iostream>
#include "MD3PortConf.h"
#include <openpal/logging/LogLevels.h>

MD3Port::MD3Port(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides) :
	DataPort(aName, aConfFilename, aConfOverrides),
	pConnection(nullptr)
{
	//the creation of a new MD3PortConf will get the point details
	pConf.reset(new MD3PortConf(ConfFilename, ConfOverrides));

	//We still may need to process the file (or overrides) to get Addr details:
	ProcessFile();
}

TCPClientServer MD3Port::ClientOrServer()
{
	if (MyConf()->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::SERVER;
	return MyConf()->mAddrConf.ClientServer;
}
bool MD3Port::IsServer()
{
	return (TCPClientServer::SERVER == ClientOrServer());
}

//DataPort function for UI
/*
const Json::Value MD3Port::GetStatus() const
{
	auto ret_val = Json::Value();

	if (!enabled)
		ret_val["Result"] = "Port disabled";
	else if (link_dead)
		ret_val["Result"] = "Port enabled - link down";
	else if (status == opendnp3::LinkStatus::RESET)
		ret_val["Result"] = "Port enabled - link up (reset)";
	else if (status == opendnp3::LinkStatus::UNRESET)
		ret_val["Result"] = "Port enabled - link up (unreset)";
	else
		ret_val["Result"] = "Port enabled - link status unknown";

	return ret_val;
}
*/

void MD3Port::ProcessElements(const Json::Value& JSONRoot)
{
	// The points are handled by the MD3PointConf class.
	// This is all the port specfic stuff.

	if (!JSONRoot.isObject()) return;

	if (JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		std::cout << "Warning: MD3 port serial device AND IP address specified - IP overrides" << std::endl;

	if (JSONRoot.isMember("IP"))
	{
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.SerialSettings.deviceName = "";
	}

	if (JSONRoot.isMember("Port"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.Port = JSONRoot["Port"].asUInt();

	if (JSONRoot.isMember("TCPClientServer"))
	{
		if (JSONRoot["TCPClientServer"].asString() == "CLIENT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::CLIENT;
		else if (JSONRoot["TCPClientServer"].asString() == "SERVER")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::SERVER;
		else if (JSONRoot["TCPClientServer"].asString() == "DEFAULT")
			static_cast<MD3PortConf*>(pConf.get())->mAddrConf.ClientServer = TCPClientServer::DEFAULT;
		else
			std::cout << "Warning: Invalid TCP client/server type: " << JSONRoot["TCPClientServer"].asString() << ", should be CLIENT, SERVER, or DEFAULT." << std::endl;
	}

	if (JSONRoot.isMember("OutstationAddr"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

	if (JSONRoot.isMember("NewDigitalCommands"))
	{
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.NewDigitalCommands = JSONRoot["NewDigitalCommands"].asBool();
	}
}

int MD3Port::Limit(int val, int max)
{
	return val > max ? max : val;
}
MD3PortConf* MD3Port::MyConf()
{
	return static_cast<MD3PortConf*>(this->pConf.get());
}

std::shared_ptr<MD3PointConf> MD3Port::MyPointConf()
{
	return MyConf()->pPointConf;
}

void MD3Port::SetSendTCPDataFn(std::function<void(std::string)> Send)
{
	SendTCPDataFn = Send;
}

// Test only method for simulating input from the TCP Connection.
void MD3Port::InjectSimulatedTCPMessage(buf_t&readbuf)
{
	// Just pass to the Connection ReadCompletionHandler, as if it had come in from the TCP port
	pConnection->ReadCompletionHandler(readbuf);
}

// The only method that sends to the TCP Socket
void MD3Port::SendMD3Message(std::vector<MD3BlockData> &CompleteMD3Message)
{
	if (CompleteMD3Message.size() == 0)
	{
		LOG("MD3Port", openpal::logflags::ERR, "", "Tried to send an empty response");
		return;
	}

	// Turn the blocks into a binary string.
	std::string MD3Message;
	for (auto blk : CompleteMD3Message)
	{
		MD3Message += blk.ToBinaryString();
	}

	// This is a pointer to a function, so that we can hook it for testing. Otherwise calls the pSockMan Write templated function
	// Small overhead to allow for testing - Is there a better way? - could not hook the pSockMan->Write function and/or another passed in function due to differences between a method and a lambda
	if (SendTCPDataFn != nullptr)
		SendTCPDataFn(MD3Message);
	else
	{
		pConnection->Write(std::string(MD3Message));
	}
}

#pragma region  PointTableAccess

bool MD3Port::GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3Port::GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta)
{
	// Change being update the last read value
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		delta = (int)res - (int)MD3PointMapIter->second->LastReadAnalog;
		MD3PointMapIter->second->LastReadAnalog = res;	// We assume it is sent to the master. May need better way to do this
		return true;
	}
	return false;
}
bool MD3Port::SetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		MD3PointMapIter->second->Analog = meas;
		MD3PointMapIter->second->ChangedTime = MD3Now();
		return true;
	}
	return false;
}
bool MD3Port::GetCounterODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, int &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Index;
		return true;
	}
	return false;
}
bool MD3Port::SetCounterValueUsingODCIndex(const uint16_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->CounterODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->CounterODCPointMap.end())
	{
		ODCPointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}

bool MD3Port::GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Analog;
		delta = (int)res - (int)MD3PointMapIter->second->LastReadAnalog;
		MD3PointMapIter->second->LastReadAnalog = res;	// We assume it is sent to the master. May need better way to do this
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, int &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Index;
		return true;
	}
	return false;
}
bool MD3Port::SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		MD3PointMapIter->second->Analog = meas;
		MD3PointMapIter->second->ChangedTime = MD3Now();
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogValueUsingODCIndex(const uint16_t index, uint16_t &res)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->Analog;
		return true;
	}
	return false;
}
bool MD3Port::SetAnalogValueUsingODCIndex(const uint16_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->Analog = meas;
		return true;
	}
	return false;
}
bool MD3Port::GetBinaryODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, int &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Index;
		return true;
	}
	return false;
}
// Gets and Clears changed flag
bool MD3Port::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Binary;
		if (MD3PointMapIter->second->Changed)
		{
			changed = true;
			MD3PointMapIter->second->Changed = false;
		}
		return true;
	}
	return false;
}
// Only gets value, does not clear changed flag
bool MD3Port::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->Binary;
		return true;
	}
	return false;
}
bool MD3Port::GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		if (MD3PointMapIter->second->Changed)
		{
			changed = MD3PointMapIter->second->Changed;
		}
		return true;
	}
	return false;
}

bool MD3Port::SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		MD3PointMapIter->second->Binary = meas;
		MD3PointMapIter->second->Changed = true;
		MD3PointMapIter->second->ChangedTime = MD3Now();
		return true;
	}
	return false;
}
bool MD3Port::GetBinaryValueUsingODCIndex(const uint16_t index, uint8_t &res, bool &changed)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->Binary;
		if (ODCPointMapIter->second->Changed)
		{
			changed = true;
			ODCPointMapIter->second->Changed = false;
		}
		return true;
	}
	return false;
}
bool MD3Port::SetBinaryValueUsingODCIndex(const uint16_t index, const uint8_t meas, MD3Time eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->Binary = meas;
		ODCPointMapIter->second->Changed = true;

		// We now need to add the change to the separate digital/binary event list
		ODCPointMapIter->second->ChangedTime = eventtime;
		MD3BinaryPoint CopyOfPoint(*(ODCPointMapIter->second));
		AddToDigitalEvents(CopyOfPoint);
		return true;
	}
	return false;
}
bool MD3Port::CheckBinaryControlExistsUsingMD3Index(const uint16_t module, const uint8_t channel)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryControlMD3PointMap.find(Md3Index);
	return (MD3PointMapIter != MyPointConf()->BinaryControlMD3PointMap.end());
}

void MD3Port::AddToDigitalEvents(MD3BinaryPoint & pt)
{
	// Will fail if full, which is the defined MD3 behaviour. Push takes a copy
	pBinaryTimeTaggedEventQueue->async_push(pt);

	// Have to collect all the bits in the module to which this point belongs into a uint16_t,
	// just to support COS Fn 11 where the whole 16 bits are returned for a possibly single bit change.
	// Do not effect the change flags which are needed for normal scanning
	bool ModuleFailed = false;
	uint16_t wordres = CollectModuleBitsIntoWord(pt.ModuleAddress, ModuleFailed);

	// Save it in the snapshot that is used for the Fn11 COS time tagged events.
	pt.ModuleBinarySnapShot = wordres;
	pBinaryModuleTimeTaggedEventQueue->async_push(pt);
}
uint16_t MD3Port::CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool &ModuleFailed)
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
uint16_t MD3Port::CollectModuleBitsIntoWord(const uint8_t ModuleAddress, bool &ModuleFailed)
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

#pragma endregion
