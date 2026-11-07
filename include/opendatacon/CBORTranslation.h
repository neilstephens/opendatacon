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
 * CBORTranslation.h
 *
 *  CBOR (de)serialisers. A CBOR structure is described by a JSON document with the same shape
 *  as the CBOR, where string values may be placeholders (eg. "INDEX", "PAYLOAD", "POINT:name").
 */

#ifndef ODC_CBORTRANSLATION_H
#define ODC_CBORTRANSLATION_H

#include <opendatacon/Translation.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odc
{

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

class CBORDeserialiser;

class CBORSerialiser: public ISerialiser
{
	friend class CBORDeserialiser;

public:
	//throws if json_string can't be parsed
	explicit CBORSerialiser(const std::string& json_string, const TranslationConfig& config = TranslationConfig());
	std::vector<uint8_t> Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields) const override;
	std::string FindNonStaticElement(SourceLookupMethod lookup) const override;

private:
	EncoderSequence Ops;
	std::vector<std::string> Strings;
	std::vector<uint64_t> UInts;
	std::vector<int64_t> Ints;
	std::vector<double> Doubles;
	std::vector<uint16_t> Halfs;
	std::vector<bool> Bools;
	std::vector<std::string> PointFields;
	const TranslationConfig Config;
	LogHelpers Log;

	void CheckForPlaceholder(const std::string_view& str, bool isKey = false);
};

class CBORDeserialiser: public IDeserialiser
{
public:
	CBORDeserialiser(std::shared_ptr<const CBORSerialiser> serialiser, const TranslationConfig& config);
	std::shared_ptr<EventInfo> Deserialise(const uint8_t* data, size_t size, const KeyContext& key) override;

private:
	const std::shared_ptr<const CBORSerialiser> pSerialiser;
	const TranslationConfig Config;
	LogHelpers Log;
};

} //namespace odc

#endif // ODC_CBORTRANSLATION_H
