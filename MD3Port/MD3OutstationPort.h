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

#include <unordered_map>
#include <vector>
#include <functional>

#include "MD3.h"
#include "MD3Port.h"
#include "MD3Engine.h"
#include "MD3Connection.h"

class MD3OutstationPort: public MD3Port
{
	enum AnalogChangeType { NoChange, DeltaChange, AllChange };
	enum AnalogCounterModuleType { CounterModule, AnalogModule };

public:
	MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~MD3OutstationPort() override;

	void Enable() override;
	void Disable() override;
	void BuildOrRebuild();

	void Event(const Binary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const Analog& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const Counter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

//	void Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
//	void Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	// These are the only quality callbacks we handle, as by setting a value of 0x8000 we can communicate that information up the line.
	// The binary points have no way of sending that information to the MD3Master.
	void Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
	void Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
//	void Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
//	void Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;
//	void Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	void ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override;

	template<typename T> void EventT(T& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback);
	template<typename T> void EventQ(T& qual, uint16_t index, const std::string & SenderName, SharedStatusCallback_t pStatusCallback);

	template<typename T> CommandStatus SupportsT(T& arCommand, uint16_t aIndex);

	CommandStatus Perform(const AnalogOutputDouble64 & arCommand, uint16_t index, bool waitforresult);
	CommandStatus Perform(const AnalogOutputInt32 & arCommand, uint16_t index, bool waitforresult);
	CommandStatus Perform(const AnalogOutputInt16 & arCommand, uint16_t index, bool waitforresult);

	CommandStatus Perform(const ControlRelayOutputBlock & arCommand, uint16_t index, bool waitforresult);

	template<typename T> CommandStatus PerformT(T& arCommand, uint16_t aIndex, bool waitforresult);

	void ProcessMD3Message(MD3Message_t &CompleteMD3Message);

	// Analog
	void DoAnalogUnconditional(MD3BlockFormatted &Header);
	void DoCounterScan(MD3BlockFormatted & Header);
	void DoAnalogDeltaScan(MD3BlockFormatted &Header);

	void ReadAnalogOrCounterRange(int ModuleAddress, int Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues);
	void GetAnalogModuleValues(AnalogCounterModuleType IsCounterOrAnalog, int Channels, int ModuleAddress, MD3OutstationPort::AnalogChangeType & ResponseType, std::vector<uint16_t>& AnalogValues, std::vector<int>& AnalogDeltaValues);
	void SendAnalogOrCounterUnconditional(MD3_FUNCTION_CODE functioncode, std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);

	// Digital/Binary
	void DoDigitalScan(MD3BlockFn11MtoS & Header);                              // Fn 7
	void DoDigitalChangeOnly(MD3BlockFormatted & Header);                       // Fn 8
	void DoDigitalHRER(MD3BlockFn9 & Header, MD3Message_t& CompleteMD3Message); // Fn 9
	void Fn9AddTimeTaggedDataToResponseWords(int MaxEventCount, int & EventCount, std::vector<uint16_t>& ResponseWords);
	void DoDigitalCOSScan(MD3BlockFn10 & Header);               // Fn 10
	void DoDigitalUnconditionalObs(MD3BlockFormatted & Header); // Fn 11
	void Fn11AddTimeTaggedDataToResponseWords(int MaxEventCount, int & EventCount, std::vector<uint16_t>& ResponseWords);
	void DoDigitalUnconditional(MD3BlockFn12MtoS & Header); // Fn 12

	void MarkAllBinaryBlocksAsChanged();

	int CountBinaryBlocksWithChanges();
	int CountBinaryBlocksWithChangesGivenRange(int NumberOfDataBlocks, int StartModuleAddress);
	void BuildListOfModuleAddressesWithChanges(int NumberOfDataBlocks, int StartModuleAddress, bool forcesend, std::vector<uint8_t>& ModuleList);
	void BuildBinaryReturnBlocks(int NumberOfDataBlocks, int StartModuleAddress, int StationAddress, bool forcesend, MD3Message_t &ResponseMD3Message);
	void BuildScanReturnBlocksFromList(std::vector<unsigned char>& ModuleList, int MaxNumberOfDataBlocks, int StationAddress, bool FormatForFn11and12, MD3Message_t& ResponseMD3Message);
	void BuildListOfModuleAddressesWithChanges(int StartModuleAddress, std::vector<uint8_t> &ModuleList);

	void DoFreezeResetCounters(MD3BlockFn16MtoS & Header);
	void DoPOMControl(MD3BlockFn17MtoS & Header, MD3Message_t& CompleteMD3Message);
	void DoDOMControl(MD3BlockFn19MtoS & Header, MD3Message_t& CompleteMD3Message);
	void DoAOMControl(MD3BlockFn23MtoS & Header, MD3Message_t& CompleteMD3Message);

	void DoSystemSignOnControl(MD3BlockFn40 & Header);
	void DoSetDateTime(MD3BlockFn43MtoS & Header, MD3Message_t& CompleteMD3Message);     // Fn 43
	void DoSystemFlagScan(MD3BlockFormatted & Header, MD3Message_t& CompleteMD3Message); // Fn 52

	void SendControlOK(MD3BlockFormatted & Header);             // Fn 15
	void SendControlOrScanRejected(MD3BlockFormatted & Header); // Fn 30


private:

	void SocketStateHandler(bool state);

	int LastHRERSequenceNumber = 100;      // Used to remember the last HRER scan we sent, starts with an invalid value
	int LastDigitalScanSequenceNumber = 0; // Used to remember the last digital scan we had
	MD3Message_t LastDigitialScanResponseMD3Message;
	MD3Message_t LastDigitialHRERResponseMD3Message;
};

#endif