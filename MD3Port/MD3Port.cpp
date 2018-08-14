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


#include <iostream>
#include "MD3Port.h"
#include "MD3PortConf.h"

MD3Port::MD3Port(const std::string &aName, const std::string & aConfFilename, const Json::Value & aConfOverrides):
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
	// This is all the port specific stuff.

	if (!JSONRoot.isObject()) return;

	if (JSONRoot.isMember("IP") && JSONRoot.isMember("SerialDevice"))
		LOGERROR("Warning: MD3 port serial device AND IP address specified - IP overrides");

	if (JSONRoot.isMember("IP"))
	{
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.IP = JSONRoot["IP"].asString();
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
			LOGERROR("Warning: Invalid TCP client/server type, it should be CLIENT, SERVER, or DEFAULT : "+ JSONRoot["TCPClientServer"].asString());
	}

	if (JSONRoot.isMember("OutstationAddr"))
		static_cast<MD3PortConf*>(pConf.get())->mAddrConf.OutstationAddr = JSONRoot["OutstationAddr"].asUInt();

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
void MD3Port::SendMD3Message(const MD3Message_t &CompleteMD3Message)
{
	if (CompleteMD3Message.size() == 0)
	{
		LOGERROR("Tried to send an empty message to the TCP Port");
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

#pragma region Analog-Counter

bool MD3Port::GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalog();
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3Port::GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	// Change being update the last read value
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
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
		MD3PointMapIter->second->SetAnalog(meas,MD3Now());
		return true;
	}
	return false;
}
bool MD3Port::GetCounterODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3Port::SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->CounterODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->CounterODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}

bool MD3Port::GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalog();
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetIndex();
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
		MD3PointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalog();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3Port::SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf()->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}

#pragma endregion

#pragma region Binary

bool MD3Port::GetBinaryODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3Port::GetBinaryQualityUsingMD3Index(const uint16_t module, const uint8_t channel, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
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
		res = MD3PointMapIter->second->GetBinary();
		changed = MD3PointMapIter->second->GetAndResetChangedFlag();
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
		res = MD3PointMapIter->second->GetBinary();
		return true;
	}
	return false;
}
// Get the changed flag without resetting it
bool MD3Port::GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		changed = MD3PointMapIter->second->GetChangedFlag();
		return true;
	}
	return false;
}

bool MD3Port::SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas, bool &valuechanged)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryMD3PointMap.end())
	{
		//TODO: Put this functionality into the MD3BinaryPoint class
		// If it has been changed, or has never been set...
		if ((MD3PointMapIter->second->GetBinary() != meas) || (MD3PointMapIter->second->GetHasBeenSet() == false))
		{
			MD3PointMapIter->second->SetBinary(meas, MD3Now());
			valuechanged = true;
		}
		return true;
	}
	return false;
}
// Get value and reset changed flag..
bool MD3Port::GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		changed = ODCPointMapIter->second->GetAndResetChangedFlag();
		return true;
	}
	return false;
}
bool MD3Port::SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, MD3Time eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->SetBinary(meas, eventtime);

		// We now need to add the change to the separate digital/binary event list
		if (IsOutStation)
			AddToDigitalEvents(*ODCPointMapIter->second); // Don't store if master - we just fire off ODC events.

		return true;
	}
	return false;
}

void MD3Port::AddToDigitalEvents(MD3BinaryPoint &inpt)
{

	if (inpt.GetPointType() == TIMETAGGEDINPUT) // Only add if the point not configured for time tagged data
	{
		MD3BinaryPoint pt = MD3BinaryPoint(inpt);

		if (MyConf()->pPointConf->NewDigitalCommands)
		{
			// Have to collect all the bits in the module to which this point belongs into a uint16_t,
			// just to support COS Fn 11 where the whole 16 bits are returned for a possibly single bit change.
			// Do not effect the change flags which are needed for normal scanning
			bool ModuleFailed = false;
			uint16_t wordres = CollectModuleBitsIntoWord(pt.GetModuleAddress(), ModuleFailed);

			// Save it in the snapshot that is used for the Fn11 COS time tagged events.
			pt.SetModuleBinarySnapShot(wordres);
			pBinaryModuleTimeTaggedEventQueue->async_push(pt);
		}
		else
		{
			// Will fail if full, which is the defined MD3 behaviour. Push takes a copy
			pBinaryTimeTaggedEventQueue->async_push(pt);
		}
	}
}
uint16_t MD3Port::CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool &ModuleFailed)
{
	uint16_t wordres = 0;

	for (int j = 0; j < 16; j++)
	{
		uint8_t bitres = 0;
		bool changed = false; // We don't care about the returned value

		if (GetBinaryValueUsingMD3Index(ModuleAddress, j, bitres, changed)) // Reading this clears the changed bit
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
		bool changed = false; // We don't care about the returned value

		if (GetBinaryValueUsingMD3Index(ModuleAddress, j, bitres)) // Reading this clears the changed bit
		{
			//TODO: Check the bit order here of the binaries
			wordres |= (uint16_t)bitres << (15 - j);
		}
		//TODO: Check and update the module failed status for this module.
	}
	return wordres;
}


#pragma endregion

#pragma region Control

bool MD3Port::GetBinaryControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf()->BinaryControlMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->BinaryControlMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3Port::GetBinaryControlMD3IndexUsingODCIndex(const size_t index, uint8_t &module, uint8_t &channel, BinaryPointType &pointtype)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf()->BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf()->BinaryControlODCPointMap.end())
	{
		module = ODCPointMapIter->second->GetModuleAddress();
		channel = ODCPointMapIter->second->GetChannel();
		pointtype = ODCPointMapIter->second->GetPointType();
		return true;
	}
	return false;
}
bool MD3Port::GetAnalogControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf()->AnalogControlMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf()->AnalogControlMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}

#pragma endregion


#pragma endregion
