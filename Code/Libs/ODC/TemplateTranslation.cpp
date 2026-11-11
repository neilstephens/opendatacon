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
 * TemplateTranslation.cpp
 *
 *  Extracted from KafkaPort: TemplateDeserialiser.h (2024-12-08) and FillTemplate (2024-08-15)
 *      Author: Neil Stephens
 */

#include <opendatacon/TemplateTranslation.h>
#include <opendatacon/util.h>
#include <cstring>
#include <map>
#include <regex>

namespace odc
{

//function to find all positions of a string in a string
static std::vector<size_t> FindAll(const std::string& data, const std::string& toSearch)
{
	std::vector<size_t> positions;
	size_t pos = data.find(toSearch);
	while(pos != std::string::npos)
	{
		positions.push_back(pos);
		pos = data.find(toSearch, pos + toSearch.size());
	}
	return positions;
}

//function to find and replace all instances of a string in a string
static void ReplaceAll(std::string& data, const std::string& toSearch, const std::string& replaceStr)
{
	size_t pos = data.find(toSearch);
	while(pos != std::string::npos)
	{
		data.replace(pos, toSearch.size(), replaceStr);
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}

//function to escape special regex chars
static std::string EscapeRegexLiteral(const std::string &lit)
{
	static const char metacharacters[] = R"(\.^$+()[]{}|?*)";
	std::string ret;
	ret.reserve(lit.size());
	for(const auto& ch : lit)
	{
		if(std::strchr(metacharacters, ch))
			ret.push_back('\\');
		ret.push_back(ch);
	}
	return ret;
}

/*********************************************************************/
/*  TemplateSerialiser                                               */
/*********************************************************************/

TemplateSerialiser::TemplateSerialiser(const std::string& template_str, const TranslationConfig& config):
	Template(template_str),
	Config(config)
{}

std::string TemplateSerialiser::Fill(const std::string& template_str, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields, const std::string& dt_fmt, const bool utc, const DataToStringMethod D2S)
{
	std::string message = template_str;
	auto find_and_replace = [&](const std::string& fnd, const auto& gen_replacement)
					{
						size_t pos = 0;
						while((pos = message.find(fnd, pos)) != std::string::npos)
						{
							auto rpl = gen_replacement(event);
							message.replace(pos, fnd.length(), rpl);
							pos += rpl.length();
						}
					};
	//Passing a generator lambda to the find_and_replace function means that the expensive string generation is only done if there is a match
	static auto EVENTTYPE = [](const std::shared_ptr<const EventInfo>& event){ return ToString(event->GetEventType()); };
	static auto INDEX = [](const std::shared_ptr<const EventInfo>& event){ return std::to_string(event->GetIndex()); };
	static auto TIMESTAMP = [](const std::shared_ptr<const EventInfo>& event){ return std::to_string(event->GetTimestamp()); };
	static auto QUALITY = [](const std::shared_ptr<const EventInfo>& event){ return ToString(event->GetQuality()); };
	static auto SOURCEPORT = [](const std::shared_ptr<const EventInfo>& event){ return event->GetSourcePort(); };
	static auto EVENTTYPE_RAW = [](const std::shared_ptr<const EventInfo>& event)
					    { return std::to_string(static_cast<std::underlying_type<EventType>::type>(event->GetEventType())); };
	static auto QUALITY_RAW = [](const std::shared_ptr<const EventInfo>& event)
					  { return std::to_string(static_cast<std::underlying_type<QualityFlags>::type>(event->GetQuality())); };

	//these aren't static because they need to capture
	auto SENDERNAME = [&](const std::shared_ptr<const EventInfo>&){ return SenderName; };
	auto DATETIME = [&](const std::shared_ptr<const EventInfo>& event){ return odc::since_epoch_to_datetime(event->GetTimestamp(),dt_fmt,utc); };
	auto PAYLOAD = [&](const std::shared_ptr<const EventInfo>& event){ return event->GetPayloadString(D2S); };

	find_and_replace("<EVENTTYPE>", EVENTTYPE);
	find_and_replace("<INDEX>", INDEX);
	find_and_replace("<TIMESTAMP>", TIMESTAMP);
	find_and_replace("<DATETIME>", DATETIME);
	find_and_replace("<QUALITY>", QUALITY);
	find_and_replace("<PAYLOAD>", PAYLOAD);
	find_and_replace("<SOURCEPORT>", SOURCEPORT);
	find_and_replace("<EVENTTYPE_RAW>", EVENTTYPE_RAW);
	find_and_replace("<QUALITY_RAW>", QUALITY_RAW);
	find_and_replace("<SENDERNAME>", SENDERNAME);
	for(const auto& kv : extra_fields)
		find_and_replace("<POINT:"+kv.first+">", [&](const std::shared_ptr<const EventInfo>&){ return kv.second; });

	return message;
}

std::vector<uint8_t> TemplateSerialiser::Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields) const
{
	auto str = Fill(Template, event, SenderName, extra_fields, Config.DateTimeFormat, Config.DateTimeIsUTC, Config.OctetStringFormat);
	return std::vector<uint8_t>(str.begin(), str.end());
}

std::string TemplateSerialiser::FindNonStaticElement(SourceLookupMethod lookup) const
{
	for(const auto& ph : {"<TIMESTAMP>", "<DATETIME>", "<QUALITY>", "<QUALITY_RAW>", "<PAYLOAD>"})
		if(Template.find(ph) != std::string::npos)
			return ph;
	if(lookup != SourceLookupMethod::SenderName && Template.find("<SENDERNAME>") != std::string::npos)
		return "<SENDERNAME>";
	if(lookup != SourceLookupMethod::SourcePort && Template.find("<SOURCEPORT>") != std::string::npos)
		return "<SOURCEPORT>";
	return "";
}

/*********************************************************************/
/*  TemplateDeserialiser                                             */
/*********************************************************************/

struct TemplateDeserialiser::Impl
{
	Impl(const TranslationConfig& config): Config(config), Log(config.LogName) {}
	const TranslationConfig Config;
	LogHelpers Log;
	std::regex template_regex;
	std::map<std::string,size_t> capture_groups;
};

TemplateDeserialiser::TemplateDeserialiser(const std::string& template_str, const TranslationConfig& config):
	pImpl(std::make_unique<Impl>(config))
{
	auto& Log = pImpl->Log;
	auto& capture_groups = pImpl->capture_groups;

	std::string regex_str = config.RegexEscapeTemplates ? EscapeRegexLiteral(template_str) : template_str;

	//TODO: check for matching groups in the template if it's regex enabled
	//	matching groups in the user supplied regex would screw up the deserialisation
	//	because inserting a group for each placeholder is how that works (see below)
	// warn (or throw if the check is robust)

	//make whitespace optional, and interchangable with other whitespace
	//  ,by replacing all whitespace with \s*
	std::regex whitespace_regex(R"(\s+)");
	regex_str = std::regex_replace(regex_str, whitespace_regex, R"(\s*)");

	//replace <SENDERNAME> with .*? (ungrouped lazy match), because it's not applicable to consuming
	std::regex sender_regex(R"(<SENDERNAME>)");
	regex_str = std::regex_replace(regex_str, sender_regex, "(?:.*?)");

	//now get the order that the placeholders appear in the template
	//  ,by storing in an ordered map by position
	std::map<size_t,std::string> positions;
	for(const auto& place_holder : {"<SOURCEPORT>", "<EVENTTYPE>", "<EVENTTYPE_RAW>", "<INDEX>", "<TIMESTAMP>", "<DATETIME>", "<QUALITY>", "<QUALITY_RAW>", "<PAYLOAD>"})
		for(const auto& pos : FindAll(regex_str, place_holder))
			positions[pos] = place_holder;
	//<POINT:name> placeholders are captured too, to identify the point from its extra fields
	std::regex point_regex(R"(<POINT:[^>]*>)");
	for(auto it = std::sregex_iterator(regex_str.begin(), regex_str.end(), point_regex); it != std::sregex_iterator(); ++it)
		positions[it->position()] = it->str();

	//now replace the placeholders with capturing groups, and store the group number
	size_t group_num = 1;
	for(const auto& [_, place_holder] : positions)
	{
		if(capture_groups.find(place_holder) == capture_groups.end())
		{
			//store the first instance of each placeholder as a capture group
			capture_groups[place_holder] = group_num++;
			size_t pos = regex_str.find(place_holder);
			regex_str.replace(pos, place_holder.size(), "(.*?)");
			//replace all other instances of the same placeholder with a non-capturing lazy match
			ReplaceAll(regex_str, place_holder, "(?:.*?)");
		}
	}
	//and finally add start/end anchors
	regex_str = "^"+regex_str+"$";
	Log.Debug("TemplateDeserialiser regex: '{}'", regex_str);

	try
	{
		pImpl->template_regex = std::regex(regex_str);
	}
	catch(const std::exception& e)
	{
		Log.Error("Failed to compile regex '{}', for template '{}': {}", regex_str, template_str, e.what());
	}
}

TemplateDeserialiser::~TemplateDeserialiser() = default;

std::vector<std::string> TemplateDeserialiser::PointFieldNames() const
{
	//in order of group number, which is the order of appearance
	std::map<size_t,std::string> by_group;
	for(const auto& [place_holder, group_num] : pImpl->capture_groups)
		if(place_holder.rfind("<POINT:", 0) == 0)
			by_group[group_num] = place_holder.substr(7, place_holder.size()-8);
	std::vector<std::string> names;
	for(const auto& [_, name] : by_group)
		names.push_back(name);
	return names;
}

std::shared_ptr<EventInfo> TemplateDeserialiser::Deserialise(const uint8_t* data, size_t size, const KeyContext& key)
{
	auto& Log = pImpl->Log;
	auto& Config = pImpl->Config;
	auto& capture_groups = pImpl->capture_groups;

	std::string val_str(reinterpret_cast<const char*>(data), size);
	if(Log.ShouldLog(spdlog::level::trace))
		Log.Trace("Deserialising record value: '{}'", val_str);

	std::smatch matches;
	if(!std::regex_search(val_str, matches, pImpl->template_regex))
	{
		Log.Warn("Failed to deserialise record (regex_search failed to match): '{}'", val_str);
		return nullptr;
	}
	if(matches.size() != capture_groups.size()+1)
	{
		Log.Error("Wrong number of group matches when deserialising record: '{}'", val_str);
		return nullptr;
	}
	std::map<std::string,std::string> captured_values;
	for(const auto& [place_holder, group_num] : capture_groups)
	{
		captured_values[place_holder] = matches[group_num].str();
		if(Log.ShouldLog(spdlog::level::trace))
			Log.Trace("Captured '{}'(grp{}): '{}'", place_holder, group_num, captured_values[place_holder]);
	}
	auto has = [&](const char* ph){ return captured_values.find(ph) != captured_values.end(); };

	//If the key doesn't identify the point, maybe the <POINT:name> fields do
	std::optional<PointIdentity> resolved;
	if(!key.Hint && key.Resolver)
	{
		std::vector<std::string> values;
		for(const auto& name : key.Resolver->Names)
		{
			auto it = captured_values.find("<POINT:"+name+">");
			if(it == captured_values.end())
				break;
			values.push_back(it->second);
		}
		if(values.size() == key.Resolver->Names.size() && !values.empty())
		{
			resolved = key.Resolver->Resolve(values);
			if(!resolved)
				Log.Debug("No point with the <POINT:> field values found in: '{}'", val_str);
		}
	}
	const bool have_fallback = key.Hint || resolved;

	EventType event_type = EventType::BeforeRange;
	bool have_event_type = false;
	try
	{
		//use EVENTTYPE_RAW if available
		if(has("<EVENTTYPE_RAW>"))
		{
			event_type = EventType(std::stoi(captured_values["<EVENTTYPE_RAW>"]));
			if(event_type >= EventType::AfterRange || event_type <= EventType::BeforeRange)
				throw std::runtime_error("Unknown EventType: '"+captured_values["<EVENTTYPE_RAW>"]+"'.");
			have_event_type = true;
		}
		else if(has("<EVENTTYPE>") || !have_fallback)
		{
			//otherwise use EVENTTYPE string
			event_type = EventTypeFromString(captured_values["<EVENTTYPE>"]);
			if(event_type >= EventType::AfterRange)
				throw std::runtime_error("Unknown EventType: '"+captured_values["<EVENTTYPE>"]+"'.");
			have_event_type = true;
		}
	}
	catch(const std::exception& e)
	{
		Log.Error("Failed to deserialise EventType: {}", e.what());
		return nullptr;
	}
	if(key.Hint)
	{
		if(have_event_type && event_type != key.Hint->Type)
			Log.Warn("Payload EventType {} doesn't match the key's EventType {}. Using the key's.", ToString(event_type), ToString(key.Hint->Type));
		event_type = key.Hint->Type;
	}
	else if(!have_event_type)
		event_type = resolved->Type;

	size_t index = 0;
	bool have_index = false;
	if(has("<INDEX>") || !have_fallback)
	{
		try
		{
			index = std::stoul(captured_values["<INDEX>"]);
			have_index = true;
		}
		catch(const std::exception& e)
		{
			Log.Error("Failed to deserialise Index '{}': {}", captured_values["<INDEX>"], e.what());
			return nullptr;
		}
	}
	if(key.Hint)
	{
		if(have_index && index != key.Hint->Index)
			Log.Warn("Payload Index {} doesn't match the key's Index {}. Using the key's.", index, key.Hint->Index);
		index = key.Hint->Index;
	}
	else if(!have_index)
		index = resolved->Index;

	auto timestamp = msSinceEpoch();
	//use TIMESTAMP if available
	if(has("<TIMESTAMP>"))
	{
		try
		{
			timestamp = std::stoull(captured_values["<TIMESTAMP>"]);
		}
		catch(const std::exception& e)
		{
			Log.Error("Failed to deserialise Timestamp '{}': {}", captured_values["<TIMESTAMP>"], e.what());
			return nullptr;
		}
	}
	else if(has("<DATETIME>"))
	{
		try
		{
			timestamp = datetime_to_since_epoch(captured_values["<DATETIME>"], Config.DateTimeFormat, Config.DateTimeIsUTC);
		}
		catch(const std::exception& e)
		{
			Log.Error("Failed to deserialise DateTime '{}' as '{}' (UTC: {}): {}", captured_values["<DATETIME>"], Config.DateTimeFormat, Config.DateTimeIsUTC, e.what());
			return nullptr;
		}
	}

	auto quality = QualityFlags::ONLINE;
	//use QUALITY_RAW if available
	try
	{
		if(has("<QUALITY_RAW>"))
		{
			quality = QualityFlags(std::stoi(captured_values["<QUALITY_RAW>"]));
		}
		else if (has("<QUALITY>"))
		{
			//otherwise use QUALITY string
			quality = QualityFlagsFromString(captured_values["<QUALITY>"]);
		}
	}
	catch(const std::exception& e)
	{
		Log.Error("Failed to deserialise Quality: {}", e.what());
		return nullptr;
	}

	std::shared_ptr<EventInfo> event = std::make_shared<EventInfo>(event_type, index, "", quality, timestamp);
	if(has("<PAYLOAD>"))
	{
		try
		{
			event->SetPayload(captured_values["<PAYLOAD>"]);
		}
		catch(const std::exception& e)
		{
			Log.Error("Failed to deserialise Payload: {}", e.what());
			return nullptr;
		}
	}

	//use SOURCEPORT if available
	if(has("<SOURCEPORT>"))
		event->SetSource(captured_values["<SOURCEPORT>"]);
	if(key.Hint && key.Hint->SourcePort)
	{
		if(has("<SOURCEPORT>") && captured_values["<SOURCEPORT>"] != *key.Hint->SourcePort)
			Log.Warn("Payload SourcePort '{}' doesn't match the key's SourcePort '{}'. Using the key's.", captured_values["<SOURCEPORT>"], *key.Hint->SourcePort);
		event->SetSource(*key.Hint->SourcePort);
	}
	else if(resolved && resolved->SourcePort && !has("<SOURCEPORT>"))
		event->SetSource(*resolved->SourcePort);

	return event;
}

} //namespace odc
