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
 * DNP3PointConf.cpp
 *
 *  Created on: 16/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "DNP3PointConf.h"
#include "OpenDNP3Helpers.h"
#include "opendatacon/IOTypes.h"
#include <algorithm>
#include <opendatacon/util.h>
#include <opendnp3/app/ClassField.h>
#include <regex>


DNP3PointConf::DNP3PointConf(const std::string& FileName, const Json::Value& ConfOverrides):
	ConfigParser(FileName, ConfOverrides),
	// DNP3 Link Configuration
	LinkTimeoutms(1000),
	LinkKeepAlivems(10000),
	LinkUseConfirms(false),
	// Common application stack configuration
	ServerAcceptMode(opendnp3::ServerAcceptMode::CloseNew),
	IPConnectRetryPeriodMinms(500),
	IPConnectRetryPeriodMaxms(30000),
	EnableUnsol(true),
	UnsolClass1(false),
	UnsolClass2(false),
	UnsolClass3(false),
	// Master Station configuration
	MasterResponseTimeoutms(5000), /// Application layer response timeout
	MasterRespondTimeSync(true),   /// If true, the master will do time syncs when it sees the time IIN bit from the outstation
	LANModeTimeSync(false),        /// If true, the master will use the LAN time sync mode
	DoUnsolOnStartup(true),
	SetQualityOnLinkStatus(true),
	FlagsToSetOnLinkStatus(odc::QualityFlags::COMM_LOST),
	FlagsToClearOnLinkStatus(odc::QualityFlags::ONLINE),
	CommsPointRideThroughTimems(0),
	CommsPointHeartBeatTimems(0),
	/// Which classes should be requested in a startup integrity scans
	StartupIntegrityClass0(true),
	StartupIntegrityClass1(true),
	StartupIntegrityClass2(true),
	StartupIntegrityClass3(true),
	LinkUpIntegrityTrigger(LinkUpIntegrityTrigger_t::ON_FIRST),
	/// Which classes should be requested for forced integrity scans
	ForcedIntegrityClass0(true),
	ForcedIntegrityClass1(true),
	ForcedIntegrityClass2(true),
	ForcedIntegrityClass3(true),
	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected
	IntegrityOnEventOverflowIIN(true),
	/// Choose to ignore DEVICE_RESTART IIN flag. Warning: non compliant behaviour if set to true
	IgnoreRestartIIN(false),
	/// Whether to retry a integrity scan that is forced by IIN
	RetryForcedIntegrity(false),
	/// Time delay beforce retrying a failed task
	TaskRetryPeriodms(5000),
	// Master Station scanning configuration
	IntegrityScanRatems(3600000),
	EventClass1ScanRatems(1000),
	EventClass2ScanRatems(1000),
	EventClass3ScanRatems(1000),
	OverrideControlCode(std::pair<opendnp3::OperationType, opendnp3::TripCloseCode>(opendnp3::OperationType::Undefined, opendnp3::TripCloseCode::NUL)),
	DoAssignClassOnStartup(false),
	// Outstation configuration
	MaxControlsPerRequest(16),
	MaxTxFragSize(2048),
	SelectTimeoutms(10000),
	SolConfirmTimeoutms(5000),
	UnsolConfirmTimeoutms(5000),
	WaitForCommandResponses(false),
	TimeSyncOnStart(false),
	TimeSyncPeriodms(0),
	// Default Static Variations
	StaticBinaryResponse(opendnp3::StaticBinaryVariation::Group1Var1),
	StaticAnalogResponse(opendnp3::StaticAnalogVariation::Group30Var5),
	StaticCounterResponse(opendnp3::StaticCounterVariation::Group20Var1),
	StaticAnalogOutputStatusResponse(opendnp3::StaticAnalogOutputStatusVariation::Group40Var1),
	StaticBinaryOutputStatusResponse(opendnp3::StaticBinaryOutputStatusVariation::Group10Var2),
	// Default Event Variations
	EventBinaryResponse(opendnp3::EventBinaryVariation::Group2Var1),
	EventAnalogResponse(opendnp3::EventAnalogVariation::Group32Var5),
	EventCounterResponse(opendnp3::EventCounterVariation::Group22Var1),
	EventAnalogOutputStatusResponse(opendnp3::EventAnalogOutputStatusVariation::Group42Var8),
	EventBinaryOutputStatusResponse(opendnp3::EventBinaryOutputStatusVariation::Group11Var2),
	// Default Analog Control Type
	AnalogControlType(odc::EventType::AnalogOutputInt32),
	// Timestamp Override Alternatives
	TimestampOverride(TimestampOverride_t::ZERO),
	// Event buffer limits
	MaxBinaryEvents(1000),
	MaxAnalogEvents(1000),
	MaxCounterEvents(1000),
	MaxOctetStringEvents(1000)
{
	ProcessFile();
}

opendnp3::ClassField DNP3PointConf::GetUnsolClassMask()
{
	return opendnp3::ClassField(true,UnsolClass1,UnsolClass2,UnsolClass3);
}

opendnp3::ClassField DNP3PointConf::GetStartupIntegrityClassMask()
{
	return opendnp3::ClassField(StartupIntegrityClass0,StartupIntegrityClass1,
		StartupIntegrityClass2,StartupIntegrityClass3);
}

opendnp3::ClassField DNP3PointConf::GetForcedIntegrityClassMask()
{
	return opendnp3::ClassField(ForcedIntegrityClass0,ForcedIntegrityClass1,
		ForcedIntegrityClass2,ForcedIntegrityClass3);
}

opendnp3::PointClass GetClass(Json::Value JPoint)
{
	opendnp3::PointClass clazz = opendnp3::PointClass::Class1;
	if(JPoint.isMember("Class"))
	{
		if(JPoint["Class"].isUInt())
		{
			switch(JPoint["Class"].asUInt())
			{
				case 0:
					clazz = opendnp3::PointClass::Class0;
					break;
				case 1:
					clazz = opendnp3::PointClass::Class1;
					break;
				case 2:
					clazz = opendnp3::PointClass::Class2;
					break;
				case 3:
					clazz = opendnp3::PointClass::Class3;
					break;
				default:
					if(auto log = odc::spdlog_get("DNP3Port"))
						log->error("Invalid class for Point: '{}'", JPoint.toStyledString());
					break;
			}
		}
	}
	return clazz;
}

void DNP3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	// DNP3 Link Configuration
	if (JSONRoot.isMember("LinkNumRetry"))
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("Use of 'LinkNumRetry' is deprecated");
	if (JSONRoot.isMember("LinkTimeoutms"))
		LinkTimeoutms = JSONRoot["LinkTimeoutms"].asUInt();
	if (JSONRoot.isMember("LinkKeepAlivems"))
		LinkKeepAlivems = JSONRoot["LinkKeepAlivems"].asUInt();
	if (JSONRoot.isMember("LinkUseConfirms"))
		LinkUseConfirms = JSONRoot["LinkUseConfirms"].asBool();
	if (JSONRoot.isMember("UseConfirms"))
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("Use of 'UseConfirms' is deprecated, use 'LinkUseConfirms' instead : '{}'", JSONRoot["UseConfirms"].toStyledString());

	// Common application configuration
	if (JSONRoot.isMember("ServerAcceptMode"))
	{
		if(JSONRoot["ServerAcceptMode"].asString() == "CloseNew")
			ServerAcceptMode = opendnp3::ServerAcceptMode::CloseNew;
		else if(JSONRoot["ServerAcceptMode"].asString() == "CloseExisting")
			ServerAcceptMode = opendnp3::ServerAcceptMode::CloseExisting;
		else
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("Invalid ServerAcceptMode : '{}'", JSONRoot["ServerAcceptMode"].asString());
	}
	if (JSONRoot.isMember("TCPConnectRetryPeriodMinms"))
	{
		IPConnectRetryPeriodMinms = JSONRoot["TCPConnectRetryPeriodMinms"].asUInt();
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->warn("TCPConnectRetryPeriodMinms is deprecated, use IPConnectRetryPeriodMinms instead");
	}
	if (JSONRoot.isMember("TCPConnectRetryPeriodMaxms"))
	{
		IPConnectRetryPeriodMaxms = JSONRoot["TCPConnectRetryPeriodMaxms"].asUInt();
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->warn("TCPConnectRetryPeriodMaxms is deprecated, use IPConnectRetryPeriodMaxms instead");
	}
	if (JSONRoot.isMember("IPConnectRetryPeriodMinms"))
		IPConnectRetryPeriodMinms = JSONRoot["IPConnectRetryPeriodMinms"].asUInt();
	if (JSONRoot.isMember("IPConnectRetryPeriodMaxms"))
		IPConnectRetryPeriodMaxms = JSONRoot["IPConnectRetryPeriodMaxms"].asUInt();
	if (JSONRoot.isMember("EnableUnsol"))
		EnableUnsol = JSONRoot["EnableUnsol"].asBool();
	if (JSONRoot.isMember("UnsolClass1"))
		UnsolClass1 = JSONRoot["UnsolClass1"].asBool();
	if (JSONRoot.isMember("UnsolClass2"))
		UnsolClass2 = JSONRoot["UnsolClass2"].asBool();
	if (JSONRoot.isMember("UnsolClass3"))
		UnsolClass3 = JSONRoot["UnsolClass3"].asBool();

	// Master Station configuration
	if (JSONRoot.isMember("MasterResponseTimeoutms"))
		MasterResponseTimeoutms = JSONRoot["MasterResponseTimeoutms"].asUInt();
	if (JSONRoot.isMember("MasterRespondTimeSync"))
		MasterRespondTimeSync = JSONRoot["MasterRespondTimeSync"].asBool();
	if (JSONRoot.isMember("LANModeTimeSync"))
		LANModeTimeSync = JSONRoot["LANModeTimeSync"].asBool();
	if (JSONRoot.isMember("DoUnsolOnStartup"))
		DoUnsolOnStartup = JSONRoot["DoUnsolOnStartup"].asBool();
	if (JSONRoot.isMember("SetQualityOnLinkStatus"))
		SetQualityOnLinkStatus = JSONRoot["SetQualityOnLinkStatus"].asBool();
	if (JSONRoot.isMember("FlagsToSetOnLinkStatus"))
		FlagsToSetOnLinkStatus = odc::QualityFlagsFromString(JSONRoot["FlagsToSetOnLinkStatus"].asString());
	if (JSONRoot.isMember("FlagsToClearOnLinkStatus"))
		FlagsToClearOnLinkStatus = odc::QualityFlagsFromString(JSONRoot["FlagsToClearOnLinkStatus"].asString());

	/// Which classes should be requested in a startup integrity scan
	if (JSONRoot.isMember("StartupIntegrityClass0"))
		StartupIntegrityClass0 = JSONRoot["StartupIntegrityClass0"].asBool();
	if (JSONRoot.isMember("StartupIntegrityClass1"))
		StartupIntegrityClass1 = JSONRoot["StartupIntegrityClass1"].asBool();
	if (JSONRoot.isMember("StartupIntegrityClass2"))
		StartupIntegrityClass2 = JSONRoot["StartupIntegrityClass2"].asBool();
	if (JSONRoot.isMember("StartupIntegrityClass3"))
		StartupIntegrityClass3 = JSONRoot["StartupIntegrityClass3"].asBool();
	if (JSONRoot.isMember("LinkUpIntegrityTrigger"))
	{
		auto trig_str = JSONRoot["LinkUpIntegrityTrigger"].asString();
		if(trig_str == "NEVER")
			LinkUpIntegrityTrigger = LinkUpIntegrityTrigger_t::NEVER;
		else if(trig_str == "ON_FIRST")
			LinkUpIntegrityTrigger = LinkUpIntegrityTrigger_t::ON_FIRST;
		else if(trig_str == "ON_EVERY")
			LinkUpIntegrityTrigger = LinkUpIntegrityTrigger_t::ON_EVERY;
		else
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->error("Invalid LinkUpIntegrityTrigger: {}, should be NEVER, ON_FIRST, or ON_EVERY - defaulting to ON_FIRST", trig_str);
		}
	}
	/// Which classes should be requested for forced integrity scans
	if (JSONRoot.isMember("ForcedIntegrityClass0"))
		ForcedIntegrityClass0 = JSONRoot["ForcedIntegrityClass0"].asBool();
	if (JSONRoot.isMember("ForcedIntegrityClass1"))
		ForcedIntegrityClass1 = JSONRoot["ForcedIntegrityClass1"].asBool();
	if (JSONRoot.isMember("ForcedIntegrityClass2"))
		ForcedIntegrityClass2 = JSONRoot["ForcedIntegrityClass2"].asBool();
	if (JSONRoot.isMember("ForcedIntegrityClass3"))
		ForcedIntegrityClass3 = JSONRoot["ForcedIntegrityClass3"].asBool();
	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected
	if (JSONRoot.isMember("IntegrityOnEventOverflowIIN"))
		IntegrityOnEventOverflowIIN = JSONRoot["IntegrityOnEventOverflowIIN"].asBool();
	/// Choose to ignore DEVICE_RESTART IIN flag. Warning: non compliant behaviour if set to true
	if (JSONRoot.isMember("IgnoreRestartIIN"))
		IgnoreRestartIIN = JSONRoot["IgnoreRestartIIN"].asBool();
	/// Whether to retry a integrity scan that is forced by IIN
	if (JSONRoot.isMember("RetryForcedIntegrity"))
		RetryForcedIntegrity = JSONRoot["RetryForcedIntegrity"].asBool();
	/// Time delay beforce retrying a failed task
	if (JSONRoot.isMember("TaskRetryPeriodms"))
		TaskRetryPeriodms = JSONRoot["TaskRetryPeriodms"].asUInt();

	// Comms Point Configuration
	if (JSONRoot.isMember("CommsPoint"))
	{
		if(JSONRoot["CommsPoint"].isMember("Index") && JSONRoot["CommsPoint"].isMember("FailValue"))
		{
			opendnp3::Flags flags;
			flags.Set(opendnp3::BinaryQuality::ONLINE);
			mCommsPoint.first = opendnp3::Binary(JSONRoot["CommsPoint"]["FailValue"].asBool(), flags);
			mCommsPoint.second = JSONRoot["CommsPoint"]["Index"].asUInt();
		}
		else
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->error("CommsPoint needs an 'Index' and a 'FailValue'.");
		}
		if(JSONRoot["CommsPoint"].isMember("RideThroughTimems"))
			CommsPointRideThroughTimems = JSONRoot["CommsPoint"]["RideThroughTimems"].asUInt();
		if(JSONRoot["CommsPoint"].isMember("HeartBeatTimems"))
			CommsPointHeartBeatTimems = JSONRoot["CommsPoint"]["HeartBeatTimems"].asUInt();
	}

	// Master Station scanning configuration
	if (JSONRoot.isMember("IntegrityScanRatems"))
		IntegrityScanRatems = JSONRoot["IntegrityScanRatems"].asUInt();
	if (JSONRoot.isMember("EventClass1ScanRatems"))
		EventClass1ScanRatems = JSONRoot["EventClass1ScanRatems"].asUInt();
	if (JSONRoot.isMember("EventClass2ScanRatems"))
		EventClass2ScanRatems = JSONRoot["EventClass2ScanRatems"].asUInt();
	if (JSONRoot.isMember("EventClass3ScanRatems"))
		EventClass3ScanRatems = JSONRoot["EventClass3ScanRatems"].asUInt();

	if (JSONRoot.isMember("DoAssignClassOnStartup"))
		DoAssignClassOnStartup = JSONRoot["DoAssignClassOnStartup"].asBool();

	if (JSONRoot.isMember("OverrideControlCode"))
	{
		OverrideControlCode.second = opendnp3::TripCloseCode::NUL;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_ON")
			OverrideControlCode.first = opendnp3::OperationType::PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_OFF")
			OverrideControlCode.first = opendnp3::OperationType::PULSE_OFF;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE")
			OverrideControlCode.first = opendnp3::OperationType::PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "LATCH_OFF")
			OverrideControlCode.first = opendnp3::OperationType::LATCH_OFF;
		if (JSONRoot["OverrideControlCode"].asString() == "LATCH_ON")
			OverrideControlCode.first = opendnp3::OperationType::LATCH_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_CLOSE")
		{
			OverrideControlCode.first = opendnp3::OperationType::PULSE_ON;
			OverrideControlCode.second = opendnp3::TripCloseCode::TRIP;
		}
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_TRIP")
		{
			OverrideControlCode.first = opendnp3::OperationType::PULSE_ON;
			OverrideControlCode.second = opendnp3::TripCloseCode::CLOSE;
		}
		if (JSONRoot["OverrideControlCode"].asString() == "NUL")
			OverrideControlCode.first = opendnp3::OperationType::NUL;
	}

	// Outstation configuration
	if (JSONRoot.isMember("MaxControlsPerRequest"))
		MaxControlsPerRequest = JSONRoot["MaxControlsPerRequest"].asUInt();
	if (JSONRoot.isMember("MaxTxFragSize"))
		MaxTxFragSize = JSONRoot["MaxTxFragSize"].asUInt();
	if (JSONRoot.isMember("SelectTimeoutms"))
		SelectTimeoutms = JSONRoot["SelectTimeoutms"].asUInt();
	if (JSONRoot.isMember("SolConfirmTimeoutms"))
		SolConfirmTimeoutms = JSONRoot["SolConfirmTimeoutms"].asUInt();
	if (JSONRoot.isMember("UnsolConfirmTimeoutms"))
		UnsolConfirmTimeoutms = JSONRoot["UnsolConfirmTimeoutms"].asUInt();
	if (JSONRoot.isMember("WaitForCommandResponses"))
		WaitForCommandResponses = JSONRoot["WaitForCommandResponses"].asBool();
	if (JSONRoot.isMember("TimeSyncOnStart"))
		TimeSyncOnStart = JSONRoot["TimeSyncOnStart"].asBool();
	if (JSONRoot.isMember("TimeSyncPeriodms"))
		TimeSyncPeriodms = JSONRoot["TimeSyncPeriodms"].asUInt64();

	// Default Static Variations
	if (JSONRoot.isMember("StaticBinaryResponse"))
		StaticBinaryResponse = StringToStaticBinaryResponse(JSONRoot["StaticBinaryResponse"].asString());
	if (JSONRoot.isMember("StaticAnalogResponse"))
		StaticAnalogResponse = StringToStaticAnalogResponse(JSONRoot["StaticAnalogResponse"].asString());
	if (JSONRoot.isMember("StaticCounterResponse"))
		StaticCounterResponse = StringToStaticCounterResponse(JSONRoot["StaticCounterResponse"].asString());
	if (JSONRoot.isMember("StaticAnalogOutputStatusResponse"))
		StaticAnalogOutputStatusResponse = StringToStaticAnalogOutputStatusResponse(JSONRoot["StaticAnalogOutputStatusResponse"].asString());
	if (JSONRoot.isMember("StaticBinaryOutputStatusResponse"))
		StaticBinaryOutputStatusResponse = StringToStaticBinaryOutputStatusResponse(JSONRoot["StaticBinaryOutputStatusResponse"].asString());

	// Default Event Variations
	if (JSONRoot.isMember("EventBinaryResponse"))
		EventBinaryResponse = StringToEventBinaryResponse(JSONRoot["EventBinaryResponse"].asString());
	if (JSONRoot.isMember("EventAnalogResponse"))
		EventAnalogResponse = StringToEventAnalogResponse(JSONRoot["EventAnalogResponse"].asString());
	if (JSONRoot.isMember("EventCounterResponse"))
		EventCounterResponse = StringToEventCounterResponse(JSONRoot["EventCounterResponse"].asString());
	if (JSONRoot.isMember("EventBinaryOutputStatusResponse"))
		EventBinaryOutputStatusResponse = StringToEventBinaryOutputStatusResponse(JSONRoot["EventBinaryOutputStatusResponse"].asString());
	if (JSONRoot.isMember("EventAnalogOutputStatusResponse"))
		EventAnalogOutputStatusResponse = StringToEventAnalogOutputStatusResponse(JSONRoot["EventAnalogOutputStatusResponse"].asString());

	//Default Analog Control Type
	if (JSONRoot.isMember("AnalogControlType"))
	{
		AnalogControlType = odc::EventTypeFromString(JSONRoot["AnalogControlType"].asString());
		if(AnalogControlType < odc::EventType::AnalogOutputInt16 || AnalogControlType > odc::EventType::AnalogOutputDouble64)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->error("Invalid AnalogControlType: '{}', should be one of the following: AnalogOutputInt16, AnalogOutputInt32, AnalogOutputFloat32, AnalogOutputDouble64 - defaulting to AnalogOutputInt32", JSONRoot["AnalogControlType"].asString());
			AnalogControlType = odc::EventType::AnalogOutputInt32;
		}
	}

	// Timestamp Override Alternatives
	if (JSONRoot.isMember("TimestampOverride"))
	{
		if (JSONRoot["TimestampOverride"].asString() == "ALWAYS")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::ALWAYS;
		else if (JSONRoot["TimestampOverride"].asString() == "ZERO")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::ZERO;
		else if (JSONRoot["TimestampOverride"].asString() == "NEVER")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::NEVER;
		else
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->error("Invalid TimestampOverride: {}, should be ALWAYS, ZERO, or NEVER - defaulting to ZERO", JSONRoot["TimestampOverride"].asString());
		}
	}

	// Event buffer limits
	auto cap_evenbuffer_size = [&](const std::string& eb_key) -> uint16_t
					   {
						   const auto EBsize = JSONRoot[eb_key].asUInt();
						   if(EBsize > std::numeric_limits<uint16_t>::max())
						   {
							   if(auto log = odc::spdlog_get("DNP3Port"))
								   log->error("Max size for DNP3 event buffer capped. Reduced {} from {} to {}. Set in config to avoid this error.", eb_key, EBsize, std::numeric_limits<uint16_t>::max());
							   return std::numeric_limits<uint16_t>::max();
						   }
						   return EBsize;
					   };

	if (JSONRoot.isMember("MaxBinaryEvents"))
		MaxBinaryEvents = cap_evenbuffer_size("MaxBinaryEvents");
	if (JSONRoot.isMember("MaxAnalogEvents"))
		MaxAnalogEvents = cap_evenbuffer_size("MaxAnalogEvents");
	if (JSONRoot.isMember("MaxCounterEvents"))
		MaxCounterEvents = cap_evenbuffer_size("MaxCounterEvents");
	if (JSONRoot.isMember("MaxOctetStringEvents"))
		MaxOctetStringEvents = cap_evenbuffer_size("MaxOctetStringEvents");

	auto GetIndexRange = [](const auto& PointArrayElement) -> std::tuple<bool,size_t,size_t>
				   {
					   size_t start, stop;
					   if(PointArrayElement.isMember("Index"))
						   start = stop = PointArrayElement["Index"].asUInt();
					   else if(PointArrayElement["Range"].isMember("Start") && PointArrayElement["Range"].isMember("Stop"))
					   {
						   start = PointArrayElement["Range"]["Start"].asUInt();
						   stop = PointArrayElement["Range"]["Stop"].asUInt();
					   }
					   else
					   {
						   if(auto log = odc::spdlog_get("DNP3Port"))
							   log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", PointArrayElement.toStyledString());
						   return {false,0,0};
					   }
					   return {true,start,stop};
				   };

	//TODO: remove stale code from way back when removing a point on-the-fly was a thing
	auto InsertOrDeleteIndex = [](const auto& PointArrayElement, auto& IndexVec, auto index) -> bool
					   {
						   if (std::find(IndexVec.begin(),IndexVec.end(),index) == IndexVec.end())
							   IndexVec.push_back(index);

						   auto start_val = PointArrayElement["StartVal"].asString();
						   if (start_val == "D")
						   {
							   auto it = std::find(IndexVec.begin(),IndexVec.end(),index);
							   IndexVec.erase(it);
							   return false;
						   }
						   return true;
					   };

	//common point configuration
	if(JSONRoot.isMember("Analogs"))
	{
		const auto Analogs = JSONRoot["Analogs"];
		for(Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(Analogs[n]);
			if(!success)
				continue;

			double deadband = 0;
			if(Analogs[n].isMember("Deadband"))
			{
				deadband = Analogs[n]["Deadband"].asDouble();
			}

			for(auto index = start; index <= stop; index++)
			{
				AnalogClasses[index] = GetClass(Analogs[n]);
				if (Analogs[n].isMember("StaticAnalogResponse"))
					StaticAnalogResponses[index] = StringToStaticAnalogResponse(Analogs[n]["StaticAnalogResponse"].asString());
				else
					StaticAnalogResponses[index] = StaticAnalogResponse;
				if (Analogs[n].isMember("EventAnalogResponse"))
					EventAnalogResponses[index] = StringToEventAnalogResponse(Analogs[n]["EventAnalogResponse"].asString());
				else
					EventAnalogResponses[index] = EventAnalogResponse;

				AnalogDeadbands[index] = deadband;

				if(!InsertOrDeleteIndex(Analogs[n],AnalogIndexes,index))
				{
					if(AnalogStartVals.count(index))
						AnalogStartVals.erase(index);
					if(AnalogClasses.count(index))
						AnalogClasses.erase(index);
					StaticAnalogResponses.erase(index);
					EventAnalogResponses.erase(index);
					AnalogDeadbands.erase(index);
					continue;
				}

				if(Analogs[n].isMember("StartVal"))
				{
					opendnp3::Flags flags;
					std::string start_val = Analogs[n]["StartVal"].asString();
					if(start_val == "X")
					{
						flags.Set(opendnp3::AnalogQuality::COMM_LOST);
						AnalogStartVals[index] = opendnp3::Analog(0,flags);
					}
					else
					{
						flags.Set(opendnp3::AnalogQuality::ONLINE);
						AnalogStartVals[index] = opendnp3::Analog(std::stod(start_val),flags);
					}
				}
				else if(AnalogStartVals.count(index))
					AnalogStartVals.erase(index);
			}
		}
		std::sort(AnalogIndexes.begin(),AnalogIndexes.end());
	}

	if(JSONRoot.isMember("Binaries"))
	{
		const auto Binaries = JSONRoot["Binaries"];
		for(Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(Binaries[n]);
			if(!success)
				continue;
			for(auto index = start; index <= stop; index++)
			{
				BinaryClasses[index] = GetClass(Binaries[n]);
				if (Binaries[n].isMember("StaticBinaryResponse"))
					StaticBinaryResponses[index] = StringToStaticBinaryResponse(Binaries[n]["StaticBinaryResponse"].asString());
				else
					StaticBinaryResponses[index] = StaticBinaryResponse;
				if (Binaries[n].isMember("EventBinaryResponse"))
					EventBinaryResponses[index] = StringToEventBinaryResponse(Binaries[n]["EventBinaryResponse"].asString());
				else
					EventBinaryResponses[index] = EventBinaryResponse;

				if(!InsertOrDeleteIndex(Binaries[n],BinaryIndexes,index))
				{
					if(BinaryStartVals.count(index))
						BinaryStartVals.erase(index);
					if(BinaryClasses.count(index))
						BinaryClasses.erase(index);
					StaticBinaryResponses.erase(index);
					EventBinaryResponses.erase(index);
					continue;
				}

				if(Binaries[n].isMember("StartVal"))
				{
					opendnp3::Flags flags;
					std::string start_val = Binaries[n]["StartVal"].asString();
					if(start_val == "X")
					{
						flags.Set(opendnp3::BinaryQuality::COMM_LOST);
						BinaryStartVals[index] = opendnp3::Binary(false,flags);
					}
					else
					{
						flags.Set(opendnp3::BinaryQuality::ONLINE);
						BinaryStartVals[index] = opendnp3::Binary(Binaries[n]["StartVal"].asBool(),flags);
					}
				}
				else if(BinaryStartVals.count(index))
					BinaryStartVals.erase(index);
			}
		}
		std::sort(BinaryIndexes.begin(),BinaryIndexes.end());
	}

	if(JSONRoot.isMember("AnalogOutputStatuses"))
	{
		const auto AnalogOutputStatuses = JSONRoot["AnalogOutputStatuses"];
		for(Json::ArrayIndex n = 0; n < AnalogOutputStatuses.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(AnalogOutputStatuses[n]);
			if(!success)
				continue;

			double deadband = 0;
			if(AnalogOutputStatuses[n].isMember("Deadband"))
			{
				deadband = AnalogOutputStatuses[n]["Deadband"].asDouble();
			}

			for(auto index = start; index <= stop; index++)
			{
				AnalogOutputStatusClasses[index] = GetClass(AnalogOutputStatuses[n]);
				if (AnalogOutputStatuses[n].isMember("StaticAnalogOutputStatusResponse"))
					StaticAnalogOutputStatusResponses[index] = StringToStaticAnalogOutputStatusResponse(AnalogOutputStatuses[n]["StaticAnalogOutputStatusResponse"].asString());
				else
					StaticAnalogOutputStatusResponses[index] = StaticAnalogOutputStatusResponse;
				if (AnalogOutputStatuses[n].isMember("EventAnalogOutputStatusResponse"))
					EventAnalogOutputStatusResponses[index] = StringToEventAnalogOutputStatusResponse(AnalogOutputStatuses[n]["EventAnalogOutputStatusResponse"].asString());
				else
					EventAnalogOutputStatusResponses[index] = EventAnalogOutputStatusResponse;

				//TODO: make deadbands per point
				AnalogOutputStatusDeadbands[index] = deadband;

				if (std::find(AnalogOutputStatusIndexes.begin(),AnalogOutputStatusIndexes.end(),index) == AnalogOutputStatusIndexes.end())
					AnalogOutputStatusIndexes.push_back(index);

				if(AnalogOutputStatuses[n].isMember("StartVal"))
				{
					opendnp3::Flags flags;
					std::string start_val = AnalogOutputStatuses[n]["StartVal"].asString();
					if(start_val == "X")
					{
						flags.Set(opendnp3::AnalogOutputStatusQuality::COMM_LOST);
						AnalogOutputStatusStartVals[index] = opendnp3::AnalogOutputStatus(0,flags);
					}
					else
					{
						flags.Set(opendnp3::AnalogOutputStatusQuality::ONLINE);
						AnalogOutputStatusStartVals[index] = opendnp3::AnalogOutputStatus(std::stod(start_val),flags);
					}
				}
				else if(AnalogOutputStatusStartVals.count(index))
					AnalogOutputStatusStartVals.erase(index);
			}
		}
		std::sort(AnalogOutputStatusIndexes.begin(),AnalogOutputStatusIndexes.end());
	}

	if(JSONRoot.isMember("BinaryOutputStatuses"))
	{
		const auto BinaryOutputStatuses = JSONRoot["BinaryOutputStatuses"];
		for(Json::ArrayIndex n = 0; n < BinaryOutputStatuses.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(BinaryOutputStatuses[n]);
			if(!success)
				continue;
			for(auto index = start; index <= stop; index++)
			{
				BinaryOutputStatusClasses[index] = GetClass(BinaryOutputStatuses[n]);
				if (BinaryOutputStatuses[n].isMember("StaticBinaryOutputStatusResponse"))
					StaticBinaryOutputStatusResponses[index] = StringToStaticBinaryOutputStatusResponse(BinaryOutputStatuses[n]["StaticBinaryOutputStatusResponse"].asString());
				else
					StaticBinaryOutputStatusResponses[index] = StaticBinaryOutputStatusResponse;
				if (BinaryOutputStatuses[n].isMember("EventBinaryOutputStatusResponse"))
					EventBinaryOutputStatusResponses[index] = StringToEventBinaryOutputStatusResponse(BinaryOutputStatuses[n]["EventBinaryOutputStatusResponse"].asString());
				else
					EventBinaryOutputStatusResponses[index] = EventBinaryOutputStatusResponse;

				if (std::find(BinaryOutputStatusIndexes.begin(),BinaryOutputStatusIndexes.end(),index) == BinaryOutputStatusIndexes.end())
					BinaryOutputStatusIndexes.push_back(index);

				if(BinaryOutputStatuses[n].isMember("StartVal"))
				{
					opendnp3::Flags flags;
					std::string start_val = BinaryOutputStatuses[n]["StartVal"].asString();
					if(start_val == "X")
					{
						flags.Set(opendnp3::BinaryQuality::COMM_LOST);
						BinaryOutputStatusStartVals[index] = opendnp3::BinaryOutputStatus(false,flags);
					}
					else
					{
						flags.Set(opendnp3::BinaryQuality::ONLINE);
						BinaryOutputStatusStartVals[index] = opendnp3::BinaryOutputStatus(BinaryOutputStatuses[n]["StartVal"].asBool(),flags);
					}
				}
				else if(BinaryOutputStatusStartVals.count(index))
					BinaryOutputStatusStartVals.erase(index);
			}
		}
		std::sort(BinaryOutputStatusIndexes.begin(),BinaryOutputStatusIndexes.end());
	}

	if (JSONRoot.isMember("OctetStrings"))
	{
		const auto OctetStrings = JSONRoot["OctetStrings"];
		for (Json::ArrayIndex n = 0; n < OctetStrings.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(OctetStrings[n]);
			if(!success)
				continue;
			for (auto index = start; index <= stop; index++)
			{
				if(!InsertOrDeleteIndex(OctetStrings[n],OctetStringIndexes,index))
					continue;
				OctetStringClasses[index] = GetClass(OctetStrings[n]);
			}
		}
		std::sort(OctetStringIndexes.begin(), OctetStringIndexes.end());
	}

	if(JSONRoot.isMember("BinaryControls"))
	{
		const auto BinaryControls= JSONRoot["BinaryControls"];
		for(Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(BinaryControls[n]);
			if(!success)
				continue;
			for(auto index = start; index <= stop; index++)
				InsertOrDeleteIndex(BinaryControls[n],ControlIndexes,index);
		}
		std::sort(ControlIndexes.begin(),ControlIndexes.end());
	}

	if (JSONRoot.isMember("AnalogControls"))
	{
		const auto AnalogControls = JSONRoot["AnalogControls"];
		for (Json::ArrayIndex n = 0; n < AnalogControls.size(); ++n)
		{
			auto [success, start, stop] = GetIndexRange(AnalogControls[n]);
			if(!success)
				continue;
			for (auto index = start; index <= stop; index++)
			{
				if (AnalogControls[n].isMember("Type"))
				{
					AnalogControlTypes[index] = odc::EventTypeFromString(AnalogControls[n]["Type"].asString());
					if(AnalogControlTypes[index] < odc::EventType::AnalogOutputInt16 || AnalogControlTypes[index] > odc::EventType::AnalogOutputDouble64)
					{
						if(auto log = odc::spdlog_get("DNP3Port"))
							log->error("Invalid AnalogControl Type: '{}', should be one of the following: AnalogOutputInt16, AnalogOutputInt32, AnalogOutputFloat32, AnalogOutputDouble64 - falling back to port default {}", AnalogControls[n]["Type"].asString(),ToString(AnalogControlType));
						AnalogControlTypes[index] = AnalogControlType;
					}
				}
				else
					AnalogControlTypes[index] = AnalogControlType;

				if(!InsertOrDeleteIndex(AnalogControls[n], AnalogControlIndexes, index))
					AnalogControlTypes.erase(index);
			}
		}
		std::sort(AnalogControlIndexes.begin(), AnalogControlIndexes.end());
	}
}



