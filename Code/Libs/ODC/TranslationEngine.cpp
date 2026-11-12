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
 * TranslationEngine.cpp
 *
 *  Point mapping and config parsing extracted from KafkaPort
 *      Author: Neil Stephens
 */

#include <opendatacon/TranslationEngine.h>
#include <opendatacon/CBORTranslation.h>
#include <opendatacon/TemplateTranslation.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace odc
{

/*********************************************************************/
/*  Config parsing                                                   */
/*********************************************************************/

void ParseTranslationSettings(const Json::Value& JSONRoot, TranslationSettings& settings)
{
	if(!JSONRoot.isObject()) return;
	LogHelpers Log(settings.Config.LogName);

	auto validateCBOR = [&](const std::string& structure)
				  {
					  //throws if the structure can't be parsed
					  CBORSerialiser test(structure, settings.Config);
				  };

	if(JSONRoot.isMember("DefaultKey"))
		settings.DefaultKey = JSONRoot["DefaultKey"].asString();
	if(JSONRoot.isMember("DefaultTemplate"))
		settings.DefaultTemplate = JSONRoot["DefaultTemplate"].asString();
	if(JSONRoot.isMember("DefaultCBORStructure"))
	{
		settings.DefaultCBORStructure = JSONRoot["DefaultCBORStructure"].asString();
		validateCBOR(settings.DefaultCBORStructure);
	}
	if(JSONRoot.isMember("DefaultLua"))
		settings.DefaultLua = JSONRoot["DefaultLua"].asString();
	if(JSONRoot.isMember("DefaultLuaFile"))
		settings.DefaultLuaFile = JSONRoot["DefaultLuaFile"].asString();
	if(JSONRoot.isMember("DefaultKeyTemplate"))
		settings.DefaultKeyTemplate = JSONRoot["DefaultKeyTemplate"].asString();
	if(JSONRoot.isMember("DefaultKeyCBORStructure"))
	{
		settings.DefaultKeyCBORStructure = JSONRoot["DefaultKeyCBORStructure"].asString();
		validateCBOR(settings.DefaultKeyCBORStructure);
	}
	if(JSONRoot.isMember("DefaultKeyLua"))
		settings.DefaultKeyLua = JSONRoot["DefaultKeyLua"].asString();
	if(JSONRoot.isMember("DefaultKeyLuaFile"))
		settings.DefaultKeyLuaFile = JSONRoot["DefaultKeyLuaFile"].asString();

	if(JSONRoot.isMember("TranslationMethod"))
	{
		if(JSONRoot["TranslationMethod"].asString() == "Lua")
			settings.TranslationMethod = EventTranslationMethod::Lua;
		else if(JSONRoot["TranslationMethod"].asString() == "Template")
			settings.TranslationMethod = EventTranslationMethod::Template;
		else if(JSONRoot["TranslationMethod"].asString() == "CBOR")
			settings.TranslationMethod = EventTranslationMethod::CBOR;
		else
		{
			Log.Error("Unknown TranslationMethod '{}'. Defaulting to Template.", JSONRoot["TranslationMethod"].asString());
			settings.TranslationMethod = EventTranslationMethod::Template;
		}
	}
	if(JSONRoot.isMember("KeyTranslationMethod"))
	{
		if(JSONRoot["KeyTranslationMethod"].asString() == "Literal")
			settings.KeyMethod = KeyTranslationMethod::Literal;
		else if(JSONRoot["KeyTranslationMethod"].asString() == "Template")
			settings.KeyMethod = KeyTranslationMethod::Template;
		else if(JSONRoot["KeyTranslationMethod"].asString() == "CBOR")
			settings.KeyMethod = KeyTranslationMethod::CBOR;
		else if(JSONRoot["KeyTranslationMethod"].asString() == "Lua")
			settings.KeyMethod = KeyTranslationMethod::Lua;
		else
		{
			Log.Error("Unknown KeyTranslationMethod '{}'. Defaulting to Literal.", JSONRoot["KeyTranslationMethod"].asString());
			settings.KeyMethod = KeyTranslationMethod::Literal;
		}
	}
	if(JSONRoot.isMember("KeyClashDetection"))
	{
		if(JSONRoot["KeyClashDetection"].asString() == "STRICT")
			settings.ClashDetection = KeyClashDetection::Strict;
		else if(JSONRoot["KeyClashDetection"].asString() == "WARN")
			settings.ClashDetection = KeyClashDetection::Warn;
		else if(JSONRoot["KeyClashDetection"].asString() == "DYNAMIC")
			settings.ClashDetection = KeyClashDetection::Dynamic;
		else
		{
			Log.Error("Unknown KeyClashDetection '{}'. Defaulting to WARN.", JSONRoot["KeyClashDetection"].asString());
			settings.ClashDetection = KeyClashDetection::Warn;
		}
	}
	if(JSONRoot.isMember("PointTranslationSource"))
	{
		if(JSONRoot["PointTranslationSource"].asString() == "SenderName")
			settings.PointTranslationSource = SourceLookupMethod::SenderName;
		else if(JSONRoot["PointTranslationSource"].asString() == "SourcePort")
			settings.PointTranslationSource = SourceLookupMethod::SourcePort;
		else if(JSONRoot["PointTranslationSource"].asString() == "None")
			settings.PointTranslationSource = SourceLookupMethod::None;
		else
		{
			Log.Error("Unknown PointTranslationSource '{}'. Defaulting to None.", JSONRoot["PointTranslationSource"].asString());
			settings.PointTranslationSource = SourceLookupMethod::None;
		}
	}
	if(JSONRoot.isMember("OctetStringFormat"))
	{
		auto fmt = JSONRoot["OctetStringFormat"].asString();
		if(fmt == "Hex")
			settings.Config.OctetStringFormat = DataToStringMethod::Hex;
		else if(fmt == "Raw")
			settings.Config.OctetStringFormat = DataToStringMethod::Raw;
		else if(fmt == "Base64")
			settings.Config.OctetStringFormat = DataToStringMethod::Base64;
		else Log.Error("Unknown OctetStringFormat '{}', should be Raw, Hex or Base64. Using the default.", fmt);
	}
	if(JSONRoot.isMember("DateTimeFormat"))
		settings.Config.DateTimeFormat = JSONRoot["DateTimeFormat"].asString();
	if(JSONRoot.isMember("DateTimeIsUTC"))
		settings.Config.DateTimeIsUTC = JSONRoot["DateTimeIsUTC"].asBool();
	if(JSONRoot.isMember("RegexEscapeTemplates"))
		settings.Config.RegexEscapeTemplates = JSONRoot["RegexEscapeTemplates"].asBool();
}

void ParsePointTranslationMap(const Json::Value& JSON_PTM, PointTranslationMap& ptm, bool overriding, bool OverridesCreateNewPTMEntries, const std::string& LogName, const PointMemberHandler& handler)
{
	LogHelpers Log(LogName);
	for(auto EventTypeStr : JSON_PTM.getMemberNames())
	{
		auto eventType = EventTypeFromString(EventTypeStr);
		if(eventType == odc::EventType::AfterRange)
		{
			Log.Error("Unknown EventType '{}' in PointTranslationMap. Ignoring.", EventTypeStr);
			continue;
		}
		if(!JSON_PTM[EventTypeStr].isArray())
		{
			Log.Error("PointTranslationMap member '{}' should be an array. Ignoring.", EventTypeStr);
			continue;
		}
		for(auto& entry : JSON_PTM[EventTypeStr])
		{
			size_t start, stop, inc=1;
			if(entry.isMember("Index"))
				start = stop = entry["Index"].asUInt();
			else if(entry.isMember("Range") && entry["Range"].isMember("Start") && entry["Range"].isMember("Stop"))
			{
				start = entry["Range"]["Start"].asUInt();
				stop = entry["Range"]["Stop"].asUInt();
				if(entry["Range"].isMember("Increment"))
					inc = entry["Range"]["Increment"].asUInt();
			}
			else
			{
				Log.Error("A PointTranslationMap entry needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : skipping '{}'", entry.toStyledString());
				continue;
			}
			for(size_t idx = start; idx <= stop; idx+=inc)
			{
				//Work out the TranslationID
				TranslationID tid; auto& [source,point_idx,evt_type] = tid;
				point_idx = idx; evt_type = eventType;
				if(entry.isMember("Source"))
					source = entry["Source"].asString();
				else
					source = "";

				bool existing_point = ptm.find(tid) != ptm.end();
				if(overriding && !existing_point && !OverridesCreateNewPTMEntries)
					continue;

				//Take a copy and update the existing entry, or create a new one
				PointTranslationEntry pte = existing_point ? std::move(ptm.at(tid)) : PointTranslationEntry();

				if(existing_point)
					pte.override = true;

				for(auto pte_member : entry.getMemberNames())
				{
					//Skip members that are already processed
					if(pte_member == "Index" || pte_member == "Range" || pte_member == "Source")
						continue;

					if(pte_member == "Key")
						pte.Key = odc::OctetStringBuffer(entry["Key"].asString());
					else if(pte_member == "Template")
						pte.Template = entry["Template"].asString();
					else if(pte_member == "KeyTemplate")
						pte.KeyTemplate = entry["KeyTemplate"].asString();
					else if(pte_member == "Lua")
						pte.Lua = entry["Lua"].asString();
					else if(pte_member == "LuaFile")
						pte.LuaFile = entry["LuaFile"].asString();
					else if(pte_member == "KeyLua")
						pte.KeyLua = entry["KeyLua"].asString();
					else if(pte_member == "KeyLuaFile")
						pte.KeyLuaFile = entry["KeyLuaFile"].asString();
					else if(pte_member == "CBORStructure" || pte_member == "KeyCBORStructure")
					{
						try
						{
							auto structure = entry[pte_member].toStyledString();
							//throws if the structure can't be parsed
							CBORSerialiser test(structure);
							if(pte_member == "CBORStructure")
								pte.CBORStructure = std::move(structure);
							else
								pte.KeyCBORStructure = std::move(structure);
						}
						catch(std::exception& e)
						{
							Log.Error("Failed to process '{}': {}",pte_member,e.what());
						}
					}
					else if(handler && handler(pte_member, entry, pte))
						continue;
					else
						pte.ExtraFields.insert_or_assign(pte_member, entry[pte_member].asString());
				}
				ptm[tid] = std::move(pte);
			}
		}
	}
}

/*********************************************************************/
/*  Engine                                                           */
/*********************************************************************/

namespace
{

std::string KeyToString(const std::string& key)
{
	std::string ret;
	for(unsigned char c : key)
	{
		if(c >= 0x20 && c < 0x7f)
			ret.push_back(c);
		else
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "\\x%02X", c);
			ret += buf;
		}
	}
	return ret;
}

std::string TidToString(const TranslationID& tid)
{
	const auto& [source,index,type] = tid;
	return ToString(type)+"("+std::to_string(index)+")"+(source.empty() ? "" : " from '"+source+"'");
}

struct ReverseKey
{
	std::string Channel;
	std::string Key;
	bool operator==(const ReverseKey& o) const { return Channel == o.Channel && Key == o.Key; }
};
struct ReverseKeyHash
{
	size_t operator()(const ReverseKey& k) const
	{
		auto h1 = std::hash<std::string>()(k.Channel);
		auto h2 = std::hash<std::string>()(k.Key);
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
	}
};

}

struct TranslationEngine::Impl
{
	Impl(const TranslationSettings& s): settings(s), Log(s.Config.LogName) {}

	struct ForwardPoint
	{
		std::string Channel;
		std::shared_ptr<ISerialiser> Serialiser;
		//Only set when the key has to be generated per event (default point, or a dynamic key)
		std::shared_ptr<ISerialiser> KeySerialiser;
		std::vector<uint8_t> Key;
		std::shared_ptr<const ExtraPointFields> Extra;
	};
	struct ReverseTarget
	{
		std::shared_ptr<IDeserialiser> Deserialiser;
		std::optional<PointIdentity> Hint;
		//Set if the deserialiser has <POINT:name> fields that can identify the point
		std::shared_ptr<const PointResolver> Resolver;
	};

	const TranslationSettings settings;
	LogHelpers Log;

	ForwardPoint DefaultPoint;
	std::map<TranslationID,ForwardPoint> Forward;
	std::unordered_map<ReverseKey,ReverseTarget,ReverseKeyHash> Reverse;
	std::shared_ptr<IDeserialiser> FallbackDeserialiser;
	std::set<std::string> ChannelSet;

	//caches, so that points with identical specs share (de)serialisers
	std::map<std::string,std::shared_ptr<ISerialiser>> SerialiserCache;
	std::map<std::string,std::shared_ptr<IDeserialiser>> DeserialiserCache;

	static std::string ReadFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::binary);
		if(!file)
			throw std::runtime_error("Failed to open Lua file '"+filename+"'");
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	//work out the spec: a per-point one if there is one, otherwise the default
	static TranslationFactory::Spec LuaSpec(const std::optional<std::string>& code, const std::optional<std::string>& file, const std::string& default_code, const std::string& default_file)
	{
		const bool point_has = (code && !code->empty()) || (file && !file->empty());
		const std::string& use_code = point_has ? (code ? *code : "") : default_code;
		const std::string& use_file = point_has ? (file ? *file : "") : default_file;
		TranslationFactory::Spec spec;
		if(!use_file.empty())
		{
			spec.Content = ReadFile(use_file);
			spec.Origin = use_file;
			return spec;
		}
		if(use_code.empty())
			throw std::runtime_error("Lua translation selected, but no Lua code or file is configured");
		spec.Content = use_code;
		spec.Origin = "inline Lua";
		return spec;
	}

	TranslationFactory::Spec PayloadSpec(const PointTranslationEntry* pte) const
	{
		switch(settings.TranslationMethod)
		{
			case EventTranslationMethod::Template:
				return {pte && pte->Template ? *pte->Template : settings.DefaultTemplate, ""};
			case EventTranslationMethod::CBOR:
				return {pte && pte->CBORStructure ? *pte->CBORStructure : settings.DefaultCBORStructure, ""};
			default:
				return LuaSpec(pte ? pte->Lua : std::nullopt, pte ? pte->LuaFile : std::nullopt, settings.DefaultLua, settings.DefaultLuaFile);
		}
	}

	//returns false for literal keys, which don't use a key serialiser
	bool KeySpec(const PointTranslationEntry* pte, EventTranslationMethod& method, TranslationFactory::Spec& spec) const
	{
		switch(settings.KeyMethod)
		{
			case KeyTranslationMethod::Literal:
				return false;
			case KeyTranslationMethod::Template:
				method = EventTranslationMethod::Template;
				spec = {pte && pte->KeyTemplate ? *pte->KeyTemplate : settings.DefaultKeyTemplate, ""};
				return true;
			case KeyTranslationMethod::CBOR:
				method = EventTranslationMethod::CBOR;
				spec = {pte && pte->KeyCBORStructure ? *pte->KeyCBORStructure : settings.DefaultKeyCBORStructure, ""};
				return true;
			default:
				method = EventTranslationMethod::Lua;
				spec = LuaSpec(pte ? pte->KeyLua : std::nullopt, pte ? pte->KeyLuaFile : std::nullopt, settings.DefaultKeyLua, settings.DefaultKeyLuaFile);
				return true;
		}
	}

	static std::string CacheKey(EventTranslationMethod method, TranslationRole role, const TranslationFactory::Spec& spec)
	{
		return std::to_string(static_cast<int>(method))+(role == TranslationRole::Key ? "K" : "P")+":"+spec.Content;
	}

	std::shared_ptr<ISerialiser> GetSerialiser(EventTranslationMethod method, TranslationRole role, const TranslationFactory::Spec& spec)
	{
		auto ck = CacheKey(method, role, spec);
		auto it = SerialiserCache.find(ck);
		if(it != SerialiserCache.end())
			return it->second;
		auto pSer = TranslationFactory::CreateSerialiser(method, spec, settings.Config, role);
		SerialiserCache[ck] = pSer;
		return pSer;
	}

	std::shared_ptr<IDeserialiser> GetDeserialiser(EventTranslationMethod method, const TranslationFactory::Spec& spec)
	{
		auto ck = CacheKey(method, TranslationRole::Payload, spec);
		auto it = DeserialiserCache.find(ck);
		if(it != DeserialiserCache.end())
			return it->second;
		auto pDes = TranslationFactory::CreateDeserialiser(method, spec, settings.Config);
		DeserialiserCache[ck] = pDes;
		return pDes;
	}

	//Gets a key serialiser, and checks that it's static (unless dynamic keys are allowed)
	//	returns whether it's static
	bool CheckKeySerialiser(const std::shared_ptr<ISerialiser>& pKeySer, const std::string& what)
	{
		auto non_static = pKeySer->FindNonStaticElement(settings.PointTranslationSource);
		if(non_static.empty())
			return true;
		if(settings.ClashDetection != KeyClashDetection::Dynamic)
			throw std::runtime_error("Key for "+what+" uses '"+non_static+"', which isn't static for a point. Only KeyClashDetection DYNAMIC allows non-static keys.");
		return false;
	}

	std::vector<uint8_t> ResolveKey(const std::shared_ptr<ISerialiser>& pKeySer, const TranslationID& tid, const ExtraPointFields& extra) const
	{
		const auto& [source,index,type] = tid;
		auto event = std::make_shared<EventInfo>(type, index, settings.PointTranslationSource == SourceLookupMethod::SourcePort ? source : "", QualityFlags::ONLINE, 0);
		event->SetPayload(); //default payload, so a script can't trip over a missing one
		return pKeySer->Serialise(event, settings.PointTranslationSource == SourceLookupMethod::SenderName ? source : "", extra);
	}

	void BuildForward(const PointTranslationMap& ptm)
	{
		auto empty_extra = std::make_shared<const ExtraPointFields>();

		//The default point (for events that aren't in the map)
		DefaultPoint.Channel = settings.DefaultChannel;
		DefaultPoint.Extra = empty_extra;
		DefaultPoint.Serialiser = GetSerialiser(settings.TranslationMethod, TranslationRole::Payload, PayloadSpec(nullptr));
		{
			EventTranslationMethod km; TranslationFactory::Spec ks;
			if(KeySpec(nullptr, km, ks))
			{
				DefaultPoint.KeySerialiser = GetSerialiser(km, TranslationRole::Key, ks);
				CheckKeySerialiser(DefaultPoint.KeySerialiser, "default key");
			}
			else
			{
				DefaultPoint.Key.assign(static_cast<const uint8_t*>(settings.DefaultKey.data()),
					static_cast<const uint8_t*>(settings.DefaultKey.data())+settings.DefaultKey.size());
			}
		}

		for(const auto& [tid, pte] : ptm)
		{
			ForwardPoint fp;
			fp.Channel = pte.Channel ? *pte.Channel : settings.DefaultChannel;
			fp.Extra = std::make_shared<const ExtraPointFields>(pte.ExtraFields);
			try
			{
				fp.Serialiser = GetSerialiser(settings.TranslationMethod, TranslationRole::Payload, PayloadSpec(&pte));
				ResolvePointKey(tid, pte, fp);
			}
			catch(const std::exception& e)
			{
				throw std::runtime_error("Failed to build translation for "+TidToString(tid)+": "+e.what());
			}
			Forward.emplace(tid, std::move(fp));
		}
	}

	//fill in the key part of a ForwardPoint. Returns false if the key is dynamic.
	bool ResolvePointKey(const TranslationID& tid, const PointTranslationEntry& pte, ForwardPoint& fp)
	{
		EventTranslationMethod km; TranslationFactory::Spec ks;
		if(!KeySpec(&pte, km, ks))
		{
			const auto& lit = pte.Key ? *pte.Key : settings.DefaultKey;
			fp.Key.assign(static_cast<const uint8_t*>(lit.data()), static_cast<const uint8_t*>(lit.data())+lit.size());
			return true;
		}
		auto pKeySer = GetSerialiser(km, TranslationRole::Key, ks);
		if(!CheckKeySerialiser(pKeySer, TidToString(tid)))
		{
			fp.KeySerialiser = pKeySer;
			return false;
		}
		fp.Key = ResolveKey(pKeySer, tid, pte.ExtraFields);
		return true;
	}

	//Index the points that share a deserialiser by the values of the extra fields that it captures (<POINT:name>)
	template <typename Points>
	std::shared_ptr<const PointResolver> BuildResolver(const ReverseKey& rk, const Points& points, const PointTranslationMap& ptm)
	{
		const auto& first_des = points.front().second;
		auto names = first_des->PointFieldNames();
		if(names.empty())
			return nullptr;

		auto pResolver = std::make_shared<PointResolver>();
		pResolver->Names = names;
		for(const auto& [tid, pDes] : points)
		{
			if(pDes != first_des)
				continue; //it's a clash, and the first wins
			const auto& extra = ptm.at(tid).ExtraFields;
			std::vector<std::string> values;
			for(const auto& name : names)
			{
				auto it = extra.find(name);
				if(it == extra.end())
					break;
				values.push_back(it->second);
			}
			if(values.size() != names.size())
				continue; //the point doesn't have all the fields, so can't be identified that way

			PointIdentity id;
			const auto& [source,index,type] = tid;
			id.Type = type;
			id.Index = index;
			if(settings.PointTranslationSource == SourceLookupMethod::SourcePort)
				id.SourcePort = source;

			auto inserted = pResolver->Index.emplace(PointResolver::MakeKey(values), id);
			if(!inserted.second)
			{
				auto msg = "Channel '"+rk.Channel+"', key '"+KeyToString(rk.Key)+"': more than one point has the same <POINT:> field values: "
				           +TidToString(tid);
				if(settings.ClashDetection == KeyClashDetection::Strict)
					throw std::runtime_error(msg);
				if(settings.ClashDetection == KeyClashDetection::Warn)
					Log.Warn("{}. Using the first.", msg);
			}
		}
		return pResolver;
	}

	void BuildReverse(const PointTranslationMap& ptm)
	{
		struct Group
		{
			std::vector<std::pair<TranslationID,std::shared_ptr<IDeserialiser>>> Points;
		};
		std::unordered_map<ReverseKey,Group,ReverseKeyHash> groups;
		std::vector<ReverseKey> order; //so the clash handling is deterministic

		for(const auto& [tid, pte] : ptm)
		{
			try
			{
				ForwardPoint fp;
				if(!ResolvePointKey(tid, pte, fp))
					continue; //dynamic keys can't be reverse looked-up
				auto pDes = GetDeserialiser(settings.TranslationMethod, PayloadSpec(&pte));
				ReverseKey rk{pte.Channel ? *pte.Channel : settings.DefaultChannel, std::string(fp.Key.begin(), fp.Key.end())};
				auto it = groups.find(rk);
				if(it == groups.end())
				{
					order.push_back(rk);
					it = groups.emplace(rk, Group()).first;
				}
				it->second.Points.emplace_back(tid, std::move(pDes));
			}
			catch(const std::exception& e)
			{
				throw std::runtime_error("Failed to build translation for "+TidToString(tid)+": "+e.what());
			}
		}

		for(const auto& rk : order)
		{
			auto& points = groups.at(rk).Points;
			const auto& [first_tid, first_des] = points.front();

			for(const auto& [tid, pDes] : points)
			{
				if(pDes == first_des)
					continue;
				auto msg = "Channel '"+rk.Channel+"', key '"+KeyToString(rk.Key)+"' is used by points with different deserialisers: "
				           +TidToString(first_tid)+" and "+TidToString(tid);
				if(settings.ClashDetection == KeyClashDetection::Strict)
					throw std::runtime_error(msg);
				if(settings.ClashDetection == KeyClashDetection::Warn)
					Log.Warn("{}. Using the first.", msg);
				break;
			}

			ReverseTarget target{first_des, std::nullopt};
			//Only keys that were generated by a key serialiser can identify a point
			//	(a literal key never does, to preserve the behaviour that the payload identifies the point)
			if(points.size() == 1 && settings.KeyMethod != KeyTranslationMethod::Literal)
			{
				const auto& [source,index,type] = first_tid;
				PointIdentity id;
				id.Type = type;
				id.Index = index;
				if(settings.PointTranslationSource == SourceLookupMethod::SourcePort)
					id.SourcePort = source;
				target.Hint = id;
			}
			target.Resolver = BuildResolver(rk, points, ptm);
			Reverse.emplace(rk, std::move(target));
		}

		if(settings.ClashDetection == KeyClashDetection::Dynamic)
			FallbackDeserialiser = GetDeserialiser(settings.TranslationMethod, PayloadSpec(nullptr));
	}
};

TranslationEngine::TranslationEngine(std::unique_ptr<Impl> impl): pImpl(std::move(impl)) {}
TranslationEngine::~TranslationEngine() = default;

std::unique_ptr<const TranslationEngine> TranslationEngine::Create(const TranslationSettings& settings, const PointTranslationMap& ptm, Direction direction)
{
	auto impl = std::make_unique<Impl>(settings);

	impl->ChannelSet.insert(settings.DefaultChannel);
	for(const auto& [tid, pte] : ptm)
		if(pte.Channel)
			impl->ChannelSet.insert(*pte.Channel);

	if(static_cast<uint8_t>(direction) & static_cast<uint8_t>(Direction::Forward))
		impl->BuildForward(ptm);
	if(static_cast<uint8_t>(direction) & static_cast<uint8_t>(Direction::Reverse))
		impl->BuildReverse(ptm);

	return std::unique_ptr<const TranslationEngine>(new TranslationEngine(std::move(impl)));
}

static TranslationID MakeTID(const TranslationSettings& settings, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName)
{
	std::string Source = "";
	if(settings.PointTranslationSource == SourceLookupMethod::SenderName)
		Source = SenderName;
	else if(settings.PointTranslationSource == SourceLookupMethod::SourcePort)
		Source = event->GetSourcePort();
	return TranslationID(Source, event->GetIndex(), event->GetEventType());
}

bool TranslationEngine::IsMapped(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName) const
{
	return pImpl->Forward.find(MakeTID(pImpl->settings, event, SenderName)) != pImpl->Forward.end();
}

TranslationEngine::Message TranslationEngine::Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName) const
{
	auto it = pImpl->Forward.find(MakeTID(pImpl->settings, event, SenderName));
	const auto& fp = it != pImpl->Forward.end() ? it->second : pImpl->DefaultPoint;

	Message msg;
	msg.Channel = fp.Channel;
	msg.Value = fp.Serialiser->Serialise(event, SenderName, *fp.Extra);
	if(fp.KeySerialiser)
		msg.Key = fp.KeySerialiser->Serialise(event, SenderName, *fp.Extra);
	else
		msg.Key = fp.Key;
	return msg;
}

std::shared_ptr<EventInfo> TranslationEngine::Deserialise(const std::string& Channel, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size, bool* matched) const
{
	KeyContext ctx;
	ctx.Key = key;
	ctx.KeySize = key_size;

	IDeserialiser* pDes = nullptr;
	auto it = pImpl->Reverse.find(ReverseKey{Channel, key ? std::string(reinterpret_cast<const char*>(key), key_size) : std::string()});
	if(it != pImpl->Reverse.end())
	{
		pDes = it->second.Deserialiser.get();
		ctx.Hint = it->second.Hint;
		ctx.Resolver = it->second.Resolver.get();
	}
	else if(pImpl->FallbackDeserialiser)
		pDes = pImpl->FallbackDeserialiser.get();

	if(matched)
		*matched = pDes != nullptr;
	if(!pDes)
		return nullptr;
	return pDes->Deserialise(value, value_size, ctx);
}

const std::set<std::string>& TranslationEngine::Channels() const
{
	return pImpl->ChannelSet;
}

} //namespace odc
