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

void CBPointTableAccess::Build(const bool isoutstation)
{
	IsOutstation = isoutstation;
//	pBinaryTimeTaggedEventQueue.reset(new StrandProtectedQueue<CBBinaryPoint>(IOS, 256));
}

#ifdef _MSC_VER
#pragma region Add Read Points
#endif

bool CBPointTableAccess::AddCounterPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation, const AnalogCounterPointType &pointtype)
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
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, payloadlocation,pointtype);
		CounterCBPointMap[CBIndex] = pt;
		CounterODCPointMap[index] = pt;
	}
	return true;
}

bool CBPointTableAccess::AddAnalogPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation, const AnalogCounterPointType &pointtype)
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
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, payloadlocation, pointtype);
		AnalogCBPointMap[CBIndex] = pt;
		AnalogODCPointMap[index] = pt;
	}
	return true;
}

bool CBPointTableAccess::AddBinaryPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation, const BinaryPointType &pointtype)
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
		LOGDEBUG("Adding Binary Point at CBIndex - " + to_hexstring(CBIndex));
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
#ifdef _MSC_VER
#pragma endregion

#pragma region Add Control Points
#endif
bool CBPointTableAccess::AddAnalogControlPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const AnalogCounterPointType &pointtype)
{
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);
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
		auto pt = std::make_shared<CBAnalogCounterPoint>(index, group, channel, PayloadLocationType(1, PayloadABType::PositionB), pointtype);
		AnalogControlCBPointMap[CBIndex] = pt;
		AnalogControlODCPointMap[index] = pt;
	}
	return true;
}
bool CBPointTableAccess::AddBinaryControlPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const BinaryPointType &pointtype)
{
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);

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

	auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, PayloadLocationType(1,PayloadABType::PositionB), pointtype);
	BinaryControlCBPointMap[CBIndex] = pt;
	BinaryControlODCPointMap[index] = pt;
	return true;
}
#ifdef _MSC_VER
#pragma endregion
#endif

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

bool CBPointTableAccess::GetAnalogValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalogAndHasBeenSet( hasbeenset);
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

#ifdef _MSC_VER
#pragma region Binary
#endif

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

		return true;
	}
	return false;
}
#ifdef _MSC_VER
#pragma endregion

#pragma region Control
#endif

bool CBPointTableAccess::GetBinaryControlODCIndexUsingCBIndex(const uint8_t group, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);

	CBBinaryPointMapIterType CBPointMapIter = BinaryControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryControlCBIndexUsingODCIndex(const size_t index, uint8_t &group, uint8_t &channel)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != BinaryControlODCPointMap.end())
	{
		group = ODCPointMapIter->second->GetGroup();
		channel = ODCPointMapIter->second->GetChannel();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryControlValueUsingODCIndex(const size_t index, uint8_t &res, bool &hasbeenset)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != BinaryControlODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetBinaryControlValueUsingODCIndex(const size_t index, const uint8_t meas, CBTime eventtime)
{
	ODCBinaryPointMapIterType ODCPointMapIter = BinaryControlODCPointMap.find(index);
	if (ODCPointMapIter != BinaryControlODCPointMap.end())
	{
		ODCPointMapIter->second->SetBinary(meas, eventtime);
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogControlODCIndexUsingCBIndex(const uint8_t group, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);

	CBAnalogCounterPointMapIterType CBPointMapIter = AnalogControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogControlCBIndexUsingODCIndex(const size_t index, uint8_t &group, uint8_t &channel)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogControlODCPointMap.find(index);
	if (ODCPointMapIter != AnalogControlODCPointMap.end())
	{
		group = ODCPointMapIter->second->GetGroup();
		channel = ODCPointMapIter->second->GetChannel();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogControlValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogControlODCPointMap.find(index);
	if (ODCPointMapIter != AnalogControlODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalogAndHasBeenSet(hasbeenset);
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetAnalogControlValueUsingODCIndex(const size_t index, const uint16_t meas, CBTime eventtime)
{
	ODCAnalogCounterPointMapIterType ODCPointMapIter = AnalogControlODCPointMap.find(index);
	if (ODCPointMapIter != AnalogControlODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, eventtime);
		return true;
	}
	return false;
}

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
uint16_t CBPointTableAccess::GetCBControlPointMapIndex(const uint8_t & group, const uint8_t & channel)
{
	assert(group <= 0x0F);
	assert(channel != 0);
	assert(channel <= 12);

	// Top 4 bits group (0-15), Next 4 bits channel (1-12)
	uint16_t CBIndex = ShiftLeftResult16Bits(group, 12) | ShiftLeftResult16Bits(channel, 8);
	return CBIndex;
}
void CBPointTableAccess::ForEachMatchingBinaryPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, std::function<void(CBBinaryPoint &pt)> fn)
{
	// Need to do a search for channels 1 to 12 in the group and payloadlocation
	for (uint8_t channel = 1; channel <= 12; channel++)
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
	// Max of two channels for analogs
	for (uint8_t channel = 1; channel <= 2; channel++)
	{
		uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
		if (AnalogCBPointMap.count(CBIndex) != 0)
		{
			// We have a match - call our function with the point as a parameter.
			fn(*AnalogCBPointMap[CBIndex]);
		}
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
#ifdef _MSC_VER
#pragma endregion
#endif
