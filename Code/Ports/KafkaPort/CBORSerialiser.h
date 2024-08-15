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

#include "EventTranslation.h"
#include <jsoncons_ext/cbor/cbor.hpp>
#include <opendatacon/IOTypes.h>
#include <string>
#include <cstdint>

using namespace odc;
using namespace jsoncons;

enum class EncodeOps : uint8_t
{
	//Serialising Ops
	EVENTTYPE,
	EVENTTYPE_KEY,
	INDEX,
	TIMESTAMP,
	DATETIME,
	DATETIME_KEY,
	QUALITY,
	QUALITY_KEY,
	PAYLOAD,
	PAYLOAD_STRING,
	PAYLOAD_STRING_KEY,
	SOURCEPORT,
	SOURCEPORT_KEY,
	EVENTTYPE_RAW,
	QUALITY_RAW,
	SENDERNAME,
	SENDERNAME_KEY,
	POINT_FIELD,
	POINT_FIELD_KEY,
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
	std::vector<std::string> PointFields;

public:
	CBORSerialiser(const std::string& json_string);
	const std::vector<uint8_t> Encode(std::shared_ptr<const EventInfo> event, const std::string& SenderName, const ExtraPointFields& extra_fields = {}) const;

private:
	void CheckForPlaceholder(const std::string_view& str, bool isKey = false);
	void EncodePayload(std::shared_ptr<const EventInfo> event, cbor::cbor_bytes_encoder& encoder) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, bool payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::DBB payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, double payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, uint32_t payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::CommandStatus payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::OctetStringBuffer payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::TAI payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::SS payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ControlRelayOutputBlock payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO16 payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AO32 payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOF payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::AOD payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::QualityFlags payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, char payload) const;
	void EncodePayload(cbor::cbor_bytes_encoder& encoder, odc::ConnectState payload) const;
};

#endif // CBORSERIALISER_H
