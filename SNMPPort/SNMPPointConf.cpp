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
 * SNMPPointConf.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <regex>
#include <string.h>
#include <algorithm>
#include "SNMPPointConf.h"
#include <opendatacon/util.h>

SNMPPointConf::SNMPPointConf(std::string FileName):
	ConfigParser(FileName)
{
	ProcessFile();
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

template<>
void SNMPPointConf::ProcessReadGroup(const Json::Value& Ranges, std::vector<std::shared_ptr<OidToBinaryEvent>>& ReadGroup)
{
	for(Json::ArrayIndex n = 0; n < Ranges.size(); ++n)
	{
		uint32_t pollgroup = 0;
		if(!Ranges[n]["PollGroup"].isNull())
		{
			pollgroup = Ranges[n]["PollGroup"].asUInt();
		}

		opendnp3::Binary startval;
		if(!Ranges[n]["StartVal"].isNull())
		{
			startval = GetStartVal<opendnp3::Binary>(Ranges[n]["StartVal"]);
		}

		size_t idx = 0;
		if(!Ranges[n]["Index"].isNull())
			idx = Ranges[n]["Index"].asInt();

		std::string trueVal = "";
		if(!Ranges[n]["TrueVal"].isNull())
			trueVal = Ranges[n]["TrueVal"].asString();

		std::string falseVal = "";
		if(!Ranges[n]["FalseVal"].isNull())
			falseVal = Ranges[n]["FalseVal"].asString();
		
		Snmp_pp::Oid oid;
		if(!Ranges[n]["Oid"].isNull())
		{
			oid = Ranges[n]["Oid"].asCString();
			if (!oid.valid())            // check validity of user oid
			{
				std::cout<<"Invalid Oid : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
				continue;
			}
		}
		else
		{
			std::cout<<"A point needs an \"Oid\" : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
			continue;
		}

		std::shared_ptr<OidToBinaryEvent> obe(new OidToBinaryEvent(oid,idx,pollgroup,startval,trueVal,falseVal));
		ReadGroup.emplace_back(obe);
		OidMap.emplace(oid,obe);
	}
}

template<>
void SNMPPointConf::ProcessReadGroup(const Json::Value& Ranges, std::vector<std::shared_ptr<OidToAnalogEvent>>& ReadGroup)
{
	for(Json::ArrayIndex n = 0; n < Ranges.size(); ++n)
	{
		uint32_t pollgroup = 0;
		if(!Ranges[n]["PollGroup"].isNull())
		{
			pollgroup = Ranges[n]["PollGroup"].asUInt();
		}
		
		opendnp3::Analog startval;
		if(!Ranges[n]["StartVal"].isNull())
		{
			startval = GetStartVal<opendnp3::Analog>(Ranges[n]["StartVal"]);
		}
		
		size_t idx = 0;
		if(!Ranges[n]["Index"].isNull())
			idx = Ranges[n]["Index"].asInt();
		
		Snmp_pp::Oid oid;
		if(!Ranges[n]["Oid"].isNull())
		{
			oid = Ranges[n]["Oid"].asCString();
			if (!oid.valid())            // check validity of user oid
			{
				std::cout<<"Invalid Oid : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
				continue;
			}
		}
		else
		{
			std::cout<<"A point needs an \"Oid\" : '"<<Ranges[n].toStyledString()<<"'"<<std::endl;
			continue;
		}
		
		std::shared_ptr<OidToAnalogEvent> oae(new OidToAnalogEvent(oid,idx,pollgroup,startval));
		ReadGroup.emplace_back(oae);
		OidMap.emplace(oid, oae);
	}
}

void SNMPPointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	if(!JSONRoot["BinaryIndicies"].isNull())
		ProcessReadGroup(JSONRoot["BinaryIndicies"], BinaryIndicies);
	if(!JSONRoot["AnalogIndicies"].isNull())
		ProcessReadGroup(JSONRoot["AnalogIndicies"], AnalogIndicies);

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

void OidToBinaryEvent::GenerateEvent(IOHandler &handler, const Snmp_pp::Vb &vb, const std::string &name)
{
	bool bvalue = false;
	switch (vb.get_syntax()) {
			/*
			 * For defined types see smi.h @name Syntax Types
			 */
		case sNMP_SYNTAX_INT32: // same as sNMP_SYNTAX_INT
		{
			long value;
			vb.get_value(value);
			bvalue = value != 0;
		}
			break;
			
		case sNMP_SYNTAX_OCTETS:
		{
			const unsigned long ptr_maxlen = 64;
			unsigned long ptr_len;
			unsigned char ptr[ptr_maxlen];
			vb.get_value(ptr, ptr_len, ptr_maxlen, true);
			
			if (this->trueVal.empty())
			{
				// no valueOn defined
				if (this->falseVal.empty())
				{
					// no valueOff defined
					std::cout << "SNMP String received and no value mapping defined: " << ptr << std::endl;
					return;
				}
				if (strcmp((char*)ptr, this->falseVal.c_str()) != 0)
				{
					// default to valueOn
					bvalue = true;
					break;
				}
			}
			else
			{
				if (strcmp((char*)ptr, this->trueVal.c_str()) == 0)
				{
					// matches valueOn
					bvalue = true;
					break;
				}
				else
				{
					if (this->falseVal.empty() || (strcmp((char*)ptr, this->falseVal.c_str()) == 0))
					{
						//default to valueOff or matches valueOff
						//bvalue = false;
						break;
					}
					else
					{
						// matches neither
						std::cout << "Unexpected string value: " << ptr << std::endl;
						return;
					}
				}
			}
		}
			break;
			
		case sNMP_SYNTAX_UINT32:     // same as sNMP_SYNTAX_GAUGE32
		case sNMP_SYNTAX_CNTR32:
		case sNMP_SYNTAX_TIMETICKS:
		{
			unsigned long value;
			vb.get_value(value);
			bvalue = value != 0;
		}
			break;
			
		case sNMP_SYNTAX_CNTR64:
		{
			pp_uint64 value;
			vb.get_value(value);
			bvalue = value != 0;
		}
			break;
			
		case sNMP_SYNTAX_ENDOFMIBVIEW:
		case sNMP_SYNTAX_NOSUCHINSTANCE:
		case sNMP_SYNTAX_NOSUCHOBJECT:
			std::cout << "SNMP Exception: " << vb.get_syntax() << " occured." << std::endl;
			return;
			break;
			
		case sNMP_SYNTAX_BITS: // obsolete
		case sNMP_SYNTAX_NULL: // invalid
		case sNMP_SYNTAX_OID:
		case sNMP_SYNTAX_IPADDR:
		case sNMP_SYNTAX_OPAQUE:
		default:
			std::cout << "Unsupported SNMP object: " << vb.get_syntax() << " received." << std::endl;
			return;
			break;
	}
	
	handler.Event(opendnp3::Binary(bvalue),this->index,name);
}

void OidToAnalogEvent::GenerateEvent(IOHandler &handler, const Snmp_pp::Vb &vb, const std::string &name)
{
	double dvalue;
	switch (vb.get_syntax()) {
			/*
			 * For defined types see smi.h @name Syntax Types
			 */
		case sNMP_SYNTAX_INT32: // same as sNMP_SYNTAX_INT
		{
			long value;
			vb.get_value(value);
			dvalue = value;
		}
			break;
			
		case sNMP_SYNTAX_OCTETS:
		{
			const unsigned long ptr_maxlen = 64;
			unsigned long ptr_len;
			unsigned char ptr[ptr_maxlen];
			vb.get_value(ptr, ptr_len, ptr_maxlen, true);
			// try best to convert string to a double
			dvalue = std::atof((char*)ptr);
		}
			break;
			
		case sNMP_SYNTAX_UINT32:     // same as sNMP_SYNTAX_GAUGE32
		case sNMP_SYNTAX_CNTR32:
		case sNMP_SYNTAX_TIMETICKS:
		{
			unsigned long value;
			vb.get_value(value);
			dvalue = value;
		}
			break;
			
		case sNMP_SYNTAX_CNTR64:
		{
			pp_uint64 value;
			vb.get_value(value);
			dvalue = value;
		}
			break;
			
		case sNMP_SYNTAX_ENDOFMIBVIEW:
		case sNMP_SYNTAX_NOSUCHINSTANCE:
		case sNMP_SYNTAX_NOSUCHOBJECT:
			std::cout << "SNMP Exception: " << vb.get_syntax() << " occured." << std::endl;
			return;
			break;
			
		case sNMP_SYNTAX_BITS: // obsolete
		case sNMP_SYNTAX_NULL: // invalid
		case sNMP_SYNTAX_OID:
		case sNMP_SYNTAX_IPADDR:
		case sNMP_SYNTAX_OPAQUE:
		default:
			std::cout << "Unsupported SNMP object: " << vb.get_syntax() << " received." << std::endl;
			return;
			break;
	}
	
	handler.Event(opendnp3::Analog(dvalue),this->index,name);
}
