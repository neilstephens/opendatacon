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
* MD3OutStationPort.h
*
*  Created on: 01/04/2018
*      Author: Scott Ellis <scott.ellis@novatex.com.au>
*/

#ifndef MD3OUTSTATIONPORT_H_
#define MD3OUTSTATIONPORT_H_
#include "MD3.h"
#include "MD3Port.h"
#include "MD3Utility.h"
#include "MD3Connection.h"
#include "MD3PointTableAccess.h"
#include <unordered_map>
#include <vector>
#include <functional>


class OutstationSystemFlags
{
	// MD3 can support 16 bits of status flags, which are reported by Fn52, system flag scan. The first 8 are reserved for MegaData use, only 3 are documented.
	// The last 8 are contract dependent, we don't know if any are used.
	// A change in any will set the RSF bit in ANY scan/control replies. So we maintain a separate RSF bit in the structure, which will be reset on a flag scan.

public:
	bool GetRemoteStatusChangeFlag() { return RSF; }

	// This is calculated by checking the digital bit changed flag, using a method registered with us
	bool GetDigitalChangedFlag()
	{
		if (DCPCalc != nullptr)
			return DCPCalc();

		LOGERROR("GetDigitalChangedFlag called without a handler being registered");
		return false;
	}

	// This is calculated by checking the timetagged data queues, using a method registered with us
	bool GetTimeTaggedDataAvailableFlag()
	{
		if (HRPCalc != nullptr)
			return HRPCalc();

		LOGERROR("GetTimeTaggedDataAvailableFlag called without a handler being registered");
		return false;
	}

	bool GetSystemPoweredUpFlag() { return SPU; }
	bool GetSystemTimeIncorrectFlag() { return STI; }

	void FlagScanPacketSent() { SPU = false; RSF = false; }
	void TimePacketReceived() { STI = false; }

	void SetDigitalChangedFlagCalculationMethod(std::function<bool(void)> Calc) { DCPCalc = Calc; }
	void SetTimeTaggedDataAvailableFlagCalculationMethod(std::function<bool(void)> Calc) { HRPCalc = Calc; }

private:
	bool RSF = true;                             // All true on start up...
	std::function<bool(void)> HRPCalc = nullptr; // HRER/TimeTagged Events Pending
	std::function<bool(void)> DCPCalc = nullptr; // Digital bit has changed and is waiting to be sent

	bool SPU = true; // Bit 16 of Block data, bit 0 of 16 bit flag data
	bool STI = true; // Bit 17 of Block data, bit 1 of 16 bit flag data
};

class MD3OutstationPortCollection;

class MD3OutstationPort: public MD3Port
{
	enum AnalogChangeType { NoChange, DeltaChange, AllChange };
	enum AnalogCounterModuleType { CounterModule, AnalogModule };

public:
	MD3OutstationPort(const std::string & aName, const std::string & aConfFilename, const Json::Value & aConfOverrides);
	void UpdateOutstationPortCollection();
	~MD3OutstationPort() override;

	void Enable() override;
	void Disable() override final;
	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	CommandStatus Perform(const std::shared_ptr<EventInfo>& event, bool waitforresult);

	void SendMD3Message(const MD3Message_t & CompleteMD3Message) override;
	MD3Message_t CorruptMD3Message(const MD3Message_t& CompleteMD3Message);
	void ProcessMD3Message(MD3Message_t &CompleteMD3Message);

	// Analog
	void DoAnalogUnconditional(MD3BlockFormatted &Header);
	void DoCounterScan(MD3BlockFormatted & Header);
	void DoAnalogDeltaScan(MD3BlockFormatted &Header);

	void ReadAnalogOrCounterRange(uint8_t ModuleAddress, uint8_t Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues);
	void GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, uint8_t Channels, uint8_t ModuleAddress, MD3OutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t>& AnalogValues, std::vector<int>& AnalogDeltaValues);
	void SendAnalogOrCounterUnconditional(MD3_FUNCTION_CODE functioncode, std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);

	// Digital/Binary
	void DoDigitalScan(MD3BlockFn11MtoS & Header); // Fn 7
	void MarkAllBinaryPointsAsChanged();
	void DoDigitalChangeOnly(MD3BlockFormatted & Header);                       // Fn 8
	void DoDigitalHRER(MD3BlockFn9 & Header, MD3Message_t& CompleteMD3Message); // Fn 9
	void Fn9AddTimeTaggedDataToResponseWords(uint8_t MaxEventCount, uint8_t & EventCount, std::vector<uint16_t>& ResponseWords);
	void DoDigitalCOSScan(MD3BlockFn10 & Header);               // Fn 10
	void DoDigitalUnconditionalObs(MD3BlockFormatted & Header); // Fn 11
	void Fn11AddTimeTaggedDataToResponseWords(uint8_t MaxEventCount, uint8_t & EventCount, std::vector<uint16_t>& ResponseWords);
	void DoDigitalUnconditional(MD3BlockFn12MtoS & Header); // Fn 12

	uint8_t CountBinaryBlocksWithChanges();
	uint8_t CountBinaryBlocksWithChangesGivenRange(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress);
	void BuildListOfModuleAddressesWithChanges(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, bool forcesend, std::vector<uint8_t>& ModuleList);
	void BuildBinaryReturnBlocks(uint8_t NumberOfDataBlocks, uint8_t StartModuleAddress, uint8_t StationAddress, bool forcesend, MD3Message_t &ResponseMD3Message);
	void BuildScanReturnBlocksFromList(std::vector<unsigned char>& ModuleList, uint8_t MaxNumberOfDataBlocks, uint8_t StationAddress, bool FormatForFn11and12, MD3Message_t& ResponseMD3Message);
	void BuildListOfModuleAddressesWithChanges(uint8_t StartModuleAddress, std::vector<uint8_t> &ModuleList);

	void DoFreezeResetCounters(MD3BlockFn16MtoS & Header);
	void DoPOMControl(MD3BlockFn17MtoS & Header, MD3Message_t& CompleteMD3Message);
	void DoDOMControl(MD3BlockFn19MtoS & Header, MD3Message_t& CompleteMD3Message);
	void DoInputPointControl(MD3BlockFn20MtoS & Header, MD3Message_t & CompleteMD3Message);
	void DoAOMControl(MD3BlockFn23MtoS & Header, MD3Message_t& CompleteMD3Message);

	void DoSystemSignOnControl(MD3BlockFn40MtoS & Header);
	void DoSetDateTime(MD3BlockFn43MtoS & Header, MD3Message_t& CompleteMD3Message); // Fn 43
	void DoSetDateTimeNew(MD3BlockFn44MtoS & Header, MD3Message_t & CompleteMD3Message);
	void DoSystemFlagScan(MD3BlockFn52MtoS & Header, MD3Message_t & CompleteMD3Message); // Fn 52

	void SendControlOK(MD3BlockFormatted & Header);             // Fn 15
	void SendControlOrScanRejected(MD3BlockFormatted & Header); // Fn 30

	// UI Interactions
	std::pair<std::string, std::shared_ptr<IUIResponder>> GetUIResponder() final;
	bool UIFailControl(const std::string& active);                // Shift the control response channel from the correct set channel to an alternative channel.
	bool UIRandomReponseBitFlips(const std::string& probability); // Zero probability = does not happen. 1 = there is a bit flip in every response packet.

	// Testing use only
	MD3PointTableAccess *GetPointTable() { return &(MyPointConf->PointTable); }
	int GetSOEOffsetMinutes() { return SOETimeOffsetMinutes; }

private:
	std::shared_ptr<MD3OutstationPortCollection> MD3OutstationCollection;

	// UI Testing flags to cause misbehaviour
	bool FailControlResponse = false;
	double BitFlipProbability = 0.0;

	bool DigitalChangedFlagCalculationMethod(void);
	bool TimeTaggedDataAvailableFlagCalculationMethod(void);
	int SOETimeOffsetMinutes = 0;

	OutstationSystemFlags SystemFlags;

	void SocketStateHandler(bool state);

	uint8_t LastHRERSequenceNumber = 100;      // Used to remember the last HRER scan we sent, starts with an invalid value
	uint8_t LastDigitalScanSequenceNumber = 0; // Used to remember the last digital scan we had
	MD3Message_t LastDigitialScanResponseMD3Message;
	MD3Message_t LastDigitialHRERResponseMD3Message;
};

#endif
