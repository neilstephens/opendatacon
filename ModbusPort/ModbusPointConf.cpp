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
 * ModbusPointConf.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <regex>
#include <algorithm>
#include "ModbusPointConf.h"
#include <opendatacon/util.h>

ModbusPointConf::ModbusPointConf(std::string FileName):
	ConfigParser(FileName)
{
	ProcessFile();
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

template<class T>
T GetStartVal(const Json::Value& value);

template<>
opendnp3::Analog GetStartVal<opendnp3::Analog>(const Json::Value& value)
{
    opendnp3::Analog startval;

    std::string start_val = value.asString();
    if(start_val == "X")
        return opendnp3::Analog(0,static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST));
    else
        return opendnp3::Analog(std::stod(start_val),static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE));
}

template<>
opendnp3::Binary GetStartVal<opendnp3::Binary>(const Json::Value& value)
{
    opendnp3::Binary startval;
    
    if(value.asString() == "X")
        return opendnp3::Binary(false,static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST));
    else
        return opendnp3::Binary(value.asBool(),static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE));
}

template<class T>
void ModbusPointConf::ProcessReadGroup(const Json::Value& Ranges, ModbusReadGroupCollection<T>& ReadGroup)
{
    for(Json::ArrayIndex n = 0; n < Ranges.size(); ++n)
    {
        uint32_t pollgroup = 0;
        if(!Ranges[n]["PollGroup"].isNull())
        {
            pollgroup = Ranges[n]["PollGroup"].asUInt();
        }
        
        T startval;
        if(!Ranges[n]["StartVal"].isNull())
        {
            startval = GetStartVal<T>(Ranges[n]["StartVal"]);
        }
        
        size_t start, stop;
        if(!Ranges[n]["Index"].isNull())
            start = stop = Ranges[n]["Index"].asUInt();
        else if(!Ranges[n]["Range"]["Start"].isNull() && !Ranges[n]["Range"]["Stop"].isNull())
        {
            start = Ranges[n]["Range"]["Start"].asUInt();
            stop = Ranges[n]["Range"]["Stop"].asUInt();
            //TODO: propper error logging
            if (start > stop)
            {
                std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
                continue;
            }
        }
        else
        {
            std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
            start = 1;
            stop = 0;
            continue;
        }
        
        ReadGroup.emplace_back(start,stop-start+1,pollgroup,startval);
    }
}

void ModbusPointConf::ProcessElements(const Json::Value& JSONRoot)
{
    if(!JSONRoot.isObject()) return;
    
    if(!JSONRoot["BitIndicies"].isNull())
        ProcessReadGroup(JSONRoot["BitIndicies"], BitIndicies);
    if(!JSONRoot["InputBitIndicies"].isNull())
        ProcessReadGroup(JSONRoot["InputBitIndicies"], InputBitIndicies);
    if(!JSONRoot["RegIndicies"].isNull())
        ProcessReadGroup(JSONRoot["RegIndicies"], RegIndicies);
    if(!JSONRoot["InputRegIndicies"].isNull())
        ProcessReadGroup(JSONRoot["InputRegIndicies"], InputRegIndicies);

    if(!JSONRoot["PollGroups"].isNull())
    {
        auto jPollGroups = JSONRoot["PollGroups"];
        for(Json::ArrayIndex n = 0; n < jPollGroups.size(); ++n)
        {
            if(jPollGroups[n]["ID"].isNull())
            {
                std::cout<<"Poll group missing ID : '"<<jPollGroups[n].toStyledString()<<"'"<<std::endl;
                continue;
            }
            if(jPollGroups[n]["PollRate"].isNull())
            {
                std::cout<<"Poll group missing PollRate : '"<<jPollGroups[n].toStyledString()<<"'"<<std::endl;
                continue;
            }

            uint32_t PollGroupID = jPollGroups[n]["ID"].asUInt();
            uint32_t pollrate = jPollGroups[n]["PollRate"].asUInt();
            
            if(PollGroupID == 0)
            {
                std::cout<<"Poll group 0 is reserved (do not poll) : '"<<jPollGroups[n].toStyledString()<<"'"<<std::endl;
                continue;
            }

            if(PollGroups.count(PollGroupID) > 0)
            {
                std::cout<<"Duplicate poll group ignored : '"<<jPollGroups[n].toStyledString()<<"'"<<std::endl;
                continue;
            }
            
            PollGroups.emplace(std::piecewise_construct,std::forward_as_tuple(PollGroupID),std::forward_as_tuple(PollGroupID,pollrate));
        }
    }
}



