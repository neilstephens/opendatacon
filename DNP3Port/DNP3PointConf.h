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
 * DNP3PointConf.h
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef DNP3POINTCONF_H_
#define DNP3POINTCONF_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/gen/ControlCode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>

class DNP3PointConf: public ConfigParser
{
public:
	DNP3PointConf(std::string FileName);

	void ProcessElements(const Json::Value& JSONRoot);

	// DNP3 Link Configuration
	uint32_t LinkNumRetry;
	uint32_t LinkTimeoutms;
	uint32_t LinkKeepAlivems;
	bool LinkUseConfirms;

	/// Common application stack configuration
	bool EnableUnsol;
	uint8_t GetUnsolClassMask();
	bool UnsolClass1;
	bool UnsolClass2;
	bool UnsolClass3;

	// Master Station configuration
	uint32_t TCPConnectRetryPeriodMinms;
	uint32_t TCPConnectRetryPeriodMaxms;
	uint32_t MasterResponseTimeoutms; /// Application layer response timeout
	bool MasterRespondTimeSync; /// If true, the master will do time syncs when it sees the time IIN bit from the outstation
	bool DoUnsolOnStartup; /// If true, the master will enable unsol on startup
	/// Which classes should be requested in a startup integrity scan
	uint8_t GetStartupIntegrityClassMask();
	bool StartupIntegrityClass0;
	bool StartupIntegrityClass1;
	bool StartupIntegrityClass2;
	bool StartupIntegrityClass3;
	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected
	bool IntegrityOnEventOverflowIIN;
	/// Time delay beforce retrying a failed task
	uint32_t TaskRetryPeriodms;

	// Master Station scanning configuration
	size_t IntegrityScanRatems;
	size_t EventClass1ScanRatems;
	size_t EventClass2ScanRatems;
	size_t EventClass3ScanRatems;

	opendnp3::ControlCode OverrideControlCode;
	bool DoAssignClassOnStartup;

	// Outstation configuration
	uint32_t TCPListenRetryPeriodMinms;
	uint32_t TCPListenRetryPeriodMaxms;
	uint8_t	MaxControlsPerRequest;
	uint32_t MaxTxFragSize;
	uint32_t SelectTimeoutms; /// How long the outstation will allow an operate to proceed after a prior select
	uint32_t SolConfirmTimeoutms; /// Timeout for solicited confirms
	uint32_t UnsolConfirmTimeoutms; /// Timeout for unsolicited confirms
	bool WaitForCommandResponses; // when responding to a command, wait for downstream command responses, otherwise returns success

	// Default Static Variations
	opendnp3::Binary::StaticVariation StaticBinaryResponse;
	opendnp3::Analog::StaticVariation StaticAnalogResponse;
	opendnp3::Counter::StaticVariation StaticCounterResponse;

	// Default Event Variations
	opendnp3::Binary::EventVariation EventBinaryResponse;
	opendnp3::Analog::EventVariation EventAnalogResponse;
	opendnp3::Counter::EventVariation EventCounterResponse;

	// Timestamp override options
	typedef enum { ALWAYS, ZERO, NEVER } TimestampOverride_t;
	TimestampOverride_t TimestampOverride;

	// Event buffer limits
	uint16_t MaxBinaryEvents; /// The number of binary events the outstation will buffer before overflowing
	uint16_t MaxAnalogEvents;	/// The number of analog events the outstation will buffer before overflowing
	uint16_t MaxCounterEvents;	/// The number of counter events the outstation will buffer before overflowing

	// Point Configuration
	std::pair<opendnp3::Binary, size_t> mCommsPoint;
	std::vector<uint32_t> BinaryIndicies;
	std::map<size_t, opendnp3::Binary> BinaryStartVals;
	std::map<size_t, opendnp3::PointClass> BinaryClasses;
	std::vector<uint32_t> AnalogIndicies;
	std::map<size_t, opendnp3::Analog> AnalogStartVals;
	std::map<size_t, opendnp3::PointClass> AnalogClasses;
	std::map<size_t, double> AnalogDeadbands;
	std::vector<uint32_t> ControlIndicies;
};

#endif /* DNP3POINTCONF_H_ */
