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
 * CBORSerialiser.cpp
 *
 *  Created on: 15/08/2024
 *      Author: Neil Stephens
 */

#include <opendatacon/CBORTranslation.h>
#include <opendatacon/IOTypes.h>
#include <opendatacon/util.h>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/cbor/cbor.hpp>
#include <algorithm>
#include <string>
#include <sstream>
#include <cstdint>
#include <string_view>

using namespace jsoncons;

namespace odc
{

namespace
{
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AbsTime_n_SysOffs payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, bool payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::DBB payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, double payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, uint32_t payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::CommandStatus payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::OctetStringBuffer payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::TAI payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::SS payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ControlRelayOutputBlock payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO16 payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO32 payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOF payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOD payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::QualityFlags payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, char payload);
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ConnectState payload);

void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AbsTime_n_SysOffs payload)
{
	encoder.begin_array(2);
	encoder.uint64_value(payload.first);
	encoder.int64_value(payload.second);
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, bool payload)
{
	encoder.bool_value(payload);
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::DBB payload)
{
	encoder.begin_array(2);
	encoder.bool_value(payload.first);
	encoder.bool_value(payload.second);
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, double payload)
{
	encoder.double_value(payload);
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, uint32_t payload)
{
	encoder.uint64_value(payload);
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::CommandStatus payload)
{
	encoder.uint64_value(static_cast<uint64_t>(payload));
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::OctetStringBuffer payload)
{
	encoder.byte_string_value(byte_string_view((const uint8_t*)payload.data(),payload.size()));
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::TAI payload)
{
	encoder.begin_array(3);
	encoder.uint64_value(std::get<0>(payload));
	encoder.uint64_value(std::get<1>(payload));
	encoder.uint64_value(std::get<2>(payload));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::SS payload)
{
	encoder.begin_array(2);
	encoder.uint64_value(std::get<0>(payload));
	encoder.uint64_value(std::get<1>(payload));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ControlRelayOutputBlock payload)
{
	encoder.begin_array(5);
	encoder.uint64_value(static_cast<uint64_t>(payload.functionCode));
	encoder.uint64_value(static_cast<uint64_t>(payload.count));
	encoder.uint64_value(static_cast<uint64_t>(payload.onTimeMS));
	encoder.uint64_value(static_cast<uint64_t>(payload.offTimeMS));
	encoder.uint64_value(static_cast<uint64_t>(payload.status));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO16 payload)
{
	encoder.begin_array(2);
	encoder.int64_value(payload.first);
	encoder.uint64_value(static_cast<uint64_t>(payload.second));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO32 payload)
{
	encoder.begin_array(2);
	encoder.int64_value(payload.first);
	encoder.uint64_value(static_cast<uint64_t>(payload.second));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOF payload)
{
	encoder.begin_array(2);
	encoder.double_value(payload.first);
	encoder.uint64_value(static_cast<uint64_t>(payload.second));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOD payload)
{
	encoder.begin_array(2);
	encoder.double_value(payload.first);
	encoder.uint64_value(static_cast<uint64_t>(payload.second));
	encoder.end_array();
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::QualityFlags payload)
{
	encoder.uint64_value(static_cast<uint64_t>(payload));
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, char payload)
{
	encoder.string_value(std::string(1,payload));
}
void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ConnectState payload)
{
	encoder.string_value(ToString(payload));
}

void EncodePayloadEvent(const std::shared_ptr<const EventInfo>& event, cbor::cbor_bytes_encoder& encoder)
{
	#define ENCODE_PAYLOAD_CASE(T)\
		case T:\
			{\
				auto payload = event->GetPayload<T>();\
				EncodePayload(encoder,payload);\
				break;\
			}
	switch(event->GetEventType())
	{
		ENCODE_PAYLOAD_CASE(odc::EventType::Binary                   )
		ENCODE_PAYLOAD_CASE(odc::EventType::DoubleBitBinary          )
		ENCODE_PAYLOAD_CASE(odc::EventType::Analog                   )
		ENCODE_PAYLOAD_CASE(odc::EventType::Counter                  )
		ENCODE_PAYLOAD_CASE(odc::EventType::FrozenCounter            )
		ENCODE_PAYLOAD_CASE(odc::EventType::BinaryOutputStatus       )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputStatus       )
		ENCODE_PAYLOAD_CASE(odc::EventType::BinaryCommandEvent       )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogCommandEvent       )
		ENCODE_PAYLOAD_CASE(odc::EventType::OctetString              )
		ENCODE_PAYLOAD_CASE(odc::EventType::TimeAndInterval          )
		ENCODE_PAYLOAD_CASE(odc::EventType::SecurityStat             )
		ENCODE_PAYLOAD_CASE(odc::EventType::ControlRelayOutputBlock  )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputInt16        )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputInt32        )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputFloat32      )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputDouble64     )
		ENCODE_PAYLOAD_CASE(odc::EventType::BinaryQuality            )
		ENCODE_PAYLOAD_CASE(odc::EventType::DoubleBitBinaryQuality   )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogQuality            )
		ENCODE_PAYLOAD_CASE(odc::EventType::CounterQuality           )
		ENCODE_PAYLOAD_CASE(odc::EventType::BinaryOutputStatusQuality)
		ENCODE_PAYLOAD_CASE(odc::EventType::FrozenCounterQuality     )
		ENCODE_PAYLOAD_CASE(odc::EventType::AnalogOutputStatusQuality)
		ENCODE_PAYLOAD_CASE(odc::EventType::OctetStringQuality       )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileAuth                 )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileCommand              )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileCommandStatus        )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileTransport            )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileTransportStatus      )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileDescriptor           )
		ENCODE_PAYLOAD_CASE(odc::EventType::FileSpecString           )
		ENCODE_PAYLOAD_CASE(odc::EventType::ConnectState             )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved1                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved2                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved3                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved4                )
		ENCODE_PAYLOAD_CASE(odc::EventType::TimeSync                 )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved6                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved8                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved9                )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved10               )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved11               )
		ENCODE_PAYLOAD_CASE(odc::EventType::Reserved12               )
		default:
			break;
	}
}

} //anonymous namespace

CBORSerialiser::CBORSerialiser(const std::string& json_string, const TranslationConfig& config):
	Config(config),
	Log(config.LogName)
{
	//the json value has the same structure as the CBOR will.
	//store a sequence of encoder operations that will serialise an EventInfo into the CBOR format
	json_string_cursor cursor(json_string);
	for (; !cursor.done(); cursor.next())
	{
		const auto& event = cursor.current();
		switch (event.event_type())
		{
			case staj_event_type::begin_array:
				Ops.emplace_back(EncodeOps::BEGIN_ARRAY);
				break;
			case staj_event_type::end_array:
				Ops.emplace_back(EncodeOps::END_ARRAY);
				break;
			case staj_event_type::begin_object:
				Ops.emplace_back(EncodeOps::BEGIN_MAP);
				break;
			case staj_event_type::end_object:
				Ops.emplace_back(EncodeOps::END_MAP);
				break;
			case staj_event_type::key:
				CheckForPlaceholder(event.get<std::string_view>(),/*isKey = */ true);
				break;
			case staj_event_type::string_value:
				CheckForPlaceholder(event.get<std::string_view>());
				break;
			case staj_event_type::null_value:
				Ops.emplace_back(EncodeOps::NULL_VAL);
				break;
			case staj_event_type::bool_value:
				Ops.emplace_back(EncodeOps::BOOL);
				Bools.emplace_back(event.get<bool>());
				break;
			case staj_event_type::int64_value:
				Ops.emplace_back(EncodeOps::INT);
				Ints.emplace_back(event.get<int64_t>());
				break;
			case staj_event_type::uint64_value:
				Ops.emplace_back(EncodeOps::UINT);
				UInts.emplace_back(event.get<uint64_t>());
				break;
			case staj_event_type::double_value:
				Ops.emplace_back(EncodeOps::DOUBLE);
				Doubles.emplace_back(event.get<double>());
				break;
			default:
			{
				std::stringstream ss;
				ss << "Unhandled parse event: " << event.event_type();
				throw std::runtime_error(ss.str());
			}
		}
	}
}

void CBORSerialiser::CheckForPlaceholder(const std::string_view& str, bool isKey)
{
	auto KeyErr = [this,&str]()
			  {
				  Log.Error("Can't use placeholder '{}' for CBOR key (only string types allowed)",str);
				  Strings.emplace_back(str);
				  return Ops.emplace_back(EncodeOps::KEY);
			  };

	#define CHOOSE_KEY(X) isKey ? Ops.emplace_back(EncodeOps::X ## _KEY) : Ops.emplace_back(EncodeOps::X)
	#define CHOOSE_KEY_ERR(X) isKey ? KeyErr() : Ops.emplace_back(EncodeOps::X)

	if(str == "EVENTTYPE")
		CHOOSE_KEY(EVENTTYPE);
	else if(str == "INDEX")
		CHOOSE_KEY_ERR(INDEX);
	else if(str == "TIMESTAMP")
		CHOOSE_KEY_ERR(TIMESTAMP);
	else if(str == "DATETIME")
		CHOOSE_KEY(DATETIME);
	else if(str == "QUALITY")
		CHOOSE_KEY(QUALITY);
	else if(str == "PAYLOAD")
		CHOOSE_KEY_ERR(PAYLOAD);
	else if(str == "PAYLOAD_STRING")
		CHOOSE_KEY(PAYLOAD_STRING);
	else if(str == "SOURCEPORT")
		CHOOSE_KEY(SOURCEPORT);
	else if(str == "EVENTTYPE_RAW")
		CHOOSE_KEY_ERR(EVENTTYPE_RAW);
	else if(str == "QUALITY_RAW")
		CHOOSE_KEY_ERR(QUALITY_RAW);
	else if(str == "SENDERNAME")
		CHOOSE_KEY(SENDERNAME);
	else
	{
		const std::string prefix("POINT:");
		if(str.find(prefix) == 0)
		{
			CHOOSE_KEY(POINT_FIELD);
			PointFields.emplace_back(str.begin()+prefix.length(),str.end());
		}
		else
		{
			isKey ? Ops.emplace_back(EncodeOps::KEY) : Ops.emplace_back(EncodeOps::STRING);
			Strings.emplace_back(str);
		}
	}
}

std::vector<uint8_t> CBORSerialiser::Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields) const
{
	const auto& dt_fmt = Config.DateTimeFormat;
	const bool utc = Config.DateTimeIsUTC;
	const auto D2S = Config.OctetStringFormat;
	auto StringIt = Strings.begin();
	auto UIntIt = UInts.begin();
	auto IntIt = Ints.begin();
	auto DoubleIt = Doubles.begin();
	auto HalfIt = Halfs.begin();
	auto BoolIt = Bools.begin();
	auto PointFieldsIt = PointFields.begin();

	std::vector<uint8_t> buffer;
	cbor::cbor_bytes_encoder encoder(buffer);
	//iterate over the EncoderSequence and call each operation on the event
	for(auto&& op : Ops)
	{
		switch(op)
		{
			case EncodeOps::EVENTTYPE:
				encoder.string_value(ToString(event->GetEventType()));
				break;
			case EncodeOps::INDEX:
				encoder.uint64_value(event->GetIndex());
				break;
			case EncodeOps::TIMESTAMP:
				encoder.uint64_value(event->GetTimestamp());
				break;
			case EncodeOps::DATETIME:
				encoder.string_value(odc::since_epoch_to_datetime(event->GetTimestamp(),dt_fmt,utc));
				break;
			case EncodeOps::QUALITY:
				encoder.string_value(ToString(event->GetQuality()));
				break;
			case EncodeOps::PAYLOAD_STRING:
				encoder.string_value(event->GetPayloadString(D2S));
				break;
			case EncodeOps::PAYLOAD:
				EncodePayloadEvent(event,encoder);
				break;
			case EncodeOps::SOURCEPORT:
				encoder.string_value(event->GetSourcePort());
				break;
			case EncodeOps::EVENTTYPE_RAW:
				encoder.uint64_value(static_cast<std::underlying_type<EventType>::type>(event->GetEventType()));
				break;
			case EncodeOps::QUALITY_RAW:
				encoder.uint64_value(static_cast<std::underlying_type<QualityFlags>::type>(event->GetQuality()));
				break;
			case EncodeOps::SENDERNAME:
				encoder.string_value(SenderName);
				break;
			case EncodeOps::POINT_FIELD:
			{
				auto field_it = extra_fields.find(*(PointFieldsIt++));
				if(field_it != extra_fields.end())
					encoder.string_value(field_it->second);
				else
					encoder.string_value("POINT:"+*(PointFieldsIt-1));
				break;
			}
			case EncodeOps::POINT_FIELD_KEY:
			{
				auto field_it = extra_fields.find(*(PointFieldsIt++));
				if(field_it != extra_fields.end())
					encoder.key(field_it->second);
				else
					encoder.key("POINT:"+*(PointFieldsIt-1));
				break;
			}
			case EncodeOps::KEY:
				encoder.key(*(StringIt++));
				break;
			case EncodeOps::NULL_VAL:
				encoder.null_value();
				break;
			case EncodeOps::BOOL:
				encoder.bool_value(*(BoolIt++));
				break;
			case EncodeOps::UINT:
				encoder.uint64_value(*(UIntIt++));
				break;
			case EncodeOps::INT:
				encoder.int64_value(*(IntIt++));
				break;
			case EncodeOps::DOUBLE:
				encoder.double_value(*(DoubleIt++));
				break;
			case EncodeOps::HALF:
				encoder.half_value(*(HalfIt++));
				break;
			case EncodeOps::STRING:
				encoder.string_value(*(StringIt++));
				break;
			case EncodeOps::BEGIN_ARRAY:
				encoder.begin_array();
				break;
			case EncodeOps::END_ARRAY:
				encoder.end_array();
				break;
			case EncodeOps::BEGIN_MAP:
				encoder.begin_object();
				break;
			case EncodeOps::END_MAP:
				encoder.end_object();
				break;
			default:
				Log.Error("Unknown CBORSerialiser EncodeOp");
				break;
		}
	}
	return buffer;
}

std::vector<std::string> CBORSerialiser::PointFieldNames() const
{
	std::vector<std::string> names;
	auto it = PointFields.begin();
	for(auto op : Ops)
	{
		if(op != EncodeOps::POINT_FIELD && op != EncodeOps::POINT_FIELD_KEY)
			continue;
		const auto& name = *(it++);
		//only the fields in value position are captured
		if(op == EncodeOps::POINT_FIELD && std::find(names.begin(), names.end(), name) == names.end())
			names.push_back(name);
	}
	return names;
}

std::string CBORSerialiser::FindNonStaticElement(SourceLookupMethod lookup) const
{
	for(auto op : Ops)
	{
		switch(op)
		{
			case EncodeOps::TIMESTAMP: return "TIMESTAMP";
			case EncodeOps::DATETIME:
			case EncodeOps::DATETIME_KEY: return "DATETIME";
			case EncodeOps::QUALITY:
			case EncodeOps::QUALITY_KEY: return "QUALITY";
			case EncodeOps::QUALITY_RAW: return "QUALITY_RAW";
			case EncodeOps::PAYLOAD:
			case EncodeOps::PAYLOAD_STRING:
			case EncodeOps::PAYLOAD_STRING_KEY: return "PAYLOAD";
			case EncodeOps::SENDERNAME:
			case EncodeOps::SENDERNAME_KEY:
				if(lookup != SourceLookupMethod::SenderName) return "SENDERNAME";
				break;
			case EncodeOps::SOURCEPORT:
			case EncodeOps::SOURCEPORT_KEY:
				if(lookup != SourceLookupMethod::SourcePort) return "SOURCEPORT";
				break;
			default:
				break;
		}
	}
	return "";
}

} //namespace odc
