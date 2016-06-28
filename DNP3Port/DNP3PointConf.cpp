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

#include <regex>
#include <algorithm>
#include <opendnp3/app/ClassField.h>
#include "DNP3PointConf.h"
#include "OpenDNP3Helpers.h"
#include <iostream> // TODO: remove include, should be met using logging mechanism


DNP3PointConf::DNP3PointConf(std::string FileName):
	ConfigParser(FileName),
	// DNP3 Link Configuration
	LinkNumRetry(0),
	LinkTimeoutms(1000),
	LinkKeepAlivems(10000),
	LinkUseConfirms(false),
	// Common application stack configuration
	EnableUnsol(true),
	UnsolClass1(false),
	UnsolClass2(false),
	UnsolClass3(false),
	// Master Station configuration
	TCPConnectRetryPeriodMinms(500),
	TCPConnectRetryPeriodMaxms(30000),
	MasterResponseTimeoutms(5000), /// Application layer response timeout
	MasterRespondTimeSync(true),   /// If true, the master will do time syncs when it sees the time IIN bit from the outstation
	DoUnsolOnStartup(true),
	/// Which classes should be requested in a startup integrity scan
	StartupIntegrityClass0(true),
	StartupIntegrityClass1(true),
	StartupIntegrityClass2(true),
	StartupIntegrityClass3(true),
	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected
	IntegrityOnEventOverflowIIN(true),
	/// Time delay beforce retrying a failed task
	TaskRetryPeriodms(5000),
	// Master Station scanning configuration
	IntegrityScanRatems(3600000),
	EventClass1ScanRatems(1000),
	EventClass2ScanRatems(1000),
	EventClass3ScanRatems(1000),
	OverrideControlCode(opendnp3::ControlCode::UNDEFINED),
	DoAssignClassOnStartup(false),
	// Outstation configuration
	TCPListenRetryPeriodMinms(0),
	TCPListenRetryPeriodMaxms(5000),
	MaxControlsPerRequest(16),
	MaxTxFragSize(2048),
	SelectTimeoutms(10000),
	SolConfirmTimeoutms(5000),
	UnsolConfirmTimeoutms(5000),
	WaitForCommandResponses(false),
	// Default Static Variations
	StaticBinaryResponse(opendnp3::Binary::StaticVariation::Group1Var1),
	StaticAnalogResponse(opendnp3::Analog::StaticVariation::Group30Var5),
	StaticCounterResponse(opendnp3::Counter::StaticVariation::Group20Var1),
	// Default Event Variations
	EventBinaryResponse(opendnp3::Binary::EventVariation::Group2Var1),
	EventAnalogResponse(opendnp3::Analog::EventVariation::Group32Var5),
	EventCounterResponse(opendnp3::Counter::EventVariation::Group22Var1),
	TimestampOverride(ZERO),
	// Event buffer limits
	MaxBinaryEvents(1000),
	MaxAnalogEvents(1000),
	MaxCounterEvents(1000)
{
	ProcessFile();
}

uint8_t DNP3PointConf::GetUnsolClassMask()
{
	uint8_t class_mask = 0;
	class_mask += (UnsolClass1 ? opendnp3::ClassField::CLASS_1 : 0);
	class_mask += (UnsolClass2 ? opendnp3::ClassField::CLASS_2 : 0);
	class_mask += (UnsolClass3 ? opendnp3::ClassField::CLASS_3 : 0);
	return class_mask;
}

uint8_t DNP3PointConf::GetStartupIntegrityClassMask()
{
	uint8_t class_mask = 0;
	class_mask += (StartupIntegrityClass0 ? opendnp3::ClassField::CLASS_0 : 0);
	class_mask += (StartupIntegrityClass1 ? opendnp3::ClassField::CLASS_1 : 0);
	class_mask += (StartupIntegrityClass2 ? opendnp3::ClassField::CLASS_2 : 0);
	class_mask += (StartupIntegrityClass3 ? opendnp3::ClassField::CLASS_3 : 0);
	return class_mask;
}

opendnp3::PointClass GetClass(Json::Value JPoint)
{
	opendnp3::PointClass clazz = opendnp3::PointClass::Class1;
	if(!JPoint["Class"].isNull())
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
					std::cout<<"Invalid class for Point: '"<<JPoint.toStyledString()<<"'"<<std::endl;
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
	if (!JSONRoot["LinkNumRetry"].isNull())
		LinkNumRetry = JSONRoot["LinkNumRetry"].asUInt();
	if (!JSONRoot["LinkTimeoutms"].isNull())
		LinkTimeoutms = JSONRoot["LinkTimeoutms"].asUInt();
	if (!JSONRoot["LinkKeepAlivems"].isNull())
		LinkKeepAlivems = JSONRoot["LinkKeepAlivems"].asUInt();
	if (!JSONRoot["LinkUseConfirms"].isNull())
		LinkUseConfirms = JSONRoot["LinkUseConfirms"].asBool();
	if (!JSONRoot["UseConfirms"].isNull())
		std::cout << "Use of 'UseConfirms' is deprecated, use 'LinkUseConfirms' instead : '" << JSONRoot["UseConfirms"].toStyledString() << "'" << std::endl;

	// Common application configuration
	if (!JSONRoot["EnableUnsol"].isNull())
		EnableUnsol = JSONRoot["EnableUnsol"].asBool();
	if (!JSONRoot["UnsolClass1"].isNull())
		UnsolClass1 = JSONRoot["UnsolClass1"].asBool();
	if (!JSONRoot["UnsolClass2"].isNull())
		UnsolClass2 = JSONRoot["UnsolClass2"].asBool();
	if (!JSONRoot["UnsolClass3"].isNull())
		UnsolClass3 = JSONRoot["UnsolClass3"].asBool();

	// Master Station configuration
	if (!JSONRoot["TCPConnectRetryPeriodMinms"].isNull())
		TCPConnectRetryPeriodMinms = JSONRoot["TCPConnectRetryPeriodMinms"].asUInt();
	if (!JSONRoot["TCPConnectRetryPeriodMaxms"].isNull())
		TCPConnectRetryPeriodMaxms = JSONRoot["TCPConnectRetryPeriodMaxms"].asUInt();
	if (!JSONRoot["MasterResponseTimeoutms"].isNull())
		MasterResponseTimeoutms = JSONRoot["MasterResponseTimeoutms"].asUInt();
	if (!JSONRoot["MasterRespondTimeSync"].isNull())
		MasterRespondTimeSync = JSONRoot["MasterRespondTimeSync"].asBool();
	if (!JSONRoot["DoUnsolOnStartup"].isNull())
		DoUnsolOnStartup = JSONRoot["DoUnsolOnStartup"].asBool();

	/// Which classes should be requested in a startup integrity scan
	if (!JSONRoot["StartupIntegrityClass0"].isNull())
		StartupIntegrityClass0 = JSONRoot["StartupIntegrityClass0"].asBool();
	if (!JSONRoot["StartupIntegrityClass1"].isNull())
		StartupIntegrityClass1 = JSONRoot["StartupIntegrityClass1"].asBool();
	if (!JSONRoot["StartupIntegrityClass2"].isNull())
		StartupIntegrityClass2 = JSONRoot["StartupIntegrityClass2"].asBool();
	if (!JSONRoot["StartupIntegrityClass3"].isNull())
		StartupIntegrityClass3 = JSONRoot["StartupIntegrityClass3"].asBool();

	/// Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected
	if (!JSONRoot["IntegrityOnEventOverflowIIN"].isNull())
		IntegrityOnEventOverflowIIN = JSONRoot["IntegrityOnEventOverflowIIN"].asBool();
	/// Time delay beforce retrying a failed task
	if (!JSONRoot["TaskRetryPeriodms"].isNull())
		TaskRetryPeriodms = JSONRoot["TaskRetryPeriodms"].asUInt();

	// Comms Point Configuration
	if (JSONRoot["CommsPoint"].isNull() || JSONRoot["CommsPoint"]["Index"].isNull())
		mCommsPoint.first = opendnp3::Binary(false, static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST));
	else
	{
		mCommsPoint.first = opendnp3::Binary(JSONRoot["CommsPoint"]["FailValue"].asBool(), static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE));
		mCommsPoint.second = JSONRoot["CommsPoint"]["Index"].asUInt();
	}

	// Master Station scanning configuration
	if (!JSONRoot["IntegrityScanRatems"].isNull())
		IntegrityScanRatems = JSONRoot["IntegrityScanRatems"].asUInt();
	if (!JSONRoot["IntegrityScanRateSec"].isNull())
		std::cout << "Use of 'IntegrityScanRateSec' is deprecated, use 'IntegrityScanRatems' instead : '" << JSONRoot["IntegrityScanRateSec"].toStyledString() << "'" << std::endl;
	if (!JSONRoot["EventClass1ScanRatems"].isNull())
		EventClass1ScanRatems = JSONRoot["EventClass1ScanRatems"].asUInt();
	if (!JSONRoot["EventClass1ScanRateSec"].isNull())
		std::cout << "Use of 'EventClass1ScanRateSec' is deprecated, use 'EventClass1ScanRatems' instead : '" << JSONRoot["EventClass1ScanRateSec"].toStyledString() << "'" << std::endl;
	if (!JSONRoot["EventClass2ScanRatems"].isNull())
		EventClass2ScanRatems = JSONRoot["EventClass2ScanRatems"].asUInt();
	if (!JSONRoot["EventClass2ScanRateSec"].isNull())
		std::cout << "Use of 'EventClass2ScanRateSec' is deprecated, use 'EventClass2ScanRatems' instead : '" << JSONRoot["EventClass2ScanRateSec"].toStyledString() << "'" << std::endl;
	if (!JSONRoot["EventClass3ScanRatems"].isNull())
		EventClass3ScanRatems = JSONRoot["EventClass3ScanRatems"].asUInt();
	if (!JSONRoot["EventClass3ScanRateSec"].isNull())
		std::cout << "Use of 'EventClass3ScanRateSec' is deprecated, use 'EventClass3ScanRatems' instead : '" << JSONRoot["EventClass3ScanRateSec"].toStyledString() << "'" << std::endl;

	if (!JSONRoot["DoAssignClassOnStartup"].isNull())
		DoAssignClassOnStartup = JSONRoot["DoAssignClassOnStartup"].asBool();

	if (!JSONRoot["OverrideControlCode"].isNull())
	{
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_ON")
			OverrideControlCode = opendnp3::ControlCode::PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_OFF")
			OverrideControlCode = opendnp3::ControlCode::PULSE_OFF;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE")
			OverrideControlCode = opendnp3::ControlCode::PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "LATCH_OFF")
			OverrideControlCode = opendnp3::ControlCode::LATCH_OFF;
		if (JSONRoot["OverrideControlCode"].asString() == "LATCH_ON")
			OverrideControlCode = opendnp3::ControlCode::LATCH_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_CLOSE")
			OverrideControlCode = opendnp3::ControlCode::CLOSE_PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "PULSE_TRIP")
			OverrideControlCode = opendnp3::ControlCode::TRIP_PULSE_ON;
		if (JSONRoot["OverrideControlCode"].asString() == "NUL")
			OverrideControlCode = opendnp3::ControlCode::NUL;
	}

	// Outstation configuration
	if (!JSONRoot["TCPListenRetryPeriodMinms"].isNull())
		TCPListenRetryPeriodMinms = JSONRoot["TCPListenRetryPeriodMinms"].asUInt();
	if (!JSONRoot["TCPListenRetryPeriodMaxms"].isNull())
		TCPListenRetryPeriodMaxms = JSONRoot["TCPListenRetryPeriodMaxms"].asUInt();
	if (!JSONRoot["MaxControlsPerRequest"].isNull())
		MaxControlsPerRequest = JSONRoot["MaxControlsPerRequest"].asUInt();
	if (!JSONRoot["MaxTxFragSize"].isNull())
		MaxTxFragSize = JSONRoot["MaxTxFragSize"].asUInt();
	if (!JSONRoot["SelectTimeoutms"].isNull())
		SelectTimeoutms = JSONRoot["SelectTimeoutms"].asUInt();
	if (!JSONRoot["SolConfirmTimeoutms"].isNull())
		SolConfirmTimeoutms = JSONRoot["SolConfirmTimeoutms"].asUInt();
	if (!JSONRoot["UnsolConfirmTimeoutms"].isNull())
		UnsolConfirmTimeoutms = JSONRoot["UnsolConfirmTimeoutms"].asUInt();
	if (!JSONRoot["WaitForCommandResponses"].isNull())
		WaitForCommandResponses = JSONRoot["WaitForCommandResponses"].asBool();

	// Default Static Variations
	if (!JSONRoot["StaticBinaryResponse"].isNull())
		StaticBinaryResponse = StringToStaticBinaryResponse(JSONRoot["StaticBinaryResponse"].asString());
	if (!JSONRoot["StaticAnalogResponse"].isNull())
		StaticAnalogResponse = StringToStaticAnalogResponse(JSONRoot["StaticAnalogResponse"].asString());
	if (!JSONRoot["StaticCounterResponse"].isNull())
		StaticCounterResponse = StringToStaticCounterResponse(JSONRoot["StaticCounterResponse"].asString());

	// Default Event Variations
	if (!JSONRoot["EventBinaryResponse"].isNull())
		EventBinaryResponse = StringToEventBinaryResponse(JSONRoot["EventBinaryResponse"].asString());
	if (!JSONRoot["EventAnalogResponse"].isNull())
		EventAnalogResponse = StringToEventAnalogResponse(JSONRoot["EventAnalogResponse"].asString());
	if (!JSONRoot["EventCounterResponse"].isNull())
		EventCounterResponse = StringToEventCounterResponse(JSONRoot["EventCounterResponse"].asString());

	// Timestamp Override Alternatives
	if (!JSONRoot["TimestampOverride"].isNull())
	{
		if (JSONRoot["TimestampOverride"].asString() == "ALWAYS")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::ALWAYS;
		else if (JSONRoot["TimestampOverride"].asString() == "ZERO")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::ZERO;
		else if (JSONRoot["TimestampOverride"].asString() == "NEVER")
			TimestampOverride = DNP3PointConf::TimestampOverride_t::NEVER;
		else
			std::cout << "Invalid TimestampOverride: " << JSONRoot["TimestampOverride"].asString() << ", should be ALWAYS, ZERO, or NEVER - defaulting to ZERO" << std::endl;
	}

	// Event buffer limits
	if (!JSONRoot["MaxBinaryEvents"].isNull())
		MaxBinaryEvents = JSONRoot["MaxBinaryEvents"].asUInt();
	if (!JSONRoot["MaxAnalogEvents"].isNull())
		MaxAnalogEvents = JSONRoot["MaxAnalogEvents"].asUInt();
	if (!JSONRoot["MaxCounterEvents"].isNull())
		MaxCounterEvents = JSONRoot["MaxCounterEvents"].asUInt();

	//common point configuration
	if(!JSONRoot["Analogs"].isNull())
	{
		const auto Analogs = JSONRoot["Analogs"];
		for(Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
		{
			double deadband = 0;
			if(!Analogs[n]["Deadband"].isNull())
			{
				deadband = Analogs[n]["Deadband"].asDouble();
			}
			opendnp3::PointClass clazz = GetClass(Analogs[n]);
			size_t start, stop;
			if(!Analogs[n]["Index"].isNull())
				start = stop = Analogs[n]["Index"].asUInt();
			else if(!Analogs[n]["Range"]["Start"].isNull() && !Analogs[n]["Range"]["Stop"].isNull())
			{
				start = Analogs[n]["Range"]["Start"].asUInt();
				stop = Analogs[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Analogs[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{
				bool exists = false;
				for(auto existing_index : AnalogIndicies)
					if(existing_index == index)
						exists = true;

				AnalogClasses[index] = clazz;
				AnalogDeadbands[index] = deadband;

				if(!exists)
					AnalogIndicies.push_back(index);

				if(!Analogs[n]["StartVal"].isNull())
				{
					std::string start_val = Analogs[n]["StartVal"].asString();
					if(start_val == "D") //delete this index
					{
						if(AnalogStartVals.count(index))
							AnalogStartVals.erase(index);
						if(AnalogClasses.count(index))
							AnalogClasses.erase(index);
						for(auto it = AnalogIndicies.begin(); it != AnalogIndicies.end(); it++)
							if(*it == index)
							{
								AnalogIndicies.erase(it);
								break;
							}
					}
					else if(start_val == "X")
						AnalogStartVals[index] = opendnp3::Analog(0,static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST));
					else
						AnalogStartVals[index] = opendnp3::Analog(std::stod(start_val),static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE));
				}
				else if(AnalogStartVals.count(index))
					AnalogStartVals.erase(index);
			}
		}
		std::sort(AnalogIndicies.begin(),AnalogIndicies.end());
	}

	if(!JSONRoot["Binaries"].isNull())
	{
		const auto Binaries = JSONRoot["Binaries"];
		for(Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
		{
			opendnp3::PointClass clazz = GetClass(Binaries[n]);
			size_t start, stop;
			if(!Binaries[n]["Index"].isNull())
				start = stop = Binaries[n]["Index"].asUInt();
			else if(!Binaries[n]["Range"]["Start"].isNull() && !Binaries[n]["Range"]["Stop"].isNull())
			{
				start = Binaries[n]["Range"]["Start"].asUInt();
				stop = Binaries[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Binaries[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{

				bool exists = false;
				for(auto existing_index : BinaryIndicies)
					if(existing_index == index)
						exists = true;

				BinaryClasses[index] = clazz;

				if(!exists)
					BinaryIndicies.push_back(index);

				if(!Binaries[n]["StartVal"].isNull())
				{
					std::string start_val = Binaries[n]["StartVal"].asString();
					if(start_val == "D") //delete this index
					{
						if(BinaryStartVals.count(index))
							BinaryStartVals.erase(index);
						if(BinaryClasses.count(index))
							BinaryClasses.erase(index);
						for(auto it = BinaryIndicies.begin(); it != BinaryIndicies.end(); it++)
							if(*it == index)
							{
								BinaryIndicies.erase(it);
								break;
							}
					}
					else if(start_val == "X")
						BinaryStartVals[index] = opendnp3::Binary(false,static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST));
					else
						BinaryStartVals[index] = opendnp3::Binary(Binaries[n]["StartVal"].asBool(),static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE));
				}
				else if(BinaryStartVals.count(index))
					BinaryStartVals.erase(index);
			}
		}
		std::sort(BinaryIndicies.begin(),BinaryIndicies.end());
	}

	if(!JSONRoot["BinaryControls"].isNull())
	{
		const auto BinaryControls= JSONRoot["BinaryControls"];
		for(Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
		{
			size_t start, stop;
			if(!BinaryControls[n]["Index"].isNull())
				start = stop = BinaryControls[n]["Index"].asUInt();
			else if(!BinaryControls[n]["Range"]["Start"].isNull() && !BinaryControls[n]["Range"]["Stop"].isNull())
			{
				start = BinaryControls[n]["Range"]["Start"].asUInt();
				stop = BinaryControls[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<BinaryControls[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{

				bool exists = false;
				for(auto existing_index : ControlIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					ControlIndicies.push_back(index);

				auto start_val = BinaryControls[n]["StartVal"].asString();
				if(start_val == "D")
				{
					for(auto it = ControlIndicies.begin(); it != ControlIndicies.end(); it++)
						if(*it == index)
						{
							ControlIndicies.erase(it);
							break;
						}
				}
			}
		}
		std::sort(ControlIndicies.begin(),ControlIndicies.end());
	}
}



