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
 * TranslationEngine.h
 *
 *  Point mapping + (de)serialisation for ports that translate EventInfo to/from keyed protocol messages.
 *
 *  A message has a channel (eg. Kafka topic, MQTT topic), a key, and a value (payload).
 *    - Forward (ODC -> protocol): the point (source,index,type) determines the channel, key and payload serialiser.
 *    - Reverse (protocol -> ODC): the (channel,key) selects the deserialiser. The key may also say which point
 *      the message is for (see KeyContext::Hint), otherwise the payload does.
 *
 *  Keys are static by default: a point's key is generated once from its identity, cached, and hashed for the
 *  reverse lookup. KeyClashDetection::DYNAMIC relaxes this.
 */

#ifndef ODC_TRANSLATIONENGINE_H
#define ODC_TRANSLATIONENGINE_H

#include <opendatacon/Translation.h>
#include <json/json.h>
#include <map>
#include <set>
#include <tuple>

namespace odc
{

//Port level translation options (and defaults for points that don't override them)
struct TranslationSettings
{
	EventTranslationMethod TranslationMethod = EventTranslationMethod::Template;
	KeyTranslationMethod KeyMethod = KeyTranslationMethod::Literal;
	TranslationConfig Config;
	SourceLookupMethod PointTraslationSource = SourceLookupMethod::None;
	KeyClashDetection ClashDetection = KeyClashDetection::WARN;

	std::string DefaultChannel = "";

	std::string DefaultTemplate = DefaultJSONwPlaceholders();
	std::string DefaultCBORStructure = odc::DefaultCBORStructure();
	std::string DefaultLua = "";
	std::string DefaultLuaFile = "";

	OctetStringBuffer DefaultKey;
	std::string DefaultKeyTemplate = "";
	std::string DefaultKeyCBORStructure = "";
	std::string DefaultKeyLua = "";
	std::string DefaultKeyLuaFile = "";
};

using SourceID = std::string;
using PointIndex = size_t;
using TranslationID = std::tuple<SourceID,PointIndex,odc::EventType>;

//Per point overrides of the port level defaults
struct PointTranslationEntry
{
	bool override = false;
	std::optional<std::string> Channel;
	std::optional<OctetStringBuffer> Key;
	std::optional<std::string> Template;
	std::optional<std::string> CBORStructure;
	std::optional<std::string> Lua;
	std::optional<std::string> LuaFile;
	std::optional<std::string> KeyTemplate;
	std::optional<std::string> KeyCBORStructure;
	std::optional<std::string> KeyLua;
	std::optional<std::string> KeyLuaFile;
	ExtraPointFields ExtraFields;
};
using PointTranslationMap = std::map<TranslationID, PointTranslationEntry>;

//Parse the port level options out of a JSON config. Recognised members:
//	TranslationMethod, KeyTranslationMethod, KeyClashDetection, PointTraslationSource,
//	DefaultTemplate, DefaultCBORStructure, DefaultLua, DefaultLuaFile,
//	DefaultKey, DefaultKeyTemplate, DefaultKeyCBORStructure, DefaultKeyLua, DefaultKeyLuaFile,
//	OctetStringFormat, DateTimeFormat, DateTimeIsUTC, RegexEscapeTemplates
//Throws if a CBOR structure is invalid.
void ParseTranslationSettings(const Json::Value& JSONRoot, TranslationSettings& settings);

//Called for each member of a PointTranslationMap entry that the generic parser doesn't recognise.
//	Return true if the member was handled (otherwise it becomes an ExtraPointField).
using PointMemberHandler = std::function<bool (const std::string& member, const Json::Value& entry, PointTranslationEntry& pte)>;

//Parse the "PointTranslationMap" JSON member into ptm (merging with whatever is already there).
//	Entries are objects with "Index" or "Range", optionally "Source", and any of:
//	Template, CBORStructure, Lua, LuaFile, Key, KeyTemplate, KeyCBORStructure, KeyLua, KeyLuaFile
void ParsePointTranslationMap(const Json::Value& JSON_PTM, PointTranslationMap& ptm, bool overriding, bool OverridesCreateNewPTMEntries, const std::string& LogName, const PointMemberHandler& handler = nullptr);

class TranslationEngine
{
public:
	enum class Direction: uint8_t { Forward = 1, Reverse = 2, Both = 3 };

	//Resolves all keys and builds all (de)serialisers.
	//	Throws std::runtime_error on configuration errors
	static std::unique_ptr<const TranslationEngine> Create(const TranslationSettings& settings, const PointTranslationMap& ptm, Direction direction);
	~TranslationEngine();

	struct Message
	{
		std::string Channel;
		std::vector<uint8_t> Key;
		std::vector<uint8_t> Value;
	};

	//Forward
	bool IsMapped(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName) const;
	Message Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName) const;

	//Reverse
	//	returns nullptr if there's no deserialiser or it failed. *matched says which.
	std::shared_ptr<EventInfo> Deserialise(const std::string& Channel, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size, bool* matched = nullptr) const;

	//All the channels the engine knows about (the default, and any per-point ones)
	const std::set<std::string>& Channels() const;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
	explicit TranslationEngine(std::unique_ptr<Impl> impl);
};

} //namespace odc

#endif // ODC_TRANSLATIONENGINE_H
