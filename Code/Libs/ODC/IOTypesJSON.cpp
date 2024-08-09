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
 * IOTypesJSON.cpp
 *
 *  Created on: 6/7/2023
 *      Author: Neil Stephens
 */

#include <json/json.h>
#include <opendatacon/IOTypesJSON.h>
#include <opendatacon/IOTypes.h>
#include <exception>
#include <regex>
#include <string>

namespace odc
{

std::shared_ptr<EventInfo> EventInfoFromJson(const std::string& event_json_str)
{
	std::istringstream json_ss(event_json_str);
	Json::CharReaderBuilder JSONReader;
	std::string err_str;
	Json::Value event_json;
	bool parse_success = Json::parseFromStream(JSONReader,json_ss, &event_json, &err_str);
	if (!parse_success)
		throw std::invalid_argument("EventInfoFromJson() Failed to parse JSON: "+err_str);
	return EventInfoFromJson(event_json);
}

std::shared_ptr<EventInfo> EventInfoFromJson(Json::Value JEvtInfo)
{
	if(!JEvtInfo.isObject())
		throw std::invalid_argument("EventInfoFromJson() requires a JSON object: "+JEvtInfo.toStyledString());

	//EventType
	if(!JEvtInfo.isMember("EventType"))
		throw std::invalid_argument("EventInfoFromJson() requires an 'EventType' member: "+JEvtInfo.toStyledString());

	EventType ET;
	if(JEvtInfo["EventType"].isIntegral())
	{
		ET = static_cast<EventType>(JEvtInfo["EventType"].asUInt());
		if(ET <= EventType::BeforeRange || ET >= EventType::AfterRange)
			throw std::invalid_argument("EventInfoFromJson() Invalid 'EventType': "+JEvtInfo["EventType"].toStyledString());
	}
	else if(JEvtInfo["EventType"].isString())
	{
		ET = EventTypeFromString(JEvtInfo["EventType"].asString());
		if(ET <= EventType::BeforeRange || ET >= EventType::AfterRange)
			throw std::invalid_argument("EventInfoFromJson() Invalid 'EventType': "+JEvtInfo["EventType"].toStyledString());
	}
	else
		throw std::invalid_argument("EventInfoFromJson() Invalid 'EventType': "+JEvtInfo["EventType"].toStyledString());
	auto event = std::make_shared<EventInfo>(ET);

	//Index
	if(!JEvtInfo.isMember("Index") || !JEvtInfo["Index"].isUInt())
		throw std::invalid_argument("EventInfoFromJson() requires an unsigned integer 'Index' member: "+JEvtInfo.toStyledString());
	event->SetIndex(JEvtInfo["Index"].asUInt());

	//SourcePort - optional
	if(JEvtInfo.isMember("SourcePort"))
	{
		if(!JEvtInfo["SourcePort"].isString())
			throw std::invalid_argument("EventInfoFromJson() Invalid 'SourcePort' - should be string: "+JEvtInfo["SourcePort"].toStyledString());
		event->SetSource(JEvtInfo["SourcePort"].asString());
	}

	//QualityFlags - optional
	if(JEvtInfo.isMember("QualityFlags"))
	{
		QualityFlags QF;
		if(JEvtInfo["QualityFlags"].isUInt())
		{
			QF = static_cast<QualityFlags>(JEvtInfo["QualityFlags"].asUInt()&0xFF);
		}
		else if(JEvtInfo["QualityFlags"].isString())
		{
			QF = QualityFlagsFromString(JEvtInfo["QualityFlags"].asString());
			if(QF == QualityFlags::NONE && JEvtInfo["QualityFlags"].asString() != "NONE")
				throw std::invalid_argument("EventInfoFromJson() Invalid 'QualityFlags': "+JEvtInfo["QualityFlags"].toStyledString());
		}
		else
			throw std::invalid_argument("EventInfoFromJson() Invalid 'QualityFlags': "+JEvtInfo["QualityFlags"].toStyledString());
		event->SetQuality(QF);
	}

	//Timestamp - optional
	if(JEvtInfo.isMember("Timestamp"))
	{
		msSinceEpoch_t t;
		if(JEvtInfo["Timestamp"].isUInt64())
		{
			t = JEvtInfo["Timestamp"].asUInt64();
		}
		else if(JEvtInfo["Timestamp"].isString())
		{
			t = datetime_to_since_epoch(JEvtInfo["Timestamp"].asString());
		}
		else
			throw std::invalid_argument("EventInfoFromJson() Invalid 'Timestamp': "+JEvtInfo["Timestamp"].toStyledString());
		event->SetTimestamp(t);
	}

	//Payload - optional
	if(JEvtInfo.isMember("Payload"))
	{
		try
		{
			PayloadFromJson(JEvtInfo["Payload"],event);
		}
		catch(const std::exception& e)
		{
			throw std::invalid_argument("EventInfoFromJson() Invalid 'Payload': "+JEvtInfo["Payload"].toStyledString()+": "+e.what());
		}
	}
	else
		event->SetPayload();

	return event;
}

template <> bool PayloadFromJson(const Json::Value& JLoad)
{
	if(!JLoad.isBool())
		throw std::invalid_argument("Payload is not boolean.");
	return JLoad.asBool();
}
template<> DBB PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'First' and 'Second' member booleans.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("First"))
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Second"))
		throw std::invalid_argument(err);

	return { JLoad["First"].asBool(), JLoad["Second"].asBool() };
}
template<> double PayloadFromJson(const Json::Value& JLoad)
{
	if(!JLoad.isNumeric())
		throw std::invalid_argument("Payload is not numeric.");
	return JLoad.asDouble();
}
template<> uint32_t PayloadFromJson(const Json::Value& JLoad)
{
	if(!JLoad.isUInt())
		throw std::invalid_argument("Payload is not unsigned integer.");
	return JLoad.asUInt();
}
template<> CommandStatus PayloadFromJson(const Json::Value& JLoad)
{
	if(JLoad.isUInt())
	{
		return static_cast<CommandStatus>(JLoad.asUInt());
	}
	if(JLoad.isString())
	{
		auto CS = CommandStatusFromString(JLoad.asString());
		if(CS == CommandStatus::UNDEFINED && JLoad.asString() != "UNDEFINED")
			throw std::invalid_argument("Can't convert string to odc::CommandStatus.");
		return CS;
	}
	throw std::invalid_argument("Payload CommandStatus is not unsigned integer or valid string");
}
template<> OctetStringBuffer PayloadFromJson(const Json::Value& JLoad)
{
	if(!JLoad.isString())
		throw std::invalid_argument("Payload is not a string.");

	static const std::regex base64_data_uri_rgx("data:([^;]+;)?base64,([a-zA-Z0-9=/]*[=]{1,2})",std::regex::extended);
	std::smatch match_results;
	auto str = JLoad.asString();
	if(std::regex_match(str, match_results, base64_data_uri_rgx))
	{
		auto b64 = match_results.str(2);
		auto bin_str = b64decode(b64);
		return OctetStringBuffer(bin_str);
	}
	return OctetStringBuffer(JLoad.asString());
}
template<> TAI PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'Timestamp', 'Interval' and 'Units' members.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Timestamp"))
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Interval"))
		throw std::invalid_argument(err);
	if(!JLoad["Interval"].isUInt())
		throw std::invalid_argument("Payload 'Interval' is not unsigned integer.");

	if(!JLoad.isMember("Units"))
		throw std::invalid_argument(err);
	if(!JLoad["Units"].isUInt())
		throw std::invalid_argument("Payload 'Units' is not unsigned integer.");

	msSinceEpoch_t TS;
	if(JLoad["Timestamp"].isUInt64())
		TS = JLoad["Timestamp"].asUInt64();
	else if(JLoad["Timestamp"].isString())
		TS = datetime_to_since_epoch(JLoad["Timestamp"].asString());
	else
		throw std::invalid_argument("Payload 'Timestamp' is not unsigned integer or string.");

	return { TS, JLoad["Interval"].asUInt(), JLoad["Units"].asUInt() };
}
template<> SS PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'First' and 'Second' member unsigned integers.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("First") || !JLoad["First"].isUInt())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Second") || !JLoad["Second"].isUInt())
		throw std::invalid_argument(err);

	return { JLoad["First"].asUInt(), JLoad["Second"].asUInt() };
}
template<> ControlRelayOutputBlock PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'ControlCode' member.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("ControlCode"))
		throw std::invalid_argument(err);

	ControlRelayOutputBlock CROB;
	if(JLoad["ControlCode"].isUInt())
		CROB.functionCode = static_cast<ControlCode>(JLoad["ControlCode"].asUInt()&0xFF);
	else if(JLoad["ControlCode"].isString())
		CROB.functionCode = ControlCodeFromString(JLoad["ControlCode"].asString());
	else
		throw std::invalid_argument("Payload 'ControlCode' is not unsigned integer or valid string.");

	if(JLoad.isMember("CommandStatus"))
		CROB.status = PayloadFromJson<CommandStatus>(JLoad["CommandStatus"]);

	if(JLoad.isMember("Count"))
	{
		if(!JLoad["Count"].isUInt())
			throw "Payload 'Count' isn't an unsigned integer.";
		CROB.count = JLoad["Count"].asUInt()&0xFF;
	}
	if(JLoad.isMember("msOnTime"))
	{
		if(!JLoad["msOnTime"].isUInt())
			throw "Payload 'msOnTime' isn't an unsigned integer.";
		CROB.onTimeMS = JLoad["msOnTime"].asUInt();
	}
	if(JLoad.isMember("msOffTime"))
	{
		if(!JLoad["msOffTime"].isUInt())
			throw "Payload 'msOffTime' isn't an unsigned integer.";
		CROB.offTimeMS = JLoad["msOffTime"].asUInt();
	}
	return CROB;
}
template<> AO16 PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'Value' and 'CommandStatus' members.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Value"))
		throw std::invalid_argument(err);
	if(!JLoad["Value"].isInt()
	   || JLoad["Value"].asInt() > std::numeric_limits<int16_t>::max()
	   || JLoad["Value"].asInt() < std::numeric_limits<int16_t>::min())
		throw std::invalid_argument("Payload 'Value' is not a 16 bit integer.");

	if(!JLoad.isMember("CommandStatus"))
		throw std::invalid_argument(err);

	auto CS = PayloadFromJson<CommandStatus>(JLoad["CommandStatus"]);

	return { JLoad["Value"].asInt(), CS };
}
template<> AO32 PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'Value' and 'CommandStatus' members.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Value"))
		throw std::invalid_argument(err);
	if(!JLoad["Value"].isInt())
		throw std::invalid_argument("Payload 'Value' is not a integer.");

	if(!JLoad.isMember("CommandStatus"))
		throw std::invalid_argument(err);

	auto CS = PayloadFromJson<CommandStatus>(JLoad["CommandStatus"]);

	return { JLoad["Value"].asInt(), CS };
}
template<> AOF PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'Value' and 'CommandStatus' members.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Value"))
		throw std::invalid_argument(err);
	if(!JLoad["Value"].isNumeric()
	   || JLoad["Value"].asDouble() > std::numeric_limits<float>::max()
	   || JLoad["Value"].asDouble() < std::numeric_limits<float>::lowest())
		throw std::invalid_argument("Payload 'Value' is not 32bit float");

	if(!JLoad.isMember("CommandStatus"))
		throw std::invalid_argument(err);

	auto CS = PayloadFromJson<CommandStatus>(JLoad["CommandStatus"]);

	return { JLoad["Value"].asFloat(), CS };
}
template<> AOD PayloadFromJson(const Json::Value& JLoad)
{
	const auto err = "Payload is not a JSON object with 'Value' and 'CommandStatus' members.";

	if(!JLoad.isObject())
		throw std::invalid_argument(err);

	if(!JLoad.isMember("Value"))
		throw std::invalid_argument(err);
	if(!JLoad["Value"].isNumeric())
		throw std::invalid_argument("Payload 'Value' is not numeric");

	if(!JLoad.isMember("CommandStatus"))
		throw std::invalid_argument(err);

	auto CS = PayloadFromJson<CommandStatus>(JLoad["CommandStatus"]);

	return { JLoad["Value"].asDouble(), CS };
}
template<> QualityFlags PayloadFromJson(const Json::Value& JLoad)
{
	if(JLoad.isUInt())
	{
		return static_cast<QualityFlags>(JLoad.asUInt());
	}
	if(JLoad.isString())
	{
		auto QF = QualityFlagsFromString(JLoad.asString());
		if(QF == QualityFlags::NONE && JLoad.asString() != "NONE")
			throw std::invalid_argument("Can't convert string to odc::QualityFlags.");
		return QF;
	}
	throw std::invalid_argument("Payload QualityFlags is not unsigned integer or valid string");
}
template<> char PayloadFromJson(const Json::Value& JLoad)
{
	if(JLoad.isString() && JLoad.asString().length() > 1)
		return *JLoad.asString().data();

	if(JLoad.isInt()
	   && JLoad.asInt() <= std::numeric_limits<char>::max()
	   && JLoad.asInt() >= std::numeric_limits<char>::min())
		return JLoad.asInt();

	throw std::invalid_argument("Payload is not convertable to char");
}
template<> ConnectState PayloadFromJson(const Json::Value& JLoad)
{
	if(JLoad.isUInt())
	{
		return static_cast<ConnectState>(JLoad.asUInt());
	}

	if(JLoad.isString())
	{
		ConnectState CS = ConnectStateFromString(JLoad.asString());
		if(CS != ConnectState::UNDEFINED)
			return CS;
	}
	throw std::invalid_argument("Payload not convertable to odc::ConnectState.");
}

#define POP_PAYLOAD_CASE(T)\
	case T:\
		{\
			event->SetPayload<T>(PayloadFromJson<typename EventTypePayload<T>::type>(JLoad));\
			break;\
		}
void PayloadFromJson(const Json::Value& JLoad, std::shared_ptr<EventInfo> event)
{
	switch(event->GetEventType())
	{
		POP_PAYLOAD_CASE(EventType::Binary                   )
		POP_PAYLOAD_CASE(EventType::DoubleBitBinary          )
		POP_PAYLOAD_CASE(EventType::Analog                   )
		POP_PAYLOAD_CASE(EventType::Counter                  )
		POP_PAYLOAD_CASE(EventType::FrozenCounter            )
		POP_PAYLOAD_CASE(EventType::BinaryOutputStatus       )
		POP_PAYLOAD_CASE(EventType::AnalogOutputStatus       )
		POP_PAYLOAD_CASE(EventType::BinaryCommandEvent       )
		POP_PAYLOAD_CASE(EventType::AnalogCommandEvent       )
		POP_PAYLOAD_CASE(EventType::OctetString              )
		POP_PAYLOAD_CASE(EventType::TimeAndInterval          )
		POP_PAYLOAD_CASE(EventType::SecurityStat             )
		POP_PAYLOAD_CASE(EventType::ControlRelayOutputBlock  )
		POP_PAYLOAD_CASE(EventType::AnalogOutputInt16        )
		POP_PAYLOAD_CASE(EventType::AnalogOutputInt32        )
		POP_PAYLOAD_CASE(EventType::AnalogOutputFloat32      )
		POP_PAYLOAD_CASE(EventType::AnalogOutputDouble64     )
		POP_PAYLOAD_CASE(EventType::BinaryQuality            )
		POP_PAYLOAD_CASE(EventType::DoubleBitBinaryQuality   )
		POP_PAYLOAD_CASE(EventType::AnalogQuality            )
		POP_PAYLOAD_CASE(EventType::CounterQuality           )
		POP_PAYLOAD_CASE(EventType::BinaryOutputStatusQuality)
		POP_PAYLOAD_CASE(EventType::FrozenCounterQuality     )
		POP_PAYLOAD_CASE(EventType::AnalogOutputStatusQuality)
		POP_PAYLOAD_CASE(EventType::FileAuth                 )
		POP_PAYLOAD_CASE(EventType::FileCommand              )
		POP_PAYLOAD_CASE(EventType::FileCommandStatus        )
		POP_PAYLOAD_CASE(EventType::FileTransport            )
		POP_PAYLOAD_CASE(EventType::FileTransportStatus      )
		POP_PAYLOAD_CASE(EventType::FileDescriptor           )
		POP_PAYLOAD_CASE(EventType::FileSpecString           )
		POP_PAYLOAD_CASE(EventType::ConnectState             )
		POP_PAYLOAD_CASE(EventType::Reserved1                )
		POP_PAYLOAD_CASE(EventType::Reserved2                )
		POP_PAYLOAD_CASE(EventType::Reserved3                )
		POP_PAYLOAD_CASE(EventType::Reserved4                )
		POP_PAYLOAD_CASE(EventType::Reserved5                )
		POP_PAYLOAD_CASE(EventType::Reserved6                )
		POP_PAYLOAD_CASE(EventType::Reserved7                )
		POP_PAYLOAD_CASE(EventType::Reserved8                )
		POP_PAYLOAD_CASE(EventType::Reserved9                )
		POP_PAYLOAD_CASE(EventType::Reserved10               )
		POP_PAYLOAD_CASE(EventType::Reserved11               )
		POP_PAYLOAD_CASE(EventType::Reserved12               )
		default:
			throw std::runtime_error("Can't parse Payload for unknown EventType "+ToString(event->GetEventType()));
	}
}

} //namespace odc
