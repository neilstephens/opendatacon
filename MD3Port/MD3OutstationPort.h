/*	opendatacon
 *
 *	Copyright (c) 2014:
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
 * MD3OutstationPort.h
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#ifndef MD3SERVERPORT_H_
#define MD3SERVERPORT_H_

#include <unordered_map>
#include <vector>
#include <functional>


#include "MD3.h"
#include "MD3Port.h"
#include "MD3Engine.h"

class MD3OutstationPort: public MD3Port
{
	enum AnalogChangeType {NoChange, DeltaChange, AllChange};

public:
	MD3OutstationPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);
	~MD3OutstationPort() override;

	void Enable() override;
	void Disable() override;
	void BuildOrRebuild(IOManager& IOMgr, openpal::LogFilters& LOG_LEVEL) override;

	std::future<CommandStatus> Event(const Binary& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const Analog& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const Counter& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName) override;
	std::future<CommandStatus> Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName) override;

	template<typename T> CommandStatus SupportsT(T& arCommand, uint16_t aIndex);
	template<typename T> CommandStatus PerformT(T& arCommand, uint16_t aIndex);
	template<typename T> std::future<CommandStatus> EventT(T& meas, uint16_t index, const std::string& SenderName);

	// only public for unit testing - could use a friend class to access?
	void ReadCompletionHandler(buf_t& readbuf);
	void RouteMD3Message(std::vector<MD3DataBlock> &CompleteMD3Message);
	void ProcessMD3Message(std::vector<MD3DataBlock> &CompleteMD3Message);

	// Analog
	void DoAnalogUnconditional(MD3FormattedBlock &Header);
	void DoAnalogDeltaScan(MD3FormattedBlock &Header);

	void ReadAnalogRange(int ModuleAddress, int Channels, MD3OutstationPort::AnalogChangeType &ResponseType, std::vector<uint16_t> &AnalogValues, std::vector<int> &AnalogDeltaValues);
	void SendAnalogUnconditional(std::vector<uint16_t> Analogs, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogDelta(std::vector<int> Deltas, uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);
	void SendAnalogNoChange(uint8_t StationAddress, uint8_t ModuleAddress, uint8_t Channels);

	// Digital/Binary
	void DoDigitalScan(MD3BlockFn11MtoS & Header);				// Fn 7
	void DoDigitalChangeOnly(MD3FormattedBlock & Header);		// Fn 8
	void DoDigitalHRER(MD3BlockFn9 & Header);					// Fn 9
	void DoDigitalCOSScan(MD3BlockFn10 & Header);				// Fn 10
	void DoDigitalUnconditionalObs(MD3FormattedBlock & Header);	// Fn 11
	void DoDigitalUnconditional(MD3BlockFn12MtoS & Header);		// Fn 12

	int CheckBinaryChangeBlocks(int & ChangedTimeTaggedBlocks, bool SendEverything);
	int CheckBinaryChangeBlocksGivenRange(int NumberOfDataBlocks, int StartModuleAddress);
	bool CheckForBinaryBlockChanges();
	void BuildBinaryReturnBlocks(int NumberOfDataBlocks, int StartModuleAddress, int StationAddress, bool forcesend, std::vector<MD3DataBlock> &ResponseMD3Message);
	void BuildBinaryScanReturnBlocks(int MaxNumberOfDataBlocks, int StartModuleAddress, int StationAddress, std::vector<MD3DataBlock>& ResponseMD3Message);
	void BuildScanReturnBlocksFromList(std::vector<unsigned char> &ModuleList, int MaxNumberOfDataBlocks, int StationAddress, std::vector<MD3DataBlock> & ResponseMD3Message);
	void BuildListOfModuleAddressesWithChanges(int StartModuleAddress, std::vector<uint8_t> &ModuleList);

	// Methods to access the outstation point table
	//TODO: Point container access extract to separate class maybe..
	bool GetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res);
	bool GetAnalogValueAndChangeUsingMD3Index(const uint16_t module, const uint8_t channel, uint16_t &res, int &delta);
	bool SetAnalogValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint16_t meas);
	bool GetAnalogValueUsingODCIndex(const uint16_t index, uint16_t &res);
	bool SetAnalogValueUsingODCIndex(const uint16_t index, const uint16_t meas);
	bool GetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, uint8_t &res, bool &changed);
	bool GetBinaryChangedUsingMD3Index(const uint16_t module, const uint8_t channel, bool &changed);
	bool SetBinaryValueUsingMD3Index(const uint16_t module, const uint8_t channel, const uint8_t meas);
	bool GetBinaryValueUsingODCIndex(const uint16_t index, uint8_t &res, bool &changed);
	bool SetBinaryValueUsingODCIndex(const uint16_t index, const uint8_t meas, uint64_t eventtime);

	void SendResponse(std::vector<MD3DataBlock>& CompleteMD3Message);

	// Testing and Debugging Methods - no other use
	// This allows us to hook the TCP Data Send Fucntion for testing.
	void SetSendTCPDataFn(std::function<void(std::string)> Send);
	void AddToDigitalEvents(const MD3Point & pt);
private:

	void SocketStateHandler(bool state);

	typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;

	std::vector<MD3DataBlock> MD3Message;

	int LastHRERSequenceNumber = 100;	// Used to remember the last HRER scan we sent, starts with an invalid value
	int LastDigitalScanSequenceNumber = 0;	// Used to remember the last digital scan we had
	std::vector<MD3DataBlock> LastDigitialScanResponseMD3Message;
	std::vector<MD3DataBlock> LastDigitialHRERResponseMD3Message;

	// Maintain a pointer to the sending function, so that we can hook it for testing purposes. Set to  default in constructor.
	std::function<void(std::string)> SendTCPDataFn = nullptr;	// nullptr normally. Set to hook function for testing

	// Worker functions to try and clean up the code...
	MD3PortConf* MyConf();
	std::shared_ptr<MD3PointConf> MyPointConf();
};

#endif