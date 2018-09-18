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
* CBPointTableAccess.cpp
*
*  Created on: 14/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/


#include "CBPortConf.h"
#include "CBPointTableAccess.h"

CBPointTableAccess::CBPointTableAccess()
{}

void CBPointTableAccess::Build(const bool isoutstation, asio::io_service& IOS)
{
	IsOutstation = isoutstation;
//	pBinaryTimeTaggedEventQueue.reset(new StrandProtectedQueue<CBBinaryPoint>(IOS, 256));
}

#pragma region Analog-Counter

bool CBPointTableAccess::AddCounterPointToPointTable(const size_t &index, const uint8_t &moduleaddress, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBindex = ShiftLeft8Result16Bits(moduleaddress) | channel;
	if (CounterCBPointMap.find(CBindex) != CounterCBPointMap.end())
	{
		LOGERROR("Duplicate Counter CB Index " + std::to_string(moduleaddress) + " - " + std::to_string(channel));
		return false;
	}

	if (CounterODCPointMap.find(index) != CounterODCPointMap.end())
	{
		LOGERROR("Duplicate Counter ODC Index : " + std::to_string(index));
		return false;
	}

	auto pt = std::make_shared<CBAnalogCounterPoint>(index, moduleaddress, channel, payloadlocation);
	CounterCBPointMap[CBindex] = pt;
	CounterODCPointMap[index] = pt;
	return true;
}
bool CBPointTableAccess::AddAnalogPointToPointTable(const size_t &index, const uint8_t &moduleaddress, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBindex = ShiftLeft8Result16Bits(moduleaddress) | channel;
	if (AnalogCBPointMap.find(CBindex) != AnalogCBPointMap.end())
	{
		LOGERROR("Duplicate Analog CB Index " + std::to_string(moduleaddress) + " - " + std::to_string(channel));
		return false;
	}

	if (AnalogODCPointMap.find(index) != AnalogODCPointMap.end())
	{
		LOGERROR("Duplicate Analog ODC Index : " + std::to_string(index));
		return false;
	}

	auto pt = std::make_shared<CBAnalogCounterPoint>(index, moduleaddress, channel, payloadlocation);
	AnalogCBPointMap[CBindex] = pt;
	AnalogODCPointMap[index] = pt;
	return true;
}
bool CBPointTableAccess::AddAnalogControlPointToPointTable(const size_t &index, const uint8_t &moduleaddress, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBindex = ShiftLeft8Result16Bits(moduleaddress) | channel;
	if (AnalogControlCBPointMap.find(CBindex) != AnalogControlCBPointMap.end())
	{
		LOGERROR("Duplicate Analog CB Index " + std::to_string(moduleaddress) + " - " + std::to_string(channel));
		return false;
	}

	if (AnalogControlODCPointMap.find(index) != AnalogControlODCPointMap.end())
	{
		LOGERROR("Duplicate Analog ODC Index : " + std::to_string(index));
		return false;
	}

	auto pt = std::make_shared<CBAnalogCounterPoint>(index, moduleaddress, channel, payloadlocation);
	AnalogControlCBPointMap[CBindex] = pt;
	AnalogControlODCPointMap[index] = pt;
	return true;
}

bool CBPointTableAccess::AddBinaryPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const BinaryPointType &pointtype, const PayloadLocationType &payloadlocation)
{
	uint16_t CBindex = ShiftLeft8Result16Bits(group) | channel;
	if (BinaryCBPointMap.find(CBindex) != BinaryCBPointMap.end())
	{
		LOGERROR("Duplicate Binary CB Index " + std::to_string(group) + " - " + std::to_string(channel));
		return false;
	}

	if (BinaryODCPointMap.find(index) != BinaryODCPointMap.end())
	{
		LOGERROR("Duplicate Binary ODC Index : " + std::to_string(index));
		return false;
	}

	auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, payloadlocation, pointtype);
	BinaryCBPointMap[CBindex] = pt;
	BinaryODCPointMap[index] = pt;
	return true;
}
bool CBPointTableAccess::AddBinaryControlPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const BinaryPointType &pointtype, const PayloadLocationType &payloadlocation)
{
	uint16_t CBindex = ShiftLeft8Result16Bits(group) | channel;
	if (BinaryControlCBPointMap.find(CBindex) != BinaryControlCBPointMap.end())
	{
		LOGERROR("Duplicate BinaryControl CB Index " + std::to_string(group) + " - " + std::to_string(channel));
		return false;
	}

	if (BinaryControlODCPointMap.find(index) != BinaryControlODCPointMap.end())
	{
		LOGERROR("Duplicate BinaryControl ODC Index : " + std::to_string(index));
		return false;
	}

	auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, payloadlocation, pointtype);
	BinaryControlCBPointMap[CBindex] = pt;
	BinaryControlODCPointMap[index] = pt;
	return true;
}


bool CBPointTableAccess::GetCounterValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = CounterCBPointMap.find(CBIndex);
	if (CBPointMapIter != CounterCBPointMap.end())
	{
		res = CBPointMapIter->second->GetAnalog();
		hasbeenset = CBPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetCounterValueAndChangeUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	// Change being update the last read value
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = CounterCBPointMap.find(CBIndex);
	if (CBPointMapIter != CounterCBPointMap.end())
	{
		res = CBPointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = CBPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetCounterValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = CounterCBPointMap.find(CBIndex);
	if (CBPointMapIter != CounterCBPointMap.end())
	{
		CBPointMapIter->second->SetAnalog(meas, CBNow());
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetCounterODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = CounterCBPointMap.find(CBIndex);
	if (CBPointMapIter != CounterCBPointMap.end())
	{
		res = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = CounterODCPointMap.find(index);
	if (ODCPointMapIter != CounterODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, CBNow());
		return true;
	}
	return false;
}
bool CBPointTableAccess::ResetCounterValueUsingODCIndex(const size_t index)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->ResetAnalog(); // Sets to 0x8000, time = 0, HasBeenSet to false
		return true;
	}
	return false;
}

bool CBPointTableAccess::GetAnalogValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogCBPointMap.end())
	{
		res = CBPointMapIter->second->GetAnalog();
		hasbeenset = CBPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogValueAndChangeUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta, bool &hasbeenset)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogCBPointMap.end())
	{
		res = CBPointMapIter->second->GetAnalogAndDelta(delta);
		hasbeenset = CBPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &res)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogCBPointMap.end())
	{
		res = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetAnalogValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint16_t meas)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogCBPointMap.end())
	{
		CBPointMapIter->second->SetAnalog(meas, CBNow());
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalog();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, CBNow());
		return true;
	}
	return false;
}
bool CBPointTableAccess::ResetAnalogValueUsingODCIndex(const size_t index)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->ResetAnalog(); // Sets to 0x8000, time = 0, HasBeenSet to false
		return true;
	}
	return false;
}

#pragma endregion

#pragma region Binary

bool CBPointTableAccess::GetBinaryODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryQualityUsingCBIndex(const uint16_t module, const uint8_t channel, bool &hasbeenset)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		hasbeenset = CBPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}

// Gets and Clears changed flag
bool CBPointTableAccess::GetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		res = CBPointMapIter->second->GetBinary();
		changed = CBPointMapIter->second->GetAndResetChangedFlag();
		return true;
	}
	return false;
}
// Only gets value, does not clear changed flag
bool CBPointTableAccess::GetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint8_t &res)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		res = CBPointMapIter->second->GetBinary();
		return true;
	}
	return false;
}
// Get the changed flag without resetting it
bool CBPointTableAccess::GetBinaryChangedUsingCBIndex(const uint16_t module, const uint8_t channel, bool &changed)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		changed = CBPointMapIter->second->GetChangedFlag();
		return true;
	}
	return false;
}

bool CBPointTableAccess::SetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint8_t meas, bool &valuechanged)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryCBPointMap.end())
	{
		//TODO: Put this functionality into the CBBinaryPoint class
		// If it has been changed, or has never been set...
		if ((CBPointMapIter->second->GetBinary() != meas) || (CBPointMapIter->second->GetHasBeenSet() == false))
		{
			CBPointMapIter->second->SetBinary(meas, CBNow());
			valuechanged = true;
		}
		return true;
	}
	return false;
}
// Get value and reset changed flag..
bool CBPointTableAccess::GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryODCPointMap.find(index);
	if (ODCPointMapIter != BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		changed = ODCPointMapIter->second->GetAndResetChangedFlag();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, CBTime eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryODCPointMap.find(index);
	if (ODCPointMapIter != BinaryODCPointMap.end())
	{
		ODCPointMapIter->second->SetBinary(meas, eventtime);

		// We now need to add the change to the separate digital/binary event list
		if (IsOutstation)
			AddToDigitalEvents(*ODCPointMapIter->second); // Don't store if master - we just fire off ODC events.

		return true;
	}
	return false;
}

void CBPointTableAccess::AddToDigitalEvents(CBBinaryPoint &inpt)
{
	if (inpt.GetPointType() == TIMETAGGEDINPUT) // Only add if the point not configured for time tagged data
	{
		CBBinaryPoint pt = CBBinaryPoint(inpt);

		// Have to collect all the bits in the module to which this point belongs into a uint16_t,
		// just to support COS Fn 11 where the whole 16 bits are returned for a possibly single bit change.
		// Do not effect the change flags which are needed for normal scanning
		bool ModuleFailed = false;
		uint16_t wordres = CollectModuleBitsIntoWord(pt.GetGroup(), ModuleFailed);

		// Save it in the snapshot that is used for the Fn11 COS time tagged events.
		pt.SetModuleBinarySnapShot(wordres);
		// Will fail if full, which is the defined CB behaviour. Push takes a copy
//		pBinaryTimeTaggedEventQueue->async_push(pt);
	}
}
uint16_t CBPointTableAccess::CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t Group, bool &ModuleFailed)
{
	uint16_t wordres = 0;

	for (uint8_t j = 0; j < 16; j++)
	{
		uint8_t bitres = 0;
		bool changed = false; // We don't care about the returned value

		if (GetBinaryValueUsingCBIndex(Group, j, bitres, changed)) // Reading this clears the changed bit
		{
			//TODO: Check the bit order here of the binaries
			wordres |= static_cast<uint16_t>(bitres) << (15 - j);
		}
	}
	return wordres;
}
uint16_t CBPointTableAccess::CollectModuleBitsIntoWord(const uint8_t Group, bool &ModuleFailed)
{
	uint16_t wordres = 0;

	for (uint8_t j = 0; j < 16; j++)
	{
		uint8_t bitres = 0;

		if (GetBinaryValueUsingCBIndex(Group, j, bitres)) // Reading this clears the changed bit
		{
			//TODO: Check the bit order here of the binaries
			wordres |= static_cast<uint16_t>(bitres) << (15 - j);
		}
	}
	return wordres;
}


#pragma endregion

#pragma region Control

bool CBPointTableAccess::GetBinaryControlODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBBinaryPointMapIterType CBPointMapIter = BinaryControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryControlCBIndexUsingODCIndex(const size_t index, uint8_t &module, uint8_t &channel, BinaryPointType &pointtype)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != BinaryControlODCPointMap.end())
	{
		module = ODCPointMapIter->second->GetGroup();
		channel = ODCPointMapIter->second->GetChannel();
		pointtype = ODCPointMapIter->second->GetPointType();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogControlODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = ShiftLeft8Result16Bits(module) | channel;

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
/*
// Dumps the points out in a list, only used for UnitTests
std::vector<CBBinaryPoint> CBPointTableAccess::DumpTimeTaggedPointList()
{
      CBBinaryPoint CurrentPoint;
      std::vector<CBBinaryPoint> PointList(50);

      while (pBinaryTimeTaggedEventQueue->sync_front(CurrentPoint))
      {
            PointList.emplace_back(CurrentPoint);
            pBinaryTimeTaggedEventQueue->sync_pop();
      }

      return PointList;
}
bool CBPointTableAccess::PeekNextTaggedEventPoint(CBBinaryPoint &pt)
{
      return pBinaryTimeTaggedEventQueue->sync_front(pt);
}
bool CBPointTableAccess::PopNextTaggedEventPoint()
{
      return pBinaryTimeTaggedEventQueue->sync_pop();
}
bool CBPointTableAccess::TimeTaggedDataAvailable()
{
      return !pBinaryTimeTaggedEventQueue->sync_empty();
}
*/
void CBPointTableAccess::ForEachBinaryPoint(std::function<void(CBBinaryPoint &pt)> fn)
{
	for (auto CBpt : BinaryCBPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachAnalogPoint(std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	for (auto CBpt : AnalogCBPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachCounterPoint(std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	for (auto CBpt : CounterCBPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
#pragma endregion
