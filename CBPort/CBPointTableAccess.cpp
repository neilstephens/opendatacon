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


#include "CBPointTableAccess.h"
#include "CBPortConf.h"

CBPointTableAccess::CBPointTableAccess()
{}

void CBPointTableAccess::Build(const std::string& _Name, const bool isoutstation, odc::asio_service &IOS, unsigned int SOEQueueSize, std::shared_ptr<protected_bool> SOEBufferOverflowFlag)
{
	Name = _Name;
	IsOutstation = isoutstation;
	// Setup TimeTagged event queue.
	// The size (default 500) does not consume memory, just sets an upper limit to the number of items in the queue.
	pBinaryTimeTaggedEventQueue = std::make_shared<StrandProtectedQueue<CBBinaryPoint>>(IOS, SOEQueueSize, SOEBufferOverflowFlag);
}

#ifdef _MSC_VER
#pragma region Add Read Points
#endif

bool CBPointTableAccess::AddCounterPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation, const AnalogCounterPointType &pointtype)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (CounterCBPointMap.find(CBIndex) != CounterCBPointMap.end())
	{
		LOGERROR("{} Error Duplicate Counter CB Index {} - {} - {}",Name,group,channel, payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);

	if (CounterODCPointMap.find(index) != CounterODCPointMap.end())
	{
		LOGWARN("{} Warning Duplicate Counter ODC Index : {}",Name,index);
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
		LOGERROR("{} Error Duplicate Analog CB Index {} - {} - {}",Name,group,channel,payloadlocation.to_string());
		return false;
	}

	UpdateMaxPayload(group, payloadlocation);

	if (AnalogODCPointMap.find(index) != AnalogODCPointMap.end())
	{
		LOGWARN("{} Warning Duplicate Analog ODC Index : {}",Name,index);
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

bool CBPointTableAccess::CheckForBinaryBitClash(const uint8_t group, uint8_t channel, const PayloadLocationType& payloadlocation, const BinaryPointType& pointtype)
{
	uint8_t bit = channel;
	if (pointtype == MCB || pointtype == MCA || pointtype == MCC)
	{
		// Dual bit point...Chan 1 goes to 1, 2 to 3 etc
		bit = (channel-1) * 2 + 1;
	}
	uint16_t CBBitIndex = GetCBBitMapIndex(group,bit, payloadlocation, pointtype);
	if (BinaryCBBitMap.find(CBBitIndex) != BinaryCBBitMap.end())
	{
		LOGERROR("{} Error Duplicate Binary CB Bit  {} - {} - {} - {}", Name, group,bit, payloadlocation.to_string(), pointtype);
		return false;
	}
	BinaryCBBitMap[CBBitIndex] = true;

	if (pointtype == MCB || pointtype == MCA || pointtype == MCC)
	{
		// Dual bit point...
		bit++; // Process 2nd channel
		CBBitIndex = GetCBBitMapIndex(group, bit, payloadlocation, pointtype);
		if (BinaryCBBitMap.find(CBBitIndex) != BinaryCBBitMap.end())
		{
			LOGERROR("{} Error Duplicate Binary CB Bit  {} - {} - {} - {}", Name, group, bit, payloadlocation.to_string(), pointtype);
			return false;
		}
		BinaryCBBitMap[CBBitIndex] = true;
	}

	return true;
}
bool CBPointTableAccess::AddBinaryPointToPointTable(const size_t &index, const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation, const BinaryPointType &pointtype, bool issoe, uint8_t soeindex)
{
	// So we need to access the Conitel data by Group/PayLoadLocation/Channel
	// When we get a scan, we need to get everything for a given group, payload by payload 1B,2A,2B and then for binary all matching channels.
	// For that scan, we will need to query the Binaries/Analog/Counters and the Status field.
	// There 'could' be gaps in the payload - we should handle this gracefully and just fill with zero.
	// For Binaries there will only be a maximum of 12 channels(bits)
	// The channels might have gaps, dont need to be in order.

	//TODO: We dont allow double bit points and single bit DIG's to occupy the same payload - it is hard to detect collisions if we do...
	// how to check for this???

	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);

	if (BinaryCBPointMap.find(CBIndex) != BinaryCBPointMap.end())
	{
		LOGERROR("{} Error Duplicate Binary CB Index {} - {} - {}",Name,group,channel,payloadlocation.to_string());
		return false;
	}

	if (!CheckForBinaryBitClash(group, channel, payloadlocation, pointtype))
		return false;

	UpdateMaxPayload(group, payloadlocation);

	if (BinaryODCPointMap.find(index) != BinaryODCPointMap.end())
	{
		LOGWARN("{} Warning Duplicate Binary ODC Index : {}",Name,index);
		BinaryCBPointMap[CBIndex] = BinaryODCPointMap[index]; //TODO: This will cause problems as the point has a group variable that will not be correct...do we need a group list?
	}
	else
	{
		LOGDEBUG("{} Adding Binary Point at CBIndex - {}",Name,to_hexstring(CBIndex));
		auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, payloadlocation, pointtype, issoe, soeindex);
		BinaryCBPointMap[CBIndex] = pt;
		BinaryODCPointMap[index] = pt;

		if (issoe)
		{
			assert(soeindex <= 120);
			BinarySOEPointMap[soeindex] = pt;
		}
	}
	return true;
}

bool CBPointTableAccess::AddStatusByteToCBMap(const uint8_t & group, const uint8_t & channel, const PayloadLocationType & payloadlocation)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, channel, payloadlocation);
	if (StatusByteMap.find(CBIndex) != StatusByteMap.end())
	{
		LOGERROR("{} Error Duplicate Status Byte CB Index {} - {} - {}",Name,group,channel, payloadlocation.to_string());
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
	// TODO: We do not detect collisions that could be caused by mixing analog types in the one payload (any mix 12 bit and 6 bit)
	// will be a problem in any situaton at they will not fit..
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);
	if (AnalogControlCBPointMap.find(CBIndex) != AnalogControlCBPointMap.end())
	{
		LOGERROR("{} Error Duplicate Analog CB Index {} - {}",Name,group,channel);
		return false;
	}

	if (AnalogControlODCPointMap.find(index) != AnalogControlODCPointMap.end())
	{
		LOGERROR("{} Duplicate Analog ODC Index : {}",Name,index);
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
		LOGERROR("{} Duplicate BinaryControl CB Index {} - {}",Name,group,channel);
		return false;
	}

	if (BinaryControlODCPointMap.find(index) != BinaryControlODCPointMap.end())
	{
		LOGERROR("{} Duplicate BinaryControl ODC Index : {}",Name,index);
		return false;
	}

	auto pt = std::make_shared<CBBinaryPoint>(index, group, channel, PayloadLocationType(1,PayloadABType::PositionB), pointtype, false,0);
	BinaryControlCBPointMap[CBIndex] = pt;
	BinaryControlODCPointMap[index] = pt;
	return true;
}
#ifdef _MSC_VER
#pragma endregion
#endif

bool CBPointTableAccess::GetCounterValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	auto ODCPointMapIter = CounterODCPointMap.find(index);
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
	auto ODCPointMapIter = CounterODCPointMap.find(index);
	if (ODCPointMapIter != CounterODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, CBNowUTC());
		return true;
	}
	return false;
}
bool CBPointTableAccess::ResetCounterValueUsingODCIndex(const size_t index)
{
	auto ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->ResetAnalog(); // Sets to MISSINGVALUE, time = 0, HasBeenSet to false
		return true;
	}
	return false;
}

bool CBPointTableAccess::GetAnalogValueUsingODCIndex(const size_t index, uint16_t &res, bool &hasbeenset)
{
	auto ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalogAndHasBeenSet( hasbeenset);
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas)
{
	auto ODCPointMapIter = AnalogODCPointMap.find(index);
	if (ODCPointMapIter != AnalogODCPointMap.end())
	{
		ODCPointMapIter->second->SetAnalog(meas, CBNowUTC());
		return true;
	}
	return false;
}
bool CBPointTableAccess::ResetAnalogValueUsingODCIndex(const size_t index)
{
	auto ODCPointMapIter = AnalogODCPointMap.find(index);
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
	auto ODCPointMapIter = BinaryODCPointMap.find(index);
	if (ODCPointMapIter != BinaryODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetBinary();
		changed = ODCPointMapIter->second->GetAndResetChangedFlag();
		hasbeenset = ODCPointMapIter->second->GetHasBeenSet();
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, const CBTime eventtime)
{
	auto ODCPointMapIter = BinaryODCPointMap.find(index);
	if (ODCPointMapIter != BinaryODCPointMap.end())
	{
		// Store SOE event to the SOE queue. If we receive a binary event that is older than the last one, it only goes in the SOE queue.
		if (ODCPointMapIter->second->GetIsSOE() && IsOutstation)
			AddToDigitalEvents(*ODCPointMapIter->second, meas, eventtime); // Don't store if master - we just fire off ODC events.

		// Now check that the data we want to set is actually newer than (or equal to) the current point information. i.e. dont go backwards!
		if (eventtime > ODCPointMapIter->second->GetChangedTime())
		{
			ODCPointMapIter->second->SetBinary(meas, eventtime);
		}
		//	else
		//		LOGDEBUG("Received a SetBinaryValue command that is older than the current binary data {}, {}", ODCPointMapIter->second->GetChangedTime(), eventtime);
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryODCIndexUsingSOE( const uint8_t soeindex, size_t &index)
{
	if (soeindex > 120) return false;

	auto SOEPointMapIter = BinarySOEPointMap.find(soeindex);
	if (SOEPointMapIter != BinarySOEPointMap.end())
	{
		index = SOEPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
void CBPointTableAccess::AddToDigitalEvents(const CBBinaryPoint &inpt, const uint8_t meas, const CBTime eventtime)
{
	if (inpt.GetIsSOE()) // Only add if the point is SOE - have already checked this once!
	{
		// The push will fail by dumping the point if full,
		// We need to copy the point and set the value and time, as the current "point" may be a newer one and we dont want to modify it.
		CBBinaryPoint pt = inpt;
		pt.SetBinary(meas, eventtime);

		// Set the queue point value, just so we can set the time. The value does not matter.
		queuefullpt.SetBinary(1, CBNowUTC());

		pBinaryTimeTaggedEventQueue->async_push(pt, queuefullpt, odc::spdlog_get("CBPort"));
		LOGDEBUG("{} Outstation Added Binary Event to SOE Queue - ODCIndex {}, Value {}",Name, pt.GetIndex(),pt.GetBinary());
	}
}
bool CBPointTableAccess::PeekNextTaggedEventPoint(CBBinaryPoint &pt)
{
	return pBinaryTimeTaggedEventQueue->sync_front(pt);
}
bool CBPointTableAccess::PopNextTaggedEventPoint( )
{
	return pBinaryTimeTaggedEventQueue->sync_pop();
}
bool CBPointTableAccess::TimeTaggedDataAvailable()
{
	return !pBinaryTimeTaggedEventQueue->sync_empty();
}
// Dumps the points out in a list, only used for UnitTests
std::vector<CBBinaryPoint> CBPointTableAccess::DumpTimeTaggedPointList( )
{
	CBBinaryPoint CurrentPoint;
	std::vector<CBBinaryPoint> PointList;
	PointList.reserve(50);

	while (pBinaryTimeTaggedEventQueue->sync_front(CurrentPoint))
	{
		PointList.emplace_back(CurrentPoint);
		pBinaryTimeTaggedEventQueue->sync_pop();
	}

	return PointList;
}
#ifdef _MSC_VER
#pragma endregion

#pragma region Control
#endif

bool CBPointTableAccess::GetBinaryControlODCIndexUsingCBIndex(const uint8_t group, const uint8_t channel, size_t &index)
{
	uint16_t CBIndex = GetCBControlPointMapIndex(group, channel);

	auto CBPointMapIter = BinaryControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != BinaryControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetBinaryControlCBIndexUsingODCIndex(const size_t index, uint8_t &group, uint8_t &channel)
{
	auto ODCPointMapIter = BinaryControlODCPointMap.find(index);
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
	auto ODCPointMapIter = BinaryControlODCPointMap.find(index);
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
	auto ODCPointMapIter = BinaryControlODCPointMap.find(index);
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

	auto CBPointMapIter = AnalogControlCBPointMap.find(CBIndex);
	if (CBPointMapIter != AnalogControlCBPointMap.end())
	{
		index = CBPointMapIter->second->GetIndex();
		return true;
	}
	return false;
}
bool CBPointTableAccess::GetAnalogControlCBIndexUsingODCIndex(const size_t index, uint8_t &group, uint8_t &channel)
{
	auto ODCPointMapIter = AnalogControlODCPointMap.find(index);
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
	auto ODCPointMapIter = AnalogControlODCPointMap.find(index);
	if (ODCPointMapIter != AnalogControlODCPointMap.end())
	{
		res = ODCPointMapIter->second->GetAnalogAndHasBeenSet(hasbeenset);
		return true;
	}
	return false;
}
bool CBPointTableAccess::SetAnalogControlValueUsingODCIndex(const size_t index, const uint16_t meas, CBTime eventtime)
{
	auto ODCPointMapIter = AnalogControlODCPointMap.find(index);
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
uint16_t CBPointTableAccess::GetCBBitMapIndex(const uint8_t& group, uint8_t bit, const PayloadLocationType& payloadlocation, const BinaryPointType& pointtype)
{
	// Top 4 bits group (0-15), Next 4 bits are bit index (1-12), next 4 bits payload packet number (0-15), next 4 bits 0(A) or 1(B)
	uint16_t CBBit = ShiftLeftResult16Bits(group, 12) | ShiftLeftResult16Bits(bit, 8) | ShiftLeftResult16Bits(payloadlocation.Packet - 1, 4) | (payloadlocation.Position == PayloadABType::PositionA ? 0 : 1);
	return CBBit;
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
void CBPointTableAccess::ForEachMatchingBinaryPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBBinaryPoint &pt)>& fn)
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

void CBPointTableAccess::ForEachMatchingAnalogPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBAnalogCounterPoint &pt)>& fn)
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
void CBPointTableAccess::ForEachMatchingCounterPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBAnalogCounterPoint &pt)>& fn)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, 1, payloadlocation);
	if (CounterCBPointMap.count(CBIndex) != 0)
	{
		// We have a match - call our function with the point as a parameter.
		fn(*CounterCBPointMap[CBIndex]);
	}
}
void CBPointTableAccess::ForEachMatchingStatusByte(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(void)>& fn)
{
	uint16_t CBIndex = GetCBPointMapIndex(group, 1, payloadlocation); // Use the same index as the points
	if (StatusByteMap.count(CBIndex) != 0)
	{
		// We have a match - call our function
		fn();
	}
}

bool CBPointTableAccess::GetMaxPayload(uint8_t group, uint8_t & blockcount)
{
	if (MaxiumPayloadPerGroupMap.count(group) == 0)
	{
		return false;
	}
	blockcount = MaxiumPayloadPerGroupMap[group];
	return true;
}

void CBPointTableAccess::ForEachBinaryPoint(const std::function<void(CBBinaryPoint &pt)>& fn)
{
	for (const auto& CBpt : BinaryODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachAnalogPoint(const std::function<void(CBAnalogCounterPoint &pt)>& fn)
{
	for (const auto& CBpt : AnalogODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
void CBPointTableAccess::ForEachCounterPoint(const std::function<void(CBAnalogCounterPoint &pt)>& fn)
{
	for (const auto& CBpt : CounterODCPointMap) // We always use the CB map - its order is the only one we care about.
	{
		fn(*CBpt.second);
	}
}
#ifdef _MSC_VER
#pragma endregion
#endif
