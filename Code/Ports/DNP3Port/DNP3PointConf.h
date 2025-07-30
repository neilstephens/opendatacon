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
#include <opendnp3/app/MeasurementInfo.h>
#include <opendnp3/app/ClassField.h>
#include <opendnp3/gen/OperationType.h>
#include <opendnp3/gen/TripCloseCode.h>
#include <opendnp3/gen/PointClass.h>
#include <opendnp3/gen/ServerAcceptMode.h>
#include <opendatacon/DataPointConf.h>
#include <opendatacon/ConfigParser.h>
#include <opendatacon/IOTypes.h>

class DNP3PointConf: public ConfigParser
{
public:
	DNP3PointConf(const std::string& FileName, const Json::Value& ConfOverrides);

	void ProcessElements(const Json::Value& JSONRoot) override;

	// DNP3 Link Configuration
	uint32_t LinkNumRetry;
	uint32_t LinkTimeoutms;
	uint32_t LinkKeepAlivems;
	bool NackConfirmedUDWhenUnreset;

	/// Common application stack configuration
	opendnp3::ServerAcceptMode ServerAcceptMode;
	uint32_t IPConnectRetryPeriodMinms;
	uint32_t IPConnectRetryPeriodMaxms;
	bool EnableUnsol;
	opendnp3::ClassField GetUnsolClassMask();
	bool UnsolClass1;
	bool UnsolClass2;
	bool UnsolClass3;

	// Master Station configuration
	uint32_t MasterResponseTimeoutms;           /// Application layer response timeout
	bool MasterRespondTimeSync;                 /// If true, the master will do time syncs when it sees the time IIN bit from the outstation
	bool LANModeTimeSync;                       /// If true, the master will use the LAN time sync mode
	bool DisableUnsolOnStartup;                 /// If true, the master will disable unsol on startup (warning: setting false produces non-compliant behaviour)
	bool SetQualityOnLinkStatus;                /// Whether to set point quality when link down
	odc::QualityFlags FlagsToSetOnLinkStatus;   /// The flags to Set when SetQualityOnLinkStatus is true
	odc::QualityFlags FlagsToClearOnLinkStatus; /// The flags to Clear when SetQualityOnLinkStatus is true
	uint32_t CommsPointRideThroughTimems;       /// How long to wait before admitting the link is down
	bool CommsPointRideThroughDemandPause;      /// Whether to pause the ridethrough timer if there's no 'demand'
	uint32_t CommsPointHeartBeatTimems;         /// Send a periodic comms event with this period if non zero
	/// Which classes should be scanned if IIN 1.1/2/3 flags are set
	opendnp3::ClassField GetEventScanOnEventsAvailableClassMask();
	bool EventScanOnEventsAvailableClass1;
	bool EventScanOnEventsAvailableClass2;
	bool EventScanOnEventsAvailableClass3;
	/// Which classes should be requested in a startup integrity scan
	opendnp3::ClassField GetStartupIntegrityClassMask();
	bool StartupIntegrityClass0;
	bool StartupIntegrityClass1;
	bool StartupIntegrityClass2;
	bool StartupIntegrityClass3;
	uint32_t LinkUpIntegrityGracePeriodms;
	/// When will the startup integrity scan be triggered
	enum class LinkUpIntegrityTrigger_t { NEVER, ON_FIRST, ON_EVERY };
	LinkUpIntegrityTrigger_t LinkUpIntegrityTrigger;
	/// Which classes should be requested for forced integrity scans
	opendnp3::ClassField GetForcedIntegrityClassMask();
	bool ForcedIntegrityClass0;
	bool ForcedIntegrityClass1;
	bool ForcedIntegrityClass2;
	bool ForcedIntegrityClass3;
	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected.
	/// Warning: non compliant behaviour if set to false
	bool IntegrityOnEventOverflowIIN;
	/// Choose to ignore DEVICE_RESTART IIN flag. Warning: non compliant behaviour if set to true
	bool IgnoreRestartIIN;
	/// Whether to retry a integrity scan that is forced by IIN
	bool RetryForcedIntegrity;
	/// Time delay beforce retrying a failed task
	uint32_t TaskRetryPeriodms;

	// Master Station scanning configuration
	size_t IntegrityScanRatems;
	size_t EventClass1ScanRatems;
	size_t EventClass2ScanRatems;
	size_t EventClass3ScanRatems;

	std::pair<opendnp3::OperationType,opendnp3::TripCloseCode> OverrideControlCode;
	bool DoAssignClassOnStartup;

	// Outstation configuration
	uint8_t MaxControlsPerRequest;
	uint32_t MaxTxFragSize;
	uint32_t SelectTimeoutms;       /// How long the outstation will allow an operate to proceed after a prior select
	uint32_t SolConfirmTimeoutms;   /// Timeout for solicited confirms
	uint32_t UnsolConfirmTimeoutms; /// Timeout for unsolicited confirms
	bool WaitForCommandResponses;   // when responding to a command, wait for downstream command responses, otherwise returns success
	bool TimeSyncOnStart;
	uint64_t TimeSyncPeriodms;

	// Default Static Variations
	opendnp3::StaticBinaryVariation StaticBinaryResponse;
	opendnp3::StaticAnalogVariation StaticAnalogResponse;
	opendnp3::StaticCounterVariation StaticCounterResponse;
	opendnp3::StaticAnalogOutputStatusVariation StaticAnalogOutputStatusResponse;
	opendnp3::StaticBinaryOutputStatusVariation StaticBinaryOutputStatusResponse;

	// Default Event Variations
	opendnp3::EventBinaryVariation EventBinaryResponse;
	opendnp3::EventAnalogVariation EventAnalogResponse;
	opendnp3::EventCounterVariation EventCounterResponse;
	opendnp3::EventAnalogOutputStatusVariation EventAnalogOutputStatusResponse;
	opendnp3::EventBinaryOutputStatusVariation EventBinaryOutputStatusResponse;

	odc::EventType AnalogControlType;

	// Timestamp override options
	enum class TimestampOverride_t { ALWAYS, ZERO, NEVER };
	TimestampOverride_t TimestampOverride;

	// Event buffer limits
	uint16_t MaxBinaryEvents;             /// The number of binary events the outstation will buffer before overflowing
	uint16_t MaxAnalogEvents;             /// The number of analog events the outstation will buffer before overflowing
	uint16_t MaxCounterEvents;            /// The number of counter events the outstation will buffer before overflowing
	uint16_t MaxOctetStringEvents;        /// The number of octet string events the outstation will buffer before overflowing
	uint16_t MaxAnalogOutputStatusEvents; /// The number of analog output status events the outstation will buffer before overflowing
	uint16_t MaxBinaryOutputStatusEvents; /// The number of binary output status events the outstation will buffer before overflowing


	// Point Configuration

	std::pair<opendnp3::Binary, size_t> mCommsPoint;
	bool AllowUnknownIndexes;

	// TODO: use struct or class for point configuration

	// Binary Point Configuration
	std::vector<uint16_t> BinaryIndexes;
	std::map<uint16_t, opendnp3::Binary> BinaryStartVals;
	std::map<uint16_t, opendnp3::PointClass> BinaryClasses;
	std::map<uint16_t, opendnp3::StaticBinaryVariation> StaticBinaryResponses;
	std::map<uint16_t, opendnp3::EventBinaryVariation> EventBinaryResponses;

	// Octet String Point Configuration
	std::vector<uint16_t> OctetStringIndexes;
	std::map<uint16_t, opendnp3::PointClass> OctetStringClasses;

	// Analog Point Configuration
	std::vector<uint16_t> AnalogIndexes;
	std::map<uint16_t, opendnp3::Analog> AnalogStartVals;
	std::map<uint16_t, opendnp3::StaticAnalogVariation> StaticAnalogResponses;
	std::map<uint16_t, opendnp3::EventAnalogVariation> EventAnalogResponses;
	std::map<uint16_t, opendnp3::PointClass> AnalogClasses;
	std::map<uint16_t, double> AnalogDeadbands;

	//Analog Output Status Point Configuration
	std::vector<uint16_t> AnalogOutputStatusIndexes;
	std::map<uint16_t, opendnp3::AnalogOutputStatus> AnalogOutputStatusStartVals;
	std::map<uint16_t, opendnp3::PointClass> AnalogOutputStatusClasses;
	std::map<uint16_t, opendnp3::StaticAnalogOutputStatusVariation> StaticAnalogOutputStatusResponses;
	std::map<uint16_t, opendnp3::EventAnalogOutputStatusVariation> EventAnalogOutputStatusResponses;
	std::map<uint16_t, double> AnalogOutputStatusDeadbands;

	// Binary Output Status Point Configuration
	std::vector<uint16_t> BinaryOutputStatusIndexes;
	std::map<uint16_t, opendnp3::BinaryOutputStatus> BinaryOutputStatusStartVals;
	std::map<uint16_t, opendnp3::PointClass> BinaryOutputStatusClasses;
	std::map<uint16_t, opendnp3::StaticBinaryOutputStatusVariation> StaticBinaryOutputStatusResponses;
	std::map<uint16_t, opendnp3::EventBinaryOutputStatusVariation> EventBinaryOutputStatusResponses;


	std::vector<uint16_t> ControlIndexes;
	std::vector<uint16_t> AnalogControlIndexes;
	std::map<uint16_t, odc::EventType> AnalogControlTypes;
};

#endif /* DNP3POINTCONF_H_ */
