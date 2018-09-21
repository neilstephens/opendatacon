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

#pragma region Add Read Points

bool CBPointTableAccess::AddCounterPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (CounterCBPointMap.find(CBIndex) != CounterCBPointMap.end())
	{
		LOGERROR("Error Duplicate Counter CB Index " + std::to_string(group) + " - " + std::to_string(channel) + " - " + payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);

	if (CounterODCPointMap.find(index) != CounterODCPointMap.end())
	{
		LOGWARN("Warning Duplicate Counter ODC Index : " + std::to_string(index));
		//Find the point and add to the CB table again
		CounterCBPointMap[CBIndex] = CounterODCPointMap[index]; //TODO: This will cause problems as the point has a group variable that will not be correct...do we need a group list?
	}
	else
	{
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, payloadlocation);
		CounterCBPointMap[CBIndex] = pt;
		CounterODCPointMap[index] = pt;
	}
	return true;
}

bool CBPointTableAccess::AddAnalogPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (AnalogCBPointMap.find(CBIndex) != AnalogCBPointMap.end())
	{
		LOGERROR("Error Duplicate Analog CB Index " + std::to_string(group) + " - " + std::to_string(channel) + " - " + payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);

	if (AnalogODCPointMap.find(index) != AnalogODCPointMap.end())
	{
		LOGWARN("Warning Duplicate Analog ODC Index : " + std::to_string(index));
		AnalogCBPointMap[CBIndex] = AnalogODCPointMap[index]; //TODO: This will cause problems as the point has a group variable that will not be correct...do we need a group list?
	}
	else
	{
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, payloadlocation);
		AnalogCBPointMap[CBIndex] = pt;
		AnalogODCPointMap[index] = pt;
	}
	return true;
}

bool CBPointTableAccess::AddBinaryPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const BinaryPointType &pointtype, const PayloadLocationType &payloadlocation)
{
	// So we need to access the Conitel data by Group/PayLoadLocation/Channel
	// When we get a scan, we need to get everything for a given group, payload by payload 1B,2A,2B and then for binary all matching channels.
	// For that scan, we will need to query the Binaries/Analog/Counters and the Status field.
	// There 'could' be gaps in the payload - we should handle this gracefully and just fill with zero.
	// For Binaries there will only be a maximum of 12 channels(bits)
	// The channels might have gaps, dont need to be in order.

	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);

	if (BinaryCBPointMap.find(CBIndex) != BinaryCBPointMap.end())
	{
		LOGERROR("Error Duplicate Binary CB Index " + std::to_string(group) + " - " + std::to_string(channel) + " - " + payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);

	if (BinaryODCPointMap.find(index) != BinaryODCPointMap.end())
	{
		LOGWARN("Warning Duplicate Binary ODC Index : " + std::to_string(index));
		BinaryCBPointMap[CBIndex] = BinaryODCPointMap[index]; //TODO: This will cause problems as the point has a group variable that will not be correct...do we need a group list?
	}
	else
	{
		LOGDEBUG("Adding Binary Point at " + to_hexstring(CBIndex));
		auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, payloadlocation, pointtype);
		BinaryCBPointMap[CBIndex] = pt;
		BinaryODCPointMap[index] = pt;
	}
	return true;
}

bool CBPointTableAccess::AddStatusByteToCBMap(const uint8_t & group, const uint8_t & channel, const PayloadLocationType & payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (StatusByteMap.find(CBIndex) != StatusByteMap.end())
	{
		LOGERROR("Error Duplicate Status Byte CB Index " + std::to_string(group) + " - " + std::to_string(channel) + " - " + payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);
	StatusByteMap[CBIndex] = 0; // Creates the map entry
	return true;
}

void CBPointTableAccess::UpdateMaxPayload(const uint8_t & group, const PayloadLocationType & payloadlocation)
{
	// Keep track of the max payload so we know when we have processed them all.
	if (MaxiumPayloadPerGroupMap.count(group) == 0)
	{
		MaxiumPayloadPerGroupMap[group] = payloadlocation.Packet;
	}
	else if (MaxiumPayloadPerGroupMap[group] < payloadlocation.Packet)
	{
		MaxiumPayloadPerGroupMap[group] = payloadlocation.Packet;
	}
}

#pragma endregion

#pragma region Add Control Points

bool CBPointTableAccess::AddAnalogControlPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (AnalogControlCBPointMap.find(CBIndex) != AnalogControlCBPointMap.end())
	{
		LOGERROR("Error Duplicate Analog CB Index " + std::to_string(group) + " - " + std::to_string(channel));
		return false;
	}

	if (AnalogControlODCPointMap.find(index) != AnalogControlODCPointMap.end())
	{
		LOGERROR("Duplicate Analog ODC Index : " + std::to_string(index));
		AnalogControlCBPointMap[CBIndex] = AnalogControlODCPointMap[index]; //TODO: This will cause problems as the point has a group variable that will not be correct...do we need a group list?
	}
	else
	{
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, payloadlocation);
		AnalogControlCBPointMap[CBIndex] = pt;
		AnalogControlODCPointMap[index] = pt;
	}
	return true;
}
bool CBPointTableAccess::AddBinaryControlPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const BinaryPointType &pointtype, const PayloadLocationType &payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);

	if (BinaryControlCBPointMap.find(CBIndex) != BinaryControlCBPointMap.end())
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
	BinaryControlCBPointMap[CBIndex] = pt;
	BinaryControlODCPointMap[index] = pt;
	return true;
}

#pragma endregion

bool CBPointTableAccess::GetCounterValueUsingCBIndex(const uint16_t group, const uint8_t channel, uint16_t &res, bool &hasbeenset)
{
	uint16_t CBIndex = 0; // GetCBPointMapIndex(group, channel, payloadlocation);

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
bool CBPointTableAccess::GetCounterValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = CounterODCPointMap.find(index);
	if (ODCPointMapIter != CounterODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalog();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
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
		ODCPointMapIter->second->ResetAnalog(); // Sets to MISSINGVALUE, time = 0, HasBeenSet to false
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
		ODCPointMapIter->second->ResetAnalog(); // Sets to MISSINGVALUE, time = 0, HasBeenSet to false
		return true;
	}
	return false;
}


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
bool CBPointTableAccess::GetBinaryValueUsingODCIndexAndResetChangedFlag(const size_t index, uint8_t &res, bool &changed, bool &hasbeenset)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryODCPointMap.find(index);
	if (ODCPointMapIter != BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		changed = ODCPointMapIter->second->GetAndResetChangedFlag();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
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
	if (inpt.GetPointType() == MCA) // Only add if the point not configured for time tagged data
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
uint16_t CBPointTableAccess::GetCBPointMapIndex(const uint8_t & group, const uint8_t & channel, const PayloadLocationType & payloadlocation)
{
	assert(group <= 0x0F);
	assert(channel != 0);
	assert(channel <= 12);
	assert(payloadlocation.Position != PayloadABType::Error);

	// Top 4 bits group (0-15), Next 4 bits channel (1-12), next 4 bits payload packet number (0-15), next 4 bits 0(A) or 1(B)
	uint16_t CBIndex = ShiftLeftResult16Bits(group, 12) | ShiftLeftResult16Bits(channel, 8) | ShiftLeftResult16Bits(payloadlocation.Packet-1, 4) | (payloadlocation.Position == PayloadABType::PositionA ? 0 : 1);
	return CBIndex;
}
void CBPointTableAccess::ForEachMatchingBinaryPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, std::function<void(CBBinaryPoint &pt)> fn)
{
	// Need to do a search for channels 1 to 12 in the group and payloadlocation
	for (uint8_t channel = 1; channel < 13; channel++)
	{
		uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
		if (BinaryCBPointMap.count(CBIndex) != 0)
		{
			// We have a match - call our function with the point as a parameter.
			fn(*BinaryCBPointMap[CBIndex]);
		}
	}
}
void CBPointTableAccess::ForEachMatchingAnalogPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, 1, payloadlocation);
	if (AnalogCBPointMap.count(CBIndex) != 0)
	{
		// We have a match - call our function with the point as a parameter.
		fn(*AnalogCBPointMap[CBIndex]);
	}
}
void CBPointTableAccess::ForEachMatchingCounterPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, 1, payloadlocation);
	if (CounterCBPointMap.count(CBIndex) != 0)
	{
		// We have a match - call our function with the point as a parameter.
		fn(*CounterCBPointMap[CBIndex]);
	}
}
void CBPointTableAccess::ForEachMatchingStatusByte(const uint8_t & group, const PayloadLocationType & payloadlocation, std::function<void(void)> fn)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, 1, payloadlocation); // Use the same index as the points
	if (StatusByteMap.count(CBIndex) != 0)
	{
		// We have a match - call our function
		fn();
	}
}

void CBPointTableAccess::GetMaxPayload(uint8_t group, uint8_t & blockcount)
{
	if (MaxiumPayloadPerGroupMap.count(group) == 0)
	{
		LOGERROR("Tried to get the payload count for a group (" + std::to_string(group) + ") that has no payload");
		return;
	}
	blockcount = MaxiumPayloadPerGroupMap[group];
}

void CBPointTableAccess::ForEachBinaryPoint(std::function<void(CBBinaryPoint &pt)> fn)
{
	for (auto CBpt : BinaryODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachAnalogPoint(std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	for (auto CBpt : AnalogODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachCounterPoint(std::function<void(CBAnalogCounterPoint &pt)> fn)
{
	for (auto CBpt : CounterODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
#pragma endregion
