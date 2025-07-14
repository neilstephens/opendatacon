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
 * CBORDeserialiser.cpp
 *
 *  Created on: 2024-12-08
 *      Author: Neil Stephens
 */

#include "CBORDeserialiser.h"

CBORDeserialiser::CBORDeserialiser(const CBORSerialiser* const serialiser, const std::string& datetime_format, const bool utc):
	Deserialiser(datetime_format,utc),
	pSerialiser(serialiser)
{
	if(!pSerialiser)
	{
		Log.Error("CBORDeserialiser created with null CBORSerialiser");
		throw std::runtime_error("CBORDeserialiser created with null CBORSerialiser");
	}
}

//TODO: serialiser takes DataToStringMethod and DateTimeFormat parameters, deserialiser probably should too
//	currently deserialiser has datetime format member, but nothing for D2S
std::shared_ptr<EventInfo> CBORDeserialiser::Deserialise(const KCC::ConsumerRecord& record)
{
	// Things we expect to find in the CBOR
	auto et = odc::EventType::BeforeRange;
	auto index = 0;
	auto timestamp = odc::msSinceEpoch_t();
	auto quality = odc::QualityFlags::ONLINE;
	std::string source_port = "";
	// payload is polymorphic based on event type

	// Iterators for the literal values we expect to find
	auto StringIt = pSerialiser->Strings.begin();
	auto UIntIt = pSerialiser->UInts.begin();
	auto IntIt = pSerialiser->Ints.begin();
	auto DoubleIt = pSerialiser->Doubles.begin();
	auto HalfIt = pSerialiser->Halfs.begin();
	auto BoolIt = pSerialiser->Bools.begin();

	//Event info that we're going to return
	std::shared_ptr<EventInfo> event = nullptr;

	std::string_view data(reinterpret_cast<const char*>(record.value().data()),record.value().size());
	cbor::cbor_bytes_cursor cursor(data);

	//Iterate over the Ops and follow along with the CBOR cursor
	for(auto Op : pSerialiser->Ops)
	{
		if(cursor.done())
		{
			Log.Error("CBORDeserialiser: CBOR has fewer elements than serialiser Ops");
			return nullptr;
		}
		const auto& cbor_event = cursor.current();
		if(!ParseMatchOp(Op, cbor_event))
		{
			Log.Error("CBORDeserialiser: Unexpected CBOR structure");
			return nullptr;
		}
		switch(Op)
		{
			case EncodeOps::BEGIN_ARRAY:
			case EncodeOps::END_ARRAY:
			case EncodeOps::BEGIN_MAP:
			case EncodeOps::END_MAP:
			case EncodeOps::NULL_VAL:
			case EncodeOps::POINT_FIELD_KEY:
			case EncodeOps::POINT_FIELD:
			case EncodeOps::SENDERNAME_KEY:
			case EncodeOps::SENDERNAME:
			{
				//not used for deserialisation
				cursor.next();
				break;
			}
			case EncodeOps::PAYLOAD:
			case EncodeOps::PAYLOAD_STRING:
			case EncodeOps::PAYLOAD_STRING_KEY:
			{
				if(et == odc::EventType::BeforeRange)
				{
					Log.Error("CBORDeserialiser: Payload found before EventType. Unable to parse.");
					return nullptr;
				}

				event = std::make_shared<EventInfo>(et);
				try
				{
					if(Op == EncodeOps::PAYLOAD) //binary (native CBOR) encoded payload
						PopPayload(cursor,event);
					else //string encoded
					{
						//TODO: support non-default D2S (data to string) methods
						event->SetPayload(cbor_event.get<std::string>());
						cursor.next();
					}
				}
				catch(const std::exception& e)
				{
					Log.Error("CBORDeserialiser: Error parsing {} payload: {}", ToString(et), e.what());
					return nullptr;
				}
				break;
			}
			case EncodeOps::EVENTTYPE_KEY:
			case EncodeOps::EVENTTYPE:
			{
				auto et_str = cbor_event.get<std::string>();
				et = odc::EventTypeFromString(et_str);
				if(et == odc::EventType::AfterRange)
				{
					Log.Error("CBORDeserialiser: Invalid EventType '{}'", et_str);
					return nullptr;
				}
				cursor.next();
				break;
			}
			case EncodeOps::DATETIME_KEY:
			case EncodeOps::DATETIME:
			{
				auto datetime_str = cbor_event.get<std::string>();
				try
				{
					timestamp = odc::datetime_to_since_epoch(datetime_str, datetime_format, utc);
				}
				catch(const std::exception& e)
				{
					Log.Error("CBORDeserialiser: Error parsing datetime '{}': {}", datetime_str, e.what());
					return nullptr;
				}
				cursor.next();
				break;
			}
			case EncodeOps::QUALITY_KEY:
			case EncodeOps::QUALITY:
			{
				quality = odc::QualityFlagsFromString(cbor_event.get<std::string>());
				cursor.next();
				break;
			}
			case EncodeOps::SOURCEPORT_KEY:
			case EncodeOps::SOURCEPORT:
			{
				source_port = cbor_event.get<std::string>();
				cursor.next();
				break;
			}
			case EncodeOps::INDEX:
			{
				index = cbor_event.get<uint64_t>();
				cursor.next();
				break;
			}
			case EncodeOps::TIMESTAMP:
			{
				timestamp = cbor_event.get<uint64_t>();
				cursor.next();
				break;
			}
			case EncodeOps::EVENTTYPE_RAW:
			{
				et = EventTypeFromInt(cbor_event.get<uint64_t>());
				if(et == EventType::AfterRange)
				{
					Log.Error("CBORDeserialiser: Invalid EventType {}", cbor_event.get<uint64_t>());
					return nullptr;
				}
				cursor.next();
				break;
			}
			case EncodeOps::QUALITY_RAW:
			{
				//check that the value is at least in 16bit range
				if(cbor_event.get<uint64_t>() > std::numeric_limits<uint16_t>::max())
				{
					Log.Error("CBORDeserialiser: Invalid QualityFlags {}", cbor_event.get<uint64_t>());
					return nullptr;
				}
				quality = static_cast<odc::QualityFlags>(cbor_event.get<uint64_t>());
				cursor.next();
				break;
			}
			case EncodeOps::STRING:
			case EncodeOps::KEY:
			{
				//compare to the expected string literals
				auto str_val = cbor_event.get<std::string>();
				if(StringIt == pSerialiser->Strings.end() || *StringIt != str_val)
				{
					Log.Error("CBORDeserialiser: Unexpected string literal '{}'", str_val);
					return nullptr;
				}
				++StringIt;
				cursor.next();
				break;
			}
			case EncodeOps::UINT:
			{
				//compare to the expected literal uints
				if(UIntIt == pSerialiser->UInts.end() || *UIntIt != cbor_event.get<uint64_t>())
				{
					Log.Error("CBORDeserialiser: Unexpected uint literal '{}'", cbor_event.get<uint64_t>());
					return nullptr;
				}
				++UIntIt;
				cursor.next();
				break;
			}
			case EncodeOps::BOOL:
			{
				//compare to the expected literal bools
				if(BoolIt == pSerialiser->Bools.end() || *BoolIt != cbor_event.get<bool>())
				{
					Log.Error("CBORDeserialiser: Unexpected bool literal '{}'", cbor_event.get<bool>());
					return nullptr;
				}
				++BoolIt;
				cursor.next();
				break;
			}
			case EncodeOps::DOUBLE:
			{
				//compare to the expected literal doubles
				if(DoubleIt == pSerialiser->Doubles.end() || *DoubleIt != cbor_event.get<double>())
				{
					Log.Error("CBORDeserialiser: Unexpected double literal '{}'", cbor_event.get<double>());
					return nullptr;
				}
				++DoubleIt;
				cursor.next();
				break;
			}
			case EncodeOps::HALF:
			{
				//compare to the expected literal halves
				if(HalfIt == pSerialiser->Halfs.end() || *HalfIt != cbor_event.get<uint16_t>())
				{
					Log.Error("CBORDeserialiser: Unexpected half literal '{}'", cbor_event.get<uint16_t>());
					return nullptr;
				}
				++HalfIt;
				cursor.next();
				break;
			}
			case EncodeOps::INT:
			{
				//compare to the expected literal ints
				if(IntIt == pSerialiser->Ints.end() || *IntIt != cbor_event.get<int64_t>())
				{
					Log.Error("CBORDeserialiser: Unexpected int literal '{}'", cbor_event.get<int64_t>());
					return nullptr;
				}
				++IntIt;
				cursor.next();
				break;
			}
			default:
			{
				Log.Error("CBORDeserialiser: Unknown Encode Operator.");
				return nullptr;
			}
		}
	}

	if(!event)
	{
		//log an error - there must not have been an event type and payload
		Log.Error("CBORDeserialiser: Minimum event info (EventType and Payload) not found in CBOR.");
		return nullptr;
	}

	//update the index, quality, time etc
	event->SetIndex(index);
	event->SetQuality(quality);
	event->SetTimestamp(timestamp);
	event->SetSource(source_port);

	return event;
}

//Function to do a preliminary check that the CBOR structure matches the encoding op
bool CBORDeserialiser::ParseMatchOp(const EncodeOps& Op, const staj_event& cbor_event)
{
	switch(Op)
	{
		case EncodeOps::PAYLOAD:
		{
			//always pass for a payload, because it can be various structures
			return true;
		}
		case EncodeOps::BEGIN_ARRAY:
		{
			if(cbor_event.event_type() != staj_event_type::begin_array)
				return false;
			return true;
		}
		case EncodeOps::END_ARRAY:
		{
			if(cbor_event.event_type() != staj_event_type::end_array)
				return false;
			return true;
		}
		case EncodeOps::BEGIN_MAP:
		{
			if(cbor_event.event_type() != staj_event_type::begin_object)
				return false;
			return true;
		}
		case EncodeOps::END_MAP:
		{
			if(cbor_event.event_type() != staj_event_type::end_object)
				return false;
			return true;
		}
		case EncodeOps::EVENTTYPE_KEY:
		case EncodeOps::DATETIME_KEY:
		case EncodeOps::PAYLOAD_STRING_KEY:
		case EncodeOps::QUALITY_KEY:
		case EncodeOps::SOURCEPORT_KEY:
		case EncodeOps::POINT_FIELD_KEY:
		case EncodeOps::SENDERNAME_KEY:
		case EncodeOps::KEY:
		{
			if(cbor_event.event_type() != staj_event_type::key)
				return false;
			return true;
		}
		case EncodeOps::EVENTTYPE:
		case EncodeOps::DATETIME:
		case EncodeOps::PAYLOAD_STRING:
		case EncodeOps::QUALITY:
		case EncodeOps::SOURCEPORT:
		case EncodeOps::POINT_FIELD:
		case EncodeOps::SENDERNAME:
		case EncodeOps::STRING:
		{
			if(cbor_event.event_type() != staj_event_type::string_value)
				return false;
			return true;
		}
		case EncodeOps::INDEX:
		case EncodeOps::TIMESTAMP:
		case EncodeOps::EVENTTYPE_RAW:
		case EncodeOps::QUALITY_RAW:
		case EncodeOps::UINT:
		{
			if(cbor_event.event_type() != staj_event_type::uint64_value)
				return false;
			return true;
		}
		case EncodeOps::BOOL:
		{
			if(cbor_event.event_type() != staj_event_type::bool_value)
				return false;
			return true;
		}
		case EncodeOps::DOUBLE:
		{
			if(cbor_event.event_type() != staj_event_type::double_value)
				return false;
			return true;
		}
		case EncodeOps::HALF:
		{
			if(cbor_event.event_type() != staj_event_type::half_value)
				return false;
			return true;
		}
		case EncodeOps::INT:
		{
			if(cbor_event.event_type() != staj_event_type::int64_value)
				return false;
			return true;
		}
		case EncodeOps::NULL_VAL:
		{
			if(cbor_event.event_type() != staj_event_type::null_value)
				return false;
			return true;
		}
		default:
		{
			Log.Error("CBORDeserialiser: Unknown Encode Operator.");
			return false;
		}
	}
	//can't/shouldn't get here
	Log.Error("CBORDeserialiser: Something went terribly wrong.");
	return false;
}

template <> bool CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	const auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::bool_value)
		throw std::invalid_argument("Payload is not a bool");
	auto ret = cbor_event.get<bool>();
	cursor.next();
	return ret;
}
template<> odc::DBB CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();

	bool b1,b2;
	// expect ' [ bool , bool ] '
	const auto err = "Payload is not an array of two bools";

	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::bool_value)
		throw std::invalid_argument(err);
	b1 = cbor_event.get<bool>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::bool_value)
		throw std::invalid_argument(err);
	b2 = cbor_event.get<bool>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {b1,b2};
}
template<> double CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::double_value)
		throw std::invalid_argument("Payload is not a double");
	auto ret = cbor_event.get<double>();
	cursor.next();
	return ret;
}
template<> uint32_t CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument("Payload is not a uint");

	//check that the uint value is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("Payload uint value is too large for uint32_t");
	auto ret = static_cast<uint32_t>(cbor_event.get<uint64_t>());
	cursor.next();
	return ret;
}
template<> odc::CommandStatus CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument("Payload is not a CommandStatus (uint)");

	//check that the uint value is a valid CommandStatus
	auto status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(status == CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload CommandStatus value is out of range");
	cursor.next();
	return status;
}
template<> odc::OctetStringBuffer CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::byte_string_value)
		throw std::invalid_argument("Payload is not an OctetStringBuffer (byte string)");
	auto byte_strv = cbor_event.get<byte_string_view>();
	cursor.next();
	return OctetStringBuffer(std::vector(byte_strv.data(),byte_strv.data()+byte_strv.size()));
}
template<> odc::TAI CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	uint64_t timestamp;
	uint32_t interval;
	uint8_t units;
	//expect ' [ uint64 , uint32, uint8 ] '
	const auto err = "Payload is not an array of three uints: [ uint64 , uint32, uint8 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	timestamp = cbor_event.get<uint64_t>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the interval value is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("Payload interval value is too large for uint32_t");
	interval = cbor_event.get<uint64_t>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the units value is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload units value is too large for uint8_t");
	units = static_cast<uint8_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {timestamp, interval, units};
}
template<> odc::SS CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	uint16_t first;
	uint32_t second;
	//expect ' [ uint16 , uint32 ] '
	const auto err = "Payload is not an array of two uints: [ uint16 , uint32 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the first value is representable in 16 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint16_t>::max())
		throw std::out_of_range("Payload first value is too large for uint16_t");
	first = static_cast<uint16_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the second value is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("Payload second value is too large for uint32_t");
	second = static_cast<uint32_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {first, second};
}
template<> odc::ControlRelayOutputBlock CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	odc::ControlRelayOutputBlock CROB;
	auto& cbor_event = cursor.current();
	//expect ' [
	//uint8_t CROB.functionCode,
	//uint8_t CROB.count,
	//uint32_t CROB.onTimeMS,
	//uint32_t CROB.offTimeMS,
	//uint8_t CROB.status,
	// ] '
	const auto err = "Payload is not an array of ControlRelayOutputBlock values";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the function code is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload function code is too large for uint8_t");
	CROB.functionCode = ControlCodeFromInt(cbor_event.get<uint64_t>());
	if(CROB.functionCode == odc::ControlCode::UNDEFINED)
		throw std::out_of_range("Payload function code is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the count is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload count is too large for uint8_t");
	CROB.count = static_cast<uint8_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the onTimeMS is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("Payload onTimeMS is too large for uint32_t");
	CROB.onTimeMS = static_cast<uint32_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the offTimeMS is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("Payload offTimeMS is too large for uint32_t");
	CROB.offTimeMS = static_cast<uint32_t>(cbor_event.get<uint64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the status is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload status is too large for uint8_t");
	CROB.status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(CROB.status == odc::CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload status is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return CROB;
}
template<> odc::AO16 CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	int16_t value;
	//expect ' [ int16 , uint8 ] '
	const auto err = "Payload is not an array of two values: [ int16 , uint8 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::int64_value)
		throw std::invalid_argument(err);
	//check that the value is representable in 16 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<int16_t>::max() ||
	   cbor_event.get<uint64_t>() < std::numeric_limits<int16_t>::min())
		throw std::out_of_range("Payload value is out of range for int16_t");
	value = static_cast<int16_t>(cbor_event.get<int64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the quality is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload quality is too large for uint8_t");
	auto status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(status == odc::CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload status is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {value, status};
}
template<> odc::AO32 CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	int32_t value;
	//expect ' [ int32 , uint8 ] '
	const auto err = "Payload is not an array of two values: [ int32 , uint8 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::int64_value)
		throw std::invalid_argument(err);
	//check that the value is representable in 32 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<int32_t>::max() ||
	   cbor_event.get<uint64_t>() < std::numeric_limits<int32_t>::min())
		throw std::out_of_range("Payload value is out of range for int32_t");
	value = static_cast<int32_t>(cbor_event.get<int64_t>());
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the quality is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload quality is too large for uint8_t");
	auto status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(status == odc::CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload status is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {value, status};
}
template<> odc::AOF CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	float value;
	//expect ' [ float32 , uint8 ] '
	const auto err = "Payload is not an array of two values: [ float32 , uint8 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::double_value)
		throw std::invalid_argument(err);
	//check that the value is representable in 32 bits
	if(cbor_event.get<double>() > std::numeric_limits<float>::max() ||
	   cbor_event.get<double>() < std::numeric_limits<float>::lowest())
		throw std::out_of_range("Payload value is out of range for float");
	value = cbor_event.get<double>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the quality is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload quality is too large for uint8_t");
	auto status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(status == odc::CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload status is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {value, status};
}
template<> odc::AOD CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	double value;
	//expect ' [ float64 , uint8 ] '
	const auto err = "Payload is not an array of two values: [ float64 , uint8 ]";
	if(cbor_event.event_type() != staj_event_type::begin_array)
		throw std::invalid_argument(err);
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::double_value)
		throw std::invalid_argument(err);
	value = cbor_event.get<double>();
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument(err);
	//check that the quality is representable in 8 bits
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint8_t>::max())
		throw std::out_of_range("Payload quality is too large for uint8_t");
	auto status = CommandStatusFromInt(cbor_event.get<uint64_t>());
	if(status == odc::CommandStatus::UNDEFINED)
		throw std::out_of_range("Payload status is out of range");
	cursor.next();
	if(cbor_event.event_type() != staj_event_type::end_array)
		throw std::invalid_argument(err);
	cursor.next();
	return {value, status};
}
template<> odc::QualityFlags CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::uint64_value)
		throw std::invalid_argument("Payload is not a QualityFlags (uint)");
	//check its in the range of uint16
	if(cbor_event.get<uint64_t>() > std::numeric_limits<uint16_t>::max())
		throw std::out_of_range("Payload QualityFlags value is too large for uint16_t");
	auto flags = static_cast<odc::QualityFlags>(cbor_event.get<uint64_t>());
	cursor.next();
	return flags;
}
template<> char CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::string_value)
		throw std::invalid_argument("Payload is not a char (string)");
	auto str_val = cbor_event.get<std::string>();
	if(str_val.size() != 1)
		throw std::invalid_argument("Payload string is not a single character");
	cursor.next();
	return str_val[0];
}
template<> odc::ConnectState CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor)
{
	auto& cbor_event = cursor.current();
	if(cbor_event.event_type() != staj_event_type::string_value)
		throw std::invalid_argument("Payload is not a ConnectState (string)");
	auto state_str = cbor_event.get<std::string>();
	auto state = odc::ConnectStateFromString(state_str);
	if(state == odc::ConnectState::UNDEFINED)
		throw std::out_of_range("Payload ConnectState value is out of range");
	cursor.next();
	return state;
}

#define POP_PAYLOAD_CASE(T)\
	case T:\
		{\
			event->SetPayload<T>(PopPayload<typename odc::EventTypePayload<T>::type>(cursor));\
			break;\
		}
void CBORDeserialiser::PopPayload(cbor::cbor_bytes_cursor& cursor, std::shared_ptr<odc::EventInfo> event)
{
	switch(event->GetEventType())
	{
		POP_PAYLOAD_CASE(odc::EventType::Binary                   )
		POP_PAYLOAD_CASE(odc::EventType::DoubleBitBinary          )
		POP_PAYLOAD_CASE(odc::EventType::Analog                   )
		POP_PAYLOAD_CASE(odc::EventType::Counter                  )
		POP_PAYLOAD_CASE(odc::EventType::FrozenCounter            )
		POP_PAYLOAD_CASE(odc::EventType::BinaryOutputStatus       )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputStatus       )
		POP_PAYLOAD_CASE(odc::EventType::BinaryCommandEvent       )
		POP_PAYLOAD_CASE(odc::EventType::AnalogCommandEvent       )
		POP_PAYLOAD_CASE(odc::EventType::OctetString              )
		POP_PAYLOAD_CASE(odc::EventType::TimeAndInterval          )
		POP_PAYLOAD_CASE(odc::EventType::SecurityStat             )
		POP_PAYLOAD_CASE(odc::EventType::ControlRelayOutputBlock  )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputInt16        )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputInt32        )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputFloat32      )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputDouble64     )
		POP_PAYLOAD_CASE(odc::EventType::BinaryQuality            )
		POP_PAYLOAD_CASE(odc::EventType::DoubleBitBinaryQuality   )
		POP_PAYLOAD_CASE(odc::EventType::AnalogQuality            )
		POP_PAYLOAD_CASE(odc::EventType::CounterQuality           )
		POP_PAYLOAD_CASE(odc::EventType::BinaryOutputStatusQuality)
		POP_PAYLOAD_CASE(odc::EventType::FrozenCounterQuality     )
		POP_PAYLOAD_CASE(odc::EventType::AnalogOutputStatusQuality)
		POP_PAYLOAD_CASE(odc::EventType::OctetStringQuality       )
		POP_PAYLOAD_CASE(odc::EventType::FileAuth                 )
		POP_PAYLOAD_CASE(odc::EventType::FileCommand              )
		POP_PAYLOAD_CASE(odc::EventType::FileCommandStatus        )
		POP_PAYLOAD_CASE(odc::EventType::FileTransport            )
		POP_PAYLOAD_CASE(odc::EventType::FileTransportStatus      )
		POP_PAYLOAD_CASE(odc::EventType::FileDescriptor           )
		POP_PAYLOAD_CASE(odc::EventType::FileSpecString           )
		POP_PAYLOAD_CASE(odc::EventType::ConnectState             )
		POP_PAYLOAD_CASE(odc::EventType::Reserved1                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved2                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved3                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved4                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved5                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved6                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved8                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved9                )
		POP_PAYLOAD_CASE(odc::EventType::Reserved10               )
		POP_PAYLOAD_CASE(odc::EventType::Reserved11               )
		POP_PAYLOAD_CASE(odc::EventType::Reserved12               )
		default:
			throw std::runtime_error("Can't parse Payload for unknown EventType "+ToString(event->GetEventType()));
	}
}
