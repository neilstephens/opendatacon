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
* MD3PointTableAccess.h
*
*  Created on: 14/8/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3POINTTABLEACCESS_H_
#define MD3POINTTABLEACCESS_H_
#include "MD3.h"
#include "MD3PointConf.h"
#include <unordered_map>
#include <vector>
#include <functional>
//#include "MD3PortConf.h"
#include "MD3Utility.h"
#include "StrandProtectedQueue.h"

using namespace odc;

// Collect the access routines into the point table here. Could possibly go into the PointConf class as well...
class MD3PointTableAccess
{
public:
	MD3PointTableAccess();
	void Build(const bool isoutstation, const bool newdigitalcommands, odc::asio_service & IOS);

	bool AddCounterPointToPointTable(const size_t & index, const uint8_t & moduleaddress, const uint8_t & channel, const uint32_t & pollgroup);
	bool AddAnalogPointToPointTable(const size_t & index, const uint8_t & moduleaddress, const uint8_t & channel, const uint32_t & pollgroup);
	bool AddAnalogControlPointToPointTable(const size_t & index, const uint8_t & moduleaddress, const uint8_t & channel, const uint32_t & pollgroup);

	bool AddBinaryPointToPointTable(const size_t & index, const uint8_t & moduleaddress, const uint8_t & channel, const BinaryPointType & pointtype, const uint32_t & pollgroup);
	bool AddBinaryControlPointToPointTable(const size_t & index, const uint8_t & moduleaddress, const uint8_t & channel, const BinaryPointType & pointtype, const uint32_t & pollgroup);

	bool GetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, bool &hasbeenset);
	bool GetCounterValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool &hasbeenset);
	bool SetCounterValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetCounterODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & res);
	bool SetCounterValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, bool & hasbeenset);
	bool GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t & res, int & delta, bool & hasbeenset);
	bool SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetAnalogValueUsingODCIndex(const size_t index, uint16_t & res, bool & hasbeenset);
	bool SetAnalogValueUsingODCIndex(const size_t index, const uint16_t meas);

	bool ResetAnalogValueUsingODCIndex(const size_t index);
	bool ResetCounterValueUsingODCIndex(const size_t index);

	bool GetAnalogODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res);
	bool GetBinaryODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t &res);

	//TODO: Return actual quality, not hasbeenset
	bool GetBinaryQualityUsingMD3Index(const uint16_t module, const uint8_t channel, bool & hasbeenset);

	bool GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed);
	bool GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t & res);
	bool SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas, bool & valuechanged);
	bool GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed);

	bool GetBinaryValueUsingODCIndex(const size_t index, uint8_t &res, bool &changed);
	bool SetBinaryValueUsingODCIndex(const size_t index, const uint8_t meas, MD3Time eventtime);

	bool GetBinaryControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & index);
	bool GetBinaryControlMD3IndexUsingODCIndex(const size_t index, uint8_t & module, uint8_t & channel, BinaryPointType & pointtype);

	bool GetAnalogControlODCIndexUsingMD3Index(const uint16_t module, const uint8_t channel, size_t & index);
	bool GetAnalogControlMD3IndexUsingODCIndex(const size_t index, uint8_t& module, uint8_t& channel);

	void ForEachBinaryPoint(const std::function<void(MD3BinaryPoint &pt)>&);
	void ForEachAnalogPoint(const std::function<void(MD3AnalogCounterPoint&pt)>& fn);
	void ForEachCounterPoint(const std::function<void(MD3AnalogCounterPoint&pt)>& fn);

	std::vector<MD3BinaryPoint> DumpTimeTaggedPointList();

	bool TimeTaggedDataAvailable();
	bool PeekNextTaggedEventPoint(MD3BinaryPoint & pt);
	bool PopNextTaggedEventPoint();

	void AddToDigitalEvents(MD3BinaryPoint & inpt);
	uint16_t CollectModuleBitsIntoWordandResetChangeFlags(const uint8_t ModuleAddress, bool & ModuleFailed);
	uint16_t CollectModuleBitsIntoWord(const uint8_t ModuleAddress, bool & ModuleFailed);

protected:

	// We access the map using a Module:Channel combination, so that they will always be in order. Makes searching the next item easier.
	std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>> BinaryMD3PointMap; // ModuleAndChannel, MD3Point
	std::map<size_t, std::shared_ptr<MD3BinaryPoint>> BinaryODCPointMap;   // Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogMD3PointMap; // ModuleAndChannel, MD3Point
	std::map<size_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogODCPointMap;   // Index OpenDataCon, MD3Point

	std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> CounterMD3PointMap; // ModuleAndChannel, MD3Point
	std::map<size_t, std::shared_ptr<MD3AnalogCounterPoint>> CounterODCPointMap;   // Index OpenDataCon, MD3Point

	// Binary Control Points are not readable
	std::map<uint16_t, std::shared_ptr<MD3BinaryPoint>> BinaryControlMD3PointMap; // ModuleAndChannel, MD3Point
	std::map<size_t, std::shared_ptr<MD3BinaryPoint>> BinaryControlODCPointMap;   // Index OpenDataCon, MD3Point

	// Analog Control Points are not readable
	std::map<uint16_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogControlMD3PointMap; // ModuleAndChannel, MD3Point
	std::map<size_t, std::shared_ptr<MD3AnalogCounterPoint>> AnalogControlODCPointMap;   // Index OpenDataCon, MD3Point

	bool IsOutstation = true;
	bool NewDigitalCommands = true;

	// Only used in outstation
	std::shared_ptr<StrandProtectedQueue<MD3BinaryPoint>> pBinaryTimeTaggedEventQueue; // Separate queue for time tagged binary events.
};

#endif
