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
* CBPointTableAccess.h
*
*  Created on: 14/09/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef CBPOINTTABLEACCESS_H_
#define CBPOINTTABLEACCESS_H_
#include "CB.h"
#include "CBUtility.h"
#include "StrandProtectedQueue.h"
#include <unordered_map>
#include <vector>
#include <functional>

using namespace odc;

// Collect the access routines into the point table here.
class CBPointTableAccess
{
public:
	CBPointTableAccess();
	void SetName(std::string _Name) { Name = _Name; };
	void Build(const std::string& _Name, bool isoutstation, odc::asio_service & IOS, unsigned int SOEQueueSize, std::shared_ptr<protected_bool> SOEBufferOverflowFlag);

	// The add to point table functions add to both the ODC and MD3 Map.
	// The Conitel Baker methods require that a
	bool AddBinaryControlPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel,  const BinaryPointType & pointtype );
	bool AddCounterPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation, const AnalogCounterPointType &pointtype);
	bool AddAnalogPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation, const AnalogCounterPointType &pointtype);
	bool CheckForBinaryBitClash(const uint8_t group, uint8_t channel, const PayloadLocationType& payloadlocation, const BinaryPointType& pointtype);
	bool AddBinaryPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType & payloadlocation, const BinaryPointType & pointtype, bool issoe, uint8_t soeindex);
	bool AddAnalogControlPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const AnalogCounterPointType &pointtype);

	bool AddStatusByteToCBMap(const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation);
	void UpdateMaxPayload(const uint8_t & group, const PayloadLocationType & payloadlocation);

	bool GetCounterValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);
	bool SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool GetAnalogValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);
	bool SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool ResetAnalogValueUsingODCIndex(const size_t index);
	bool ResetCounterValueUsingODCIndex(const size_t index);

	bool GetBinaryValueUsingODCIndexAndResetChangedFlag(const size_t index, uint8_t &res, bool &changed, bool &hasbeenset);
	bool SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, CBTime eventtime);

	bool GetBinaryODCIndexUsingSOE( const uint8_t number, size_t & index);

	void AddToDigitalEvents(const CBBinaryPoint & inpt, const uint8_t meas, const CBTime eventtime);
	bool PeekNextTaggedEventPoint(CBBinaryPoint &pt);
	bool PopNextTaggedEventPoint();
	bool TimeTaggedDataAvailable();
	std::vector<CBBinaryPoint> DumpTimeTaggedPointList();

	bool GetBinaryControlODCIndexUsingCBIndex(const uint8_t group, const uint8_t channel, size_t & index);
	bool GetBinaryControlCBIndexUsingODCIndex(const size_t index, uint8_t &group, uint8_t & channel);
	bool GetBinaryControlValueUsingODCIndex(const size_t index, uint8_t &res, bool &hasbeenset);
	bool SetBinaryControlValueUsingODCIndex(const size_t index, const uint8_t meas, const CBTime eventtime);

	bool GetAnalogControlODCIndexUsingCBIndex(const uint8_t group, const uint8_t channel, size_t & index);
	bool GetAnalogControlCBIndexUsingODCIndex(const size_t index, uint8_t & group, uint8_t & channel);
	bool GetAnalogControlValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);

	bool SetAnalogControlValueUsingODCIndex(const size_t index, const uint16_t meas, CBTime eventtime);

	void ForEachBinaryPoint(const std::function<void(CBBinaryPoint &pt)>&);
	void ForEachAnalogPoint(const std::function<void(CBAnalogCounterPoint&pt)>& fn);
	void ForEachCounterPoint(const std::function<void(CBAnalogCounterPoint&pt)>& fn);

	void ForEachMatchingBinaryPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBBinaryPoint&pt)>& fn);
	void ForEachMatchingAnalogPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBAnalogCounterPoint&pt)>& fn);
	void ForEachMatchingCounterPoint(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(CBAnalogCounterPoint&pt)>& fn);
	void ForEachMatchingStatusByte(const uint8_t & group, const PayloadLocationType & payloadlocation, const std::function<void(void)>& fn);

	bool GetMaxPayload(uint8_t group, uint8_t &blockcount);

	// Public only for testing
	static uint16_t GetCBPointMapIndex(const uint8_t &group, const uint8_t &channel, const PayloadLocationType &payloadlocation); // Group/Payload/Channel
	static uint16_t GetCBBitMapIndex(const uint8_t& group, uint8_t bit, const PayloadLocationType& payloadlocation, const BinaryPointType& pointtype);
	static uint16_t GetCBControlPointMapIndex(const uint8_t & group, const uint8_t & channel);
protected:

	// We access the map using a Module:Channel combination, so that they will always be in order. Makes searching the next item easier.
	std::map<uint16_t, std::shared_ptr<CBBinaryPoint>> BinaryCBPointMap;  // Group/Payload/Channel, CBPoin
	std::map<uint16_t, bool> BinaryCBBitMap;                              // Group/Payload/Bitl, bool
	std::map<uint16_t, std::shared_ptr<CBBinaryPoint>> BinarySOEPointMap; // Group/SoeIndexl, CBPoint
	std::map<size_t, std::shared_ptr<CBBinaryPoint>> BinaryODCPointMap;   // Index OpenDataCon, CBPoint

	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogCBPointMap; // Group/Payload/Channel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogODCPointMap;  // Index OpenDataCon, CBPoint

	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> CounterCBPointMap; // Group/Payload/Channel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> CounterODCPointMap;  // Index OpenDataCon, CBPoint

	// Binary Control Points are not readable
	std::map<uint16_t, std::shared_ptr<CBBinaryPoint>> BinaryControlCBPointMap; //Group/Payload/Channel, CBPoint
	std::map<size_t, std::shared_ptr<CBBinaryPoint>> BinaryControlODCPointMap;  // Index OpenDataCon, CBPoint

	// Analog Control Points are not readable
	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogControlCBPointMap; // Group/Payload/Channel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogControlODCPointMap;  // Index OpenDataCon, CBPoint

	std::map<uint16_t, uint8_t> StatusByteMap; // Group/Payload/Channel, second parameter empty.

	std::map<uint8_t, uint8_t> MaxiumPayloadPerGroupMap; // Group 0 to 15, Max payload - a count of 1 to 16.

	bool IsOutstation = true;
	std::string Name;
	std::shared_ptr<StrandProtectedQueue<CBBinaryPoint>> pBinaryTimeTaggedEventQueue; // Separate queue for time tagged binary events.
	// Define the special SOE buffer overflow point, so that it can be added to the SOE queue if the buffer overflows. The only thing that gets changed is the time.
	CBBinaryPoint queuefullpt = CBBinaryPoint(0, 0, 1, PayloadLocationType(), BinaryPointType::DIG, true, 127);
};
#endif
