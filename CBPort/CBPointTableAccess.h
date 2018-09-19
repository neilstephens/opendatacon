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

#include <unordered_map>
#include <vector>
#include <functional>


#include "CB.h"
#include "CBPointConf.h"
//#include "CBPortConf.h"
#include "CBUtility.h"
//#include "StrandProtectedQueue.h"

using namespace odc;

// Collect the access routines into the point table here.
class CBPointTableAccess
{
public:
	CBPointTableAccess();
	void Build(const bool isoutstation, asio::io_service & IOS);

	// The add to point table functions add to both the ODC and MD3 Map.
	// The Conitel Baker methods require that a
	bool AddBinaryPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const BinaryPointType & pointtype, const PayloadLocationType &payloadlocation);
	bool AddBinaryControlPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const BinaryPointType & pointtype, const PayloadLocationType &payloadlocation);

	bool AddCounterPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation);
	bool AddAnalogPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation);
	bool AddAnalogControlPointToPointTable(const size_t & index, const uint8_t & group, const uint8_t & channel, const PayloadLocationType &payloadlocation);

	bool GetCounterValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t & res, bool &hasbeenset);
	bool GetCounterValueAndChangeUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool &hasbeenset);
	bool SetCounterValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetCounterODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t & res);
	bool SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool GetAnalogValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t & res, bool & hasbeenset);
	bool GetAnalogValueAndChangeUsingCBIndex(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool & hasbeenset);
	bool SetAnalogValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetAnalogValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);
	bool SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool ResetAnalogValueUsingODCIndex(const size_t index);
	bool ResetCounterValueUsingODCIndex(const size_t index);

	bool GetAnalogODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &res);
	bool GetBinaryODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t &res);

	//TODO: Return actual quality, not hasbeenset
	bool GetBinaryQualityUsingCBIndex(const uint16_t module, const uint8_t channel, bool & hasbeenset);

	bool GetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed);
	bool GetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, uint8_t & res);
	bool SetBinaryValueUsingCBIndex(const uint16_t module, const uint8_t channel, const uint8_t meas, bool & valuechanged);
	bool GetBinaryChangedUsingCBIndex(const uint16_t module, const uint8_t channel, bool &changed);

	bool GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed);
	bool SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, CBTime eventtime);

	bool GetBinaryControlODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t & index);
	bool GetBinaryControlCBIndexUsingODCIndex(const size_t index, uint8_t & module, uint8_t & channel, BinaryPointType & pointtype);

	bool GetAnalogControlODCIndexUsingCBIndex(const uint16_t module, const uint8_t channel, size_t & index);

	void ForEachBinaryPoint(std::function<void(CBBinaryPoint &pt)>);
	void ForEachAnalogPoint(std::function<void(CBAnalogCounterPoint&pt)> fn);
	void ForEachCounterPoint(std::function<void(CBAnalogCounterPoint&pt)> fn);

//	std::vector<CBBinaryPoint> DumpTimeTaggedPointList();

//	bool TimeTaggedDataAvailable();
//	bool PeekNextTaggedEventPoint(CBBinaryPoint & pt);
//	bool PopNextTaggedEventPoint();

	void AddToDigitalEvents(CBBinaryPoint & inpt);
	uint16_t CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t Group, bool & ModuleFailed);
	uint16_t CollectModuleBitsIntoWord(const uint8_t Group, bool & ModuleFailed);

protected:

	// We access the map using a Module:Channel combination, so that they will always be in order. Makes searching the next item easier.
	std::map<uint16_t, std::shared_ptr<CBBinaryPoint>> BinaryCBPointMap; // ModuleAndChannel, CBPoint
	std::map<size_t, std::shared_ptr<CBBinaryPoint>> BinaryODCPointMap;  // Index OpenDataCon, CBPoint

	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogCBPointMap; // ModuleAndChannel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogODCPointMap;  // Index OpenDataCon, CBPoint

	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> CounterCBPointMap; // ModuleAndChannel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> CounterODCPointMap;  // Index OpenDataCon, CBPoint

	// Binary Control Points are not readable
	std::map<uint16_t, std::shared_ptr<CBBinaryPoint>> BinaryControlCBPointMap; // ModuleAndChannel, CBPoint
	std::map<size_t, std::shared_ptr<CBBinaryPoint>> BinaryControlODCPointMap;  // Index OpenDataCon, CBPoint

	// Analog Control Points are not readable
	std::map<uint16_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogControlCBPointMap; // ModuleAndChannel, CBPoint
	std::map<size_t, std::shared_ptr<CBAnalogCounterPoint>> AnalogControlODCPointMap;  // Index OpenDataCon, CBPoint

	bool IsOutstation = true;

	// Only used in outstation
//	std::shared_ptr<StrandProtectedQueue<CBBinaryPoint>> pBinaryTimeTaggedEventQueue; // Separate queue for time tagged binary events.
};

#endif
