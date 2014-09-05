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
#include <opendatacon/util.h>

DNP3PointConf::DNP3PointConf(std::string FileName):
	ConfigParser(FileName),
		EnableUnsol(true),
		UnsolClass1(false),
		UnsolClass2(false),
		UnsolClass3(false),
		EventBinaryResponse(opendnp3::EventBinaryResponse::Group2Var1),
		EventAnalogResponse(opendnp3::EventAnalogResponse::Group32Var5),
		EventCounterResponse(opendnp3::EventCounterResponse::Group22Var1),
		DoUnsolOnStartup(true),
		DoAssignClassOnStartup(true),
		UseConfirms(true),
		IntegrityScanRateSec(3600),
		EventClass1ScanRateSec(1),
		EventClass2ScanRateSec(1),
		EventClass3ScanRateSec(1)
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
	if(!JSONRoot["EventClass3ScanRateSec"].isNull())
		EventClass3ScanRateSec = JSONRoot["EventClass3ScanRateSec"].asUInt();

	if(JSONRoot["CommsPoint"].isNull() || JSONRoot["CommsPoint"]["Index"].isNull())
		mCommsPoint.first = opendnp3::Binary(false,static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST));
	else
	{
		mCommsPoint.first = opendnp3::Binary(JSONRoot["CommsPoint"]["FailValue"].asBool(),static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE));
		mCommsPoint.second = JSONRoot["CommsPoint"]["Index"].asBool();
	}

	if(!JSONRoot["EventClass2ScanRateSec"].isNull())
		EventClass2ScanRateSec = JSONRoot["EventClass2ScanRateSec"].asUInt();

	if(!JSONRoot["EventClass1ScanRateSec"].isNull())
		EventClass1ScanRateSec = JSONRoot["EventClass1ScanRateSec"].asUInt();

	if(!JSONRoot["IntegrityScanRateSec"].isNull())
		IntegrityScanRateSec = JSONRoot["IntegrityScanRateSec"].asUInt();

	if(!JSONRoot["EnableUnsol"].isNull())
		EnableUnsol = JSONRoot["EnableUnsol"].asBool();

	if(!JSONRoot["UnsolClass1"].isNull())
		UnsolClass1 = JSONRoot["UnsolClass1"].asBool();

	if(!JSONRoot["UnsolClass2"].isNull())
		UnsolClass2 = JSONRoot["UnsolClass2"].asBool();

	if(!JSONRoot["UnsolClass3"].isNull())
		UnsolClass3 = JSONRoot["UnsolClass3"].asBool();

	if(!JSONRoot["EventBinaryResponse"].isNull())
		EventBinaryResponse = StringToEventBinaryResponse(JSONRoot["EventBinaryResponse"].asString());

	if(!JSONRoot["EventAnalogResponse"].isNull())
		EventAnalogResponse = StringToEventAnalogResponse(JSONRoot["EventAnalogResponse"].asString());

	if(!JSONRoot["EventCounterResponse"].isNull())
		EventCounterResponse = StringToEventCounterResponse(JSONRoot["EventCounterResponse"].asString());

	if(!JSONRoot["DoUnsolOnStartup"].isNull())
		DoUnsolOnStartup = JSONRoot["DoUnsolOnStartup"].asBool();

	if(!JSONRoot["DoAssignClassOnStartup"].isNull())
		DoAssignClassOnStartup = JSONRoot["DoAssignClassOnStartup"].asBool();

	if(!JSONRoot["UseConfirms"].isNull())
		UseConfirms = JSONRoot["UseConfirms"].asBool();

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
				for(auto existing_index: AnalogIndicies)
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
				for(auto existing_index: BinaryIndicies)
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
				for(auto existing_index: ControlIndicies)
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



