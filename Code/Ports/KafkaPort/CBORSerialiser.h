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
 * CBORSerialiser.h
 *
 *  Created on: 13/08/2024
 *      Author: Neil Stephens
 */

#ifndef CBORSERIALISER_H
#define CBORSERIALISER_H

#include <cstdint>
#include <opendatacon/IOTypes.h>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/cbor/cbor.hpp>
#include <string>

using namespace odc;
using namespace jsoncons;

enum class EncodeOps : uint8_t
{
	//Serialising Ops
	EVENTTYPE,
	INDEX,
	TIMESTAMP,
	DATETIME,
	QUALITY,
	PAYLOAD,
	SOURCEPORT,
	EVENTTYPE_RAW,
	QUALITY_RAW,
	SENDERNAME,
	//Structure Ops
	KEY,
	NULL_VAL,
	BOOL,
	UINT,
	INT,
	DOUBLE,
	HALF,
	STRING,
	BEGIN_ARRAY,
	END_ARRAY,
	BEGIN_MAP,
	END_MAP,
};

using EncoderSequence = std::vector<EncodeOps>;

class CBORSerialiser
{
private:
	EncoderSequence Ops;
	std::vector<std::string> Strings;
	std::vector<uint64_t> UInts;
	std::vector<int64_t> Ints;
	std::vector<double> Doubles;
	std::vector<uint16_t> Halfs;
	std::vector<bool> Bools;

public:
	CBORSerialiser(const std::string& json_string)
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
					Ops.emplace_back(EncodeOps::KEY);
					Strings.emplace_back(event.get<jsoncons::string_view>());
					break;
				case staj_event_type::string_value:
					Ops.emplace_back(EncodeOps::STRING);
					Strings.emplace_back(event.get<jsoncons::string_view>());
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
					if(auto log = odc::spdlog_get("KafkaPort"))
						log->error("Unknown CBORSerialiser EncodeOp");
					throw std::runtime_error("Unknown CBORSerialiser EncodeOp");
			}
		}
	}

	const std::vector<uint8_t> Encode(std::shared_ptr<const EventInfo> event, const std::string& SenderName)
	{
		auto StringIt = Strings.begin();
		auto UIntIt = UInts.begin();
		auto IntIt = Ints.begin();
		auto DoubleIt = Doubles.begin();
		auto HalfIt = Halfs.begin();
		auto BoolIt = Bools.begin();

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
					encoder.string_value(odc::since_epoch_to_datetime(event->GetTimestamp()));
					break;
				case EncodeOps::QUALITY:
					encoder.string_value(ToString(event->GetQuality()));
					break;
				case EncodeOps::PAYLOAD:
					encoder.string_value(event->GetPayloadString());
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
					if(auto log = odc::spdlog_get("KafkaPort"))
						log->error("Unknown CBORSerialiser EncodeOp");
					break;
			}
		}
		return buffer;
	}
};

#endif // CBORSERIALISER_H
