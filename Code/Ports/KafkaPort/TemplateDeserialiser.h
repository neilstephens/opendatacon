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
 * TemplateDeserialiser.h
 *
 *  Created on: 2024-12-08
 *      Author: Neil Stephens
 */

#ifndef TEMPLATE_DESERIALISER_H
#define TEMPLATE_DESERIALISER_H

#include "Deserialiser.h"
#include <opendatacon/util.h>
#include <string>
#include <map>
#include <regex>

//using namespace odc;

//TODO: move the code to a cpp file

//function to find all positions of a string in a string
inline std::vector<size_t> FindAll(const std::string& data, const std::string& toSearch)
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
inline void ReplaceAll(std::string& data, const std::string& toSearch, const std::string& replaceStr)
{
	size_t pos = data.find(toSearch);
	while(pos != std::string::npos)
	{
		data.replace(pos, toSearch.size(), replaceStr);
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}

//function to escape special regex chars
inline std::string EscapeRegexLiteral(const std::string &lit)
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

class TemplateDeserialiser: public Deserialiser
{
public:
	TemplateDeserialiser(const std::string& template_str, const std::string& datetime_format, const bool regexEnabled):
		Deserialiser(datetime_format)
	{
		std::string regex_str = regexEnabled ? template_str : EscapeRegexLiteral(template_str);

		//TODO: check for matching groups in the template if it's regex enabled
		// warn (or throw if the check is robust)

		//make whitespace optional an interchangable with other whitespace
		//  ,by replacing all whitespace with \s*
		std::regex whitespace_regex(R"(\s+)");
		regex_str = std::regex_replace(regex_str, whitespace_regex, R"(\s*)");

		//replace <POINT:...> or <SENDERNAME>
		//  with .*? (ungrouped lazy match), because they're not applicable to consuming
		std::regex point_regex(R"(<POINT:[^>]*>|<SENDERNAME>)");
		regex_str = std::regex_replace(regex_str, point_regex, "(?:.*?)");

		//now get the order that the placeholders appear in the template
		//  ,by storing in an ordered map by position
		std::map<size_t,std::string> positions;
		for(const auto& place_holder : {"<SOURCEPORT>", "<EVENTTYPE>", "<EVENTTYPE_RAW>", "<INDEX>", "<TIMESTAMP>", "<DATETIME>", "<QUALITY>", "<QUALITY_RAW>", "<PAYLOAD>"})
			for(const auto& pos : FindAll(regex_str, place_holder))
				positions[pos] = place_holder;

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
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->debug("TemplateDeserialiser regex: '{}'", regex_str);

		try
		{
			template_regex = std::regex(regex_str);
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Failed to compile regex '{}', for template '{}': {}", regex_str, template_str, e.what());
		}
	}
	virtual ~TemplateDeserialiser() = default;

	std::shared_ptr<EventInfo> Deserialise(const KCC::ConsumerRecord& record) override
	{
		std::string val_str(static_cast<const char*>(record.value().data()), record.value().size());
		if(auto log = odc::spdlog_get("KafkaPort"))
			log->trace("Deserialising record value: '{}'", val_str);

		std::smatch matches;
		if(!std::regex_search(val_str, matches, template_regex))
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->warn("Failed to deserialise record (regex_search failed to match): {}", record.toString());
			return nullptr;
		}
		if(matches.size() != capture_groups.size()+1)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Wrong number of group matches when deserialising record: {}", record.toString());
			return nullptr;
		}
		std::map<std::string,std::string> captured_values;
		for(const auto& [place_holder, group_num] : capture_groups)
		{
			captured_values[place_holder] = matches[group_num].str();
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->trace("Captured '{}'(grp{}): '{}'", place_holder, group_num, captured_values[place_holder]);
		}

		EventType event_type;
		try
		{
			//use EVENTTYPE_RAW if available
			if(captured_values.find("<EVENTTYPE_RAW>") != captured_values.end())
			{
				event_type = EventType(std::stoi(captured_values["<EVENTTYPE_RAW>"]));
				if(event_type >= EventType::AfterRange || event_type <= EventType::BeforeRange)
					throw std::runtime_error("Unknown EventType: '"+captured_values["<EVENTTYPE_RAW>"]+"'.");
			}
			else
			{
				//otherwise use EVENTTYPE string
				event_type = EventTypeFromString(captured_values["<EVENTTYPE>"]);
				if(event_type >= EventType::AfterRange)
					throw std::runtime_error("Unknown EventType: '"+captured_values["<EVENTTYPE>"]+"'.");
			}
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Failed to deserialise EventType: {}", e.what());
			return nullptr;
		}

		size_t index;
		try
		{
			index = std::stoul(captured_values["<INDEX>"]);
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Failed to deserialise Index '{}': {}", captured_values["<INDEX>"], e.what());
			return nullptr;
		}

		auto timestamp = msSinceEpoch();
		//use TIMESTAMP if available
		if(captured_values.find("<TIMESTAMP>") != captured_values.end())
		{
			try
			{
				timestamp = std::stoull(captured_values["<TIMESTAMP>"]);
			}
			catch(const std::exception& e)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->error("Failed to deserialise Timestamp '{}': {}", captured_values["<TIMESTAMP>"], e.what());
				return nullptr;
			}
		}
		else if(captured_values.find("<DATETIME>") != captured_values.end())
		{
			try
			{
				timestamp = datetime_to_since_epoch(captured_values["<DATETIME>"], datetime_format);
			}
			catch(const std::exception& e)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->error("Failed to deserialise DateTime '{}' as '{}' : {}", captured_values["<DATETIME>"], datetime_format, e.what());
				return nullptr;
			}
		}

		auto quality = QualityFlags::ONLINE;
		//use QUALITY_RAW if available
		try
		{
			if(captured_values.find("<QUALITY_RAW>") != captured_values.end())
			{
				quality = QualityFlags(std::stoi(captured_values["<QUALITY_RAW>"]));
			}
			else if (captured_values.find("<QUALITY>") != captured_values.end())
			{
				//otherwise use QUALITY string
				quality = QualityFlagsFromString(captured_values["<QUALITY>"]);
			}
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("KafkaPort"))
				log->error("Failed to deserialise Quality: {}", e.what());
			return nullptr;
		}

		std::shared_ptr<EventInfo> event = std::make_shared<EventInfo>(event_type, index, "", quality, timestamp);
		if(captured_values.find("<PAYLOAD>") != captured_values.end())
		{
			try
			{
				event->SetPayload(captured_values["<PAYLOAD>"]);
			}
			catch(const std::exception& e)
			{
				if(auto log = odc::spdlog_get("KafkaPort"))
					log->error("Failed to deserialise Payload: {}", e.what());
				return nullptr;
			}
		}

		//use SOURCEPORT if available
		if(captured_values.find("<SOURCEPORT>") != captured_values.end())
		{
			event->SetSource(captured_values["<SOURCEPORT>"]);
		}

		return event;
	}

private:
	std::regex template_regex;
	std::map<std::string,size_t> capture_groups;
};

#endif // TEMPLATE_DESERIALISER_H