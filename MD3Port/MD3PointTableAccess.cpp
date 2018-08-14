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
* MD3PointTableAccess.cpp
*
*  Created on: 14/08/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include <iostream>

#include "MD3PortConf.h"
#include "MD3PointTableAccess.h"

MD3PointTableAccess::MD3PointTableAccess(const bool isoutstation, std::shared_ptr<MD3PointConf> MPC, asio::io_service& IOS):
	MyPointConf(MPC)
{
	pBinaryTimeTaggedEventQueue.reset(new StrandProtectedQueue<MD3BinaryPoint>(IOS, 256));
}

#pragma region Analog-Counter


bool MD3PointTableAccess::GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalog();
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	// Change being update the last read value
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::SetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->CounterMD3PointMap.end())
	{
		MD3PointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetCounterODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->CounterMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->CounterMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf->CounterODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->CounterODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}

bool MD3PointTableAccess::GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalog();
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetAnalogODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->AnalogMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->AnalogMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->AnalogMD3PointMap.end())
	{
		MD3PointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetAnalogValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalog();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = MyPointConf->AnalogODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, MD3Now());
		return true;
	}
	return false;
}

#pragma endregion

#pragma region Binary

bool MD3PointTableAccess::GetBinaryODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetBinaryQualityUsingMD3Index(const uint16_t module, const uint8_t channel, bool &hasbeenset)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
	{
		hasbeenset = MD3PointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}

// Gets and Clears changed flag
bool MD3PointTableAccess::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetBinary();
		changed = MD3PointMapIter->second->GetAndResetChangedFlag();
		return true;
	}
	return false;
}
// Only gets value, does not clear changed flag
bool MD3PointTableAccess::GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
	{
		res = MD3PointMapIter->second->GetBinary();
		return true;
	}
	return false;
}
// Get the changed flag without resetting it
bool MD3PointTableAccess::GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
	{
		changed = MD3PointMapIter->second->GetChangedFlag();
		return true;
	}
	return false;
}

bool MD3PointTableAccess::SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas, bool &valuechanged)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryMD3PointMap.end())
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
bool MD3PointTableAccess::GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		changed = ODCPointMapIter->second->GetAndResetChangedFlag();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, MD3Time eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf->BinaryODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->SetBinary(meas, eventtime);

		// We now need to add the change to the separate digital/binary event list
		if (IsOutStation)
			AddToDigitalEvents(*ODCPointMapIter->second); // Don't store if master - we just fire off ODC events.

		return true;
	}
	return false;
}

void MD3PointTableAccess::AddToDigitalEvents(MD3BinaryPoint &inpt)
{
	if (inpt.GetPointType() == TIMETAGGEDINPUT) // Only add if the point not configured for time tagged data
	{
		MD3BinaryPoint pt = MD3BinaryPoint(inpt);

		if (MyPointConf->NewDigitalCommands)
		{
			// Have to collect all the bits in the module to which this point belongs into a uint16_t,
			// just to support COS Fn 11 where the whole 16 bits are returned for a possibly single bit change.
			// Do not effect the change flags which are needed for normal scanning
			bool ModuleFailed = false;
			uint16_t wordres = CollectModuleBitsIntoWord(pt.GetModuleAddress(), ModuleFailed);

			// Save it in the snapshot that is used for the Fn11 COS time tagged events.
			pt.SetModuleBinarySnapShot(wordres);
		}
		// Will fail if full, which is the defined MD3 behaviour. Push takes a copy
		pBinaryTimeTaggedEventQueue->async_push(pt);
	}
}
uint16_t MD3PointTableAccess::CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool &ModuleFailed)
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
uint16_t MD3PointTableAccess::CollectModuleBitsIntoWord(const uint8_t ModuleAddress, bool &ModuleFailed)
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

bool MD3PointTableAccess::GetBinaryControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3BinaryPointMapIterType MD3PointMapIter = MyPointConf->BinaryControlMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->BinaryControlMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetBinaryControlMD3IndexUsingODCIndex(const size_t index, uint8_t &module, uint8_t &channel, BinaryPointType &pointtype)
{
	ODCBinaryPointMapIterType ODCPointMapIter = MyPointConf->BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != MyPointConf->BinaryControlODCPointMap.end())
	{
		module = ODCPointMapIter->second->GetModuleAddress();
		channel = ODCPointMapIter->second->GetChannel();
		pointtype = ODCPointMapIter->second->GetPointType();
		return true;
	}
	return false;
}
bool MD3PointTableAccess::GetAnalogControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t Md3Index = (module << 8) | channel;

	MD3AnalogCounterPointMapIterType MD3PointMapIter = MyPointConf->AnalogControlMD3PointMap.find(Md3Index);
	if (MD3PointMapIter != MyPointConf->AnalogControlMD3PointMap.end())
	{
		index = MD3PointMapIter->second->GetIndex();
		return true;
	}
	return false;
}

// Dumps the points out in a list, only used for UnitTests
std::vector<MD3BinaryPoint> MD3PointTableAccess::DumpTimeTaggedPointList()
{
	MD3BinaryPoint CurrentPoint;
	std::vector<MD3BinaryPoint> PointList(50);

	while (pBinaryTimeTaggedEventQueue->sync_front(CurrentPoint))
	{
		PointList.emplace_back(CurrentPoint);
		pBinaryTimeTaggedEventQueue->sync_pop();
	}

	return PointList;
}
bool MD3PointTableAccess::PeekNextTaggedEventPoint(MD3BinaryPoint &pt)
{
	return pBinaryTimeTaggedEventQueue->sync_front(pt);
}
bool MD3PointTableAccess::PopNextTaggedEventPoint()
{
	return pBinaryTimeTaggedEventQueue->sync_pop();
}
bool MD3PointTableAccess::TimeTaggedDataAvailable()
{
	return !pBinaryTimeTaggedEventQueue->sync_empty();
}
#pragma endregion
