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
 * TranslationTests.cpp
 */

#include <opendatacon/TranslationEngine.h>
#include <opendatacon/TemplateTranslation.h>
#include <opendatacon/CBORTranslation.h>
#include <catch.hpp>
#include <string>

using namespace odc;

#define SUITE(name) "TranslationTestSuite - " name

namespace
{
std::shared_ptr<EventInfo> MakeAnalog(size_t index, double value, const std::string& source = "SrcPort", msSinceEpoch_t ts = 1700000000123)
{
	auto event = std::make_shared<EventInfo>(EventType::Analog, index, source, QualityFlags::ONLINE, ts);
	event->SetPayload<EventType::Analog>(std::move(value));
	return event;
}

std::string Str(const std::vector<uint8_t>& v)
{
	return std::string(v.begin(), v.end());
}

Json::Value ParseJSON(const std::string& str)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream ss(str);
	REQUIRE(Json::parseFromStream(builder, ss, &root, &errs));
	return root;
}

PointTranslationMap ParsePTM(const std::string& json, const PointMemberHandler& handler = nullptr)
{
	PointTranslationMap ptm;
	ParsePointTranslationMap(ParseJSON(json), ptm, false, false, "opendatacon", handler);
	return ptm;
}

void CheckSameEvent(const std::shared_ptr<EventInfo>& got, const std::shared_ptr<EventInfo>& expected)
{
	REQUIRE(got);
	CHECK(got->GetEventType() == expected->GetEventType());
	CHECK(got->GetIndex() == expected->GetIndex());
	CHECK(got->GetQuality() == expected->GetQuality());
	CHECK(got->GetTimestamp() == expected->GetTimestamp());
	CHECK(got->GetSourcePort() == expected->GetSourcePort());
	CHECK(got->GetPayloadString() == expected->GetPayloadString());
}

auto Bytes(const std::string& s)
{
	return reinterpret_cast<const uint8_t*>(s.data());
}
}

TEST_CASE(SUITE("Template round trip"))
{
	TranslationConfig config;
	TemplateSerialiser ser(DefaultJSONwPlaceholders(), config);
	TemplateDeserialiser des(DefaultJSONwPlaceholders(), config);

	auto event = MakeAnalog(7, 123.5);
	ExtraPointFields extra;
	extra["Name"] = "Point Seven";
	auto bytes = ser.Serialise(event, "SenderA", extra);
	auto text = Str(bytes);
	CHECK(text.find("\"Index\": 7") != std::string::npos);
	CHECK(text.find("Point Seven") != std::string::npos);
	CHECK(text.find("SenderA") != std::string::npos);

	CheckSameEvent(des.Deserialise(bytes.data(), bytes.size(), KeyContext()), event);
}

TEST_CASE(SUITE("Template static check"))
{
	CHECK(TemplateSerialiser("ai/<INDEX>/<EVENTTYPE>/<POINT:x>", {}).FindNonStaticElement(SourceLookupMethod::None) == "");
	CHECK(TemplateSerialiser("<TIMESTAMP>", {}).FindNonStaticElement(SourceLookupMethod::None) == "<TIMESTAMP>");
	CHECK(TemplateSerialiser("a<PAYLOAD>", {}).FindNonStaticElement(SourceLookupMethod::None) == "<PAYLOAD>");
	CHECK(TemplateSerialiser("<SENDERNAME>", {}).FindNonStaticElement(SourceLookupMethod::None) == "<SENDERNAME>");
	CHECK(TemplateSerialiser("<SENDERNAME>", {}).FindNonStaticElement(SourceLookupMethod::SenderName) == "");
	CHECK(TemplateSerialiser("<SOURCEPORT>", {}).FindNonStaticElement(SourceLookupMethod::SenderName) == "<SOURCEPORT>");
	CHECK(TemplateSerialiser("<SOURCEPORT>", {}).FindNonStaticElement(SourceLookupMethod::SourcePort) == "");
}

TEST_CASE(SUITE("Template deserialise with key hint"))
{
	TranslationConfig config;
	//the template doesn't say which point it is, so the key has to
	TemplateDeserialiser des("val=<PAYLOAD>", config);
	std::string payload = "val=99.5";

	KeyContext ctx;
	ctx.Hint = PointIdentity{EventType::Analog, 12, std::string("PortX")};
	auto event = des.Deserialise(Bytes(payload), payload.size(), ctx);
	REQUIRE(event);
	CHECK(event->GetEventType() == EventType::Analog);
	CHECK(event->GetIndex() == 12);
	CHECK(event->GetSourcePort() == "PortX");
	CHECK(event->GetPayload<EventType::Analog>() == 99.5);

	//without the hint it can't work
	CHECK(des.Deserialise(Bytes(payload), payload.size(), KeyContext()) == nullptr);
}

TEST_CASE(SUITE("Template deserialise - key wins over payload"))
{
	TemplateDeserialiser des("t=<EVENTTYPE>,i=<INDEX>,v=<PAYLOAD>", TranslationConfig());
	std::string payload = "t=Binary,i=3,v=1";
	KeyContext ctx;
	ctx.Hint = PointIdentity{EventType::Analog, 8, std::nullopt};
	std::string analog = "t=Analog,i=3,v=1.5";
	auto event = des.Deserialise(Bytes(analog), analog.size(), ctx);
	REQUIRE(event);
	CHECK(event->GetIndex() == 8);
	CHECK(event->GetEventType() == EventType::Analog);
	event = des.Deserialise(Bytes(payload), payload.size(), ctx);
	REQUIRE(event);
	CHECK(event->GetEventType() == EventType::Analog); //key wins, even over the payload's type
	CHECK(event->GetIndex() == 8);
}

TEST_CASE(SUITE("CBOR round trip"))
{
	TranslationConfig config;
	auto pSer = std::make_shared<const CBORSerialiser>(DefaultCBORStructure(), config);
	CBORDeserialiser des(pSer, config);

	auto event = MakeAnalog(21, -5.25);
	auto bytes = pSer->Serialise(event, "Sender", {});
	CheckSameEvent(des.Deserialise(bytes.data(), bytes.size(), KeyContext()), event);
}

TEST_CASE(SUITE("CBOR bad structure throws"))
{
	CHECK_THROWS(CBORSerialiser("[ not json", TranslationConfig()));
}

TEST_CASE(SUITE("CBOR static check"))
{
	CHECK(CBORSerialiser(R"(["EVENTTYPE","INDEX","POINT:x"])").FindNonStaticElement(SourceLookupMethod::None) == "");
	CHECK(CBORSerialiser(R"(["TIMESTAMP"])").FindNonStaticElement(SourceLookupMethod::None) == "TIMESTAMP");
	CHECK(CBORSerialiser(R"(["PAYLOAD"])").FindNonStaticElement(SourceLookupMethod::None) == "PAYLOAD");
	CHECK(CBORSerialiser(R"(["SOURCEPORT"])").FindNonStaticElement(SourceLookupMethod::None) == "SOURCEPORT");
	CHECK(CBORSerialiser(R"(["SOURCEPORT"])").FindNonStaticElement(SourceLookupMethod::SourcePort) == "");
}

TEST_CASE(SUITE("CBOR deserialise with key hint"))
{
	TranslationConfig config;
	//no EVENTTYPE or INDEX in the structure
	auto pSer = std::make_shared<const CBORSerialiser>(R"(["PAYLOAD"])", config);
	CBORDeserialiser des(pSer, config);
	auto bytes = pSer->Serialise(MakeAnalog(1, 3.5), "", {});

	KeyContext ctx;
	ctx.Hint = PointIdentity{EventType::Analog, 44, std::nullopt};
	auto event = des.Deserialise(bytes.data(), bytes.size(), ctx);
	REQUIRE(event);
	CHECK(event->GetIndex() == 44);
	CHECK(event->GetPayload<EventType::Analog>() == 3.5);

	CHECK(des.Deserialise(bytes.data(), bytes.size(), KeyContext()) == nullptr);
}

TEST_CASE(SUITE("Settings parsing"))
{
	TranslationSettings s;
	CHECK(s.TranslationMethod == EventTranslationMethod::Template);
	CHECK(s.KeyMethod == KeyTranslationMethod::Literal);
	CHECK(s.ClashDetection == KeyClashDetection::Warn);

	ParseTranslationSettings(ParseJSON(R"({
		"TranslationMethod": "CBOR",
		"KeyTranslationMethod": "Template",
		"KeyClashDetection": "STRICT",
		"PointTraslationSource": "SenderName",
		"DefaultKey": "mykey",
		"DefaultKeyTemplate": "k/<INDEX>",
		"DefaultLuaFile": "x.lua",
		"OctetStringFormat": "Hex",
		"DateTimeFormat": "%Y",
		"DateTimeIsUTC": true,
		"RegexEscapeTemplates": false
	})"), s);
	CHECK(s.TranslationMethod == EventTranslationMethod::CBOR);
	CHECK(s.KeyMethod == KeyTranslationMethod::Template);
	CHECK(s.ClashDetection == KeyClashDetection::Strict);
	CHECK(s.PointTraslationSource == SourceLookupMethod::SenderName);
	CHECK(s.DefaultKey.size() == 5);
	CHECK(s.DefaultKeyTemplate == "k/<INDEX>");
	CHECK(s.DefaultLuaFile == "x.lua");
	CHECK(s.Config.OctetStringFormat == DataToStringMethod::Hex);
	CHECK(s.Config.DateTimeFormat == "%Y");
	CHECK(s.Config.DateTimeIsUTC);
	CHECK_FALSE(s.Config.RegexEscapeTemplates);

	CHECK_THROWS(ParseTranslationSettings(ParseJSON(R"({"DefaultCBORStructure": "[ nope"})"), s));
}

TEST_CASE(SUITE("Point map parsing"))
{
	std::string topic_seen;
	auto ptm = ParsePTM(R"({
		"Analog": [
			{"Index": 1, "Topic": "t1", "Key": "k1", "Template": "x", "Colour": "red"},
			{"Range": {"Start": 10, "Stop": 14, "Increment": 2}, "Source": "S", "KeyTemplate": "a<INDEX>", "Lua": "code", "LuaFile": "f.lua"}
		],
		"Binary": [ {"Index": 3, "CBORStructure": ["INDEX"], "KeyCBORStructure": ["EVENTTYPE"]} ],
		"Nonsense": [ {"Index": 1} ],
		"Counter": [ {"Foo": 1} ]
	})", [&](const std::string& member, const Json::Value& entry, PointTranslationEntry& pte)
		{
			if(member != "Topic")
				return false;
			pte.Channel = entry[member].asString();
			return true;
		});

	REQUIRE(ptm.size() == 5);
	const auto& p1 = ptm.at({"",1,EventType::Analog});
	CHECK(p1.Channel == "t1");
	REQUIRE(p1.Key);
	CHECK(p1.Key->size() == 2);
	CHECK(p1.Template == "x");
	CHECK(p1.ExtraFields.at("Colour") == "red");
	CHECK(p1.ExtraFields.size() == 1);

	for(size_t i : {10,12,14})
	{
		const auto& p = ptm.at({"S",i,EventType::Analog});
		CHECK(p.KeyTemplate == "a<INDEX>");
		CHECK(p.Lua == "code");
		CHECK(p.LuaFile == "f.lua");
	}
	CHECK(ptm.count({"S",11,EventType::Analog}) == 0);

	const auto& b = ptm.at({"",3,EventType::Binary});
	CHECK(b.CBORStructure.has_value());
	CHECK(b.KeyCBORStructure.has_value());
}

TEST_CASE(SUITE("Point map parsing - overrides"))
{
	auto base = ParsePTM(R"({"Analog": [{"Index": 1, "Template": "base"}, {"Index": 2, "Template": "base2"}]})");

	auto overridden = base;
	ParsePointTranslationMap(ParseJSON(R"({"Analog": [{"Index": 1, "Template": "over"}, {"Index": 9, "Template": "new"}]})"), overridden, true, false, "opendatacon");
	CHECK(overridden.size() == 2);
	CHECK(overridden.at({"",1,EventType::Analog}).Template == "over");
	CHECK(overridden.at({"",1,EventType::Analog}).override);
	CHECK_FALSE(overridden.at({"",2,EventType::Analog}).override);

	overridden = base;
	ParsePointTranslationMap(ParseJSON(R"({"Analog": [{"Index": 9, "Template": "new"}]})"), overridden, true, true, "opendatacon");
	CHECK(overridden.size() == 3);
}

TEST_CASE(SUITE("Engine - forward mapping"))
{
	TranslationSettings s;
	s.DefaultChannel = "defchan";
	s.DefaultKey = OctetStringBuffer(std::string("defkey"));
	s.DefaultTemplate = "d:<INDEX>=<PAYLOAD>";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 5, "Channel": "x", "Key": "pointkey", "Template": "p:<INDEX>=<PAYLOAD>"}]})");

	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);

	auto mapped = MakeAnalog(5, 1.5);
	CHECK(engine->IsMapped(mapped, ""));
	auto msg = engine->Serialise(mapped, "sender");
	CHECK(msg.Channel == "defchan");
	CHECK(Str(msg.Key) == "pointkey");
	CHECK(Str(msg.Value) == "p:5=1.500000");

	auto unmapped = MakeAnalog(6, 2.5);
	CHECK_FALSE(engine->IsMapped(unmapped, ""));
	msg = engine->Serialise(unmapped, "sender");
	CHECK(msg.Channel == "defchan");
	CHECK(Str(msg.Key) == "defkey");
	CHECK(Str(msg.Value) == "d:6=2.500000");
}

TEST_CASE(SUITE("Engine - point lookup source"))
{
	TranslationSettings s;
	s.DefaultTemplate = "default";
	s.PointTraslationSource = SourceLookupMethod::SenderName;
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 1, "Source": "A", "Template": "from A"}]})");
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
	auto event = MakeAnalog(1, 1);
	CHECK(engine->IsMapped(event, "A"));
	CHECK_FALSE(engine->IsMapped(event, "B"));
	CHECK(Str(engine->Serialise(event, "A").Value) == "from A");
	CHECK(Str(engine->Serialise(event, "B").Value) == "default");

	s.PointTraslationSource = SourceLookupMethod::SourcePort;
	engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
	CHECK(engine->IsMapped(MakeAnalog(1, 1, "A"), "whatever"));
	CHECK_FALSE(engine->IsMapped(MakeAnalog(1, 1, "B"), "A"));
}

TEST_CASE(SUITE("Engine - channels"))
{
	TranslationSettings s;
	s.DefaultChannel = "d";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 1, "Channel": "c1"}, {"Index": 2, "Channel": "c2"}, {"Index": 3}]})",
		[](const std::string& m, const Json::Value& e, PointTranslationEntry& pte){ if(m=="Channel"){pte.Channel=e[m].asString(); return true;} return false; });
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);
	CHECK(engine->Channels() == std::set<std::string>{"d","c1","c2"});
}

TEST_CASE(SUITE("Engine - static key template, reverse lookup with hint"))
{
	TranslationSettings s;
	s.DefaultChannel = "chan";
	s.KeyMethod = KeyTranslationMethod::Template;
	s.DefaultKeyTemplate = "site/<EVENTTYPE>/<INDEX>/<POINT:Tag>";
	//the payload only carries the value, the key says which point it is
	s.DefaultTemplate = "<PAYLOAD>";
	auto ptm = ParsePTM(R"({"Analog": [{"Range": {"Start": 1, "Stop": 3}, "Tag": "t"}]})");

	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);

	auto msg = engine->Serialise(MakeAnalog(2, 77.25), "");
	CHECK(Str(msg.Key) == "site/Analog/2/t");
	CHECK(Str(msg.Value) == "77.250000");

	bool matched = false;
	auto event = engine->Deserialise("chan", msg.Key.data(), msg.Key.size(), msg.Value.data(), msg.Value.size(), &matched);
	CHECK(matched);
	REQUIRE(event);
	CHECK(event->GetEventType() == EventType::Analog);
	CHECK(event->GetIndex() == 2);
	CHECK(event->GetPayload<EventType::Analog>() == 77.25);

	//unknown key
	std::string other = "site/Analog/9/t";
	event = engine->Deserialise("chan", Bytes(other), other.size(), msg.Value.data(), msg.Value.size(), &matched);
	CHECK_FALSE(matched);
	CHECK(event == nullptr);
	//right key, wrong channel
	event = engine->Deserialise("nochan", msg.Key.data(), msg.Key.size(), msg.Value.data(), msg.Value.size(), &matched);
	CHECK_FALSE(matched);
}

TEST_CASE(SUITE("Engine - CBOR key"))
{
	TranslationSettings s;
	s.DefaultChannel = "chan";
	s.TranslationMethod = EventTranslationMethod::CBOR;
	s.DefaultCBORStructure = R"(["PAYLOAD"])";
	s.KeyMethod = KeyTranslationMethod::CBOR;
	s.DefaultKeyCBORStructure = R"(["EVENTTYPE_RAW","INDEX"])";
	auto ptm = ParsePTM(R"({"Analog": [{"Range": {"Start": 1, "Stop": 3}}]})");
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);

	auto msg1 = engine->Serialise(MakeAnalog(1, 1.5), "");
	auto msg2 = engine->Serialise(MakeAnalog(2, 1.5), "");
	CHECK(msg1.Key != msg2.Key);
	CHECK(msg1.Key == engine->Serialise(MakeAnalog(1, 9.5, "other", 5), "").Key); //static

	auto event = engine->Deserialise("chan", msg2.Key.data(), msg2.Key.size(), msg2.Value.data(), msg2.Value.size());
	REQUIRE(event);
	CHECK(event->GetIndex() == 2);
	CHECK(event->GetPayload<EventType::Analog>() == 1.5);
}

TEST_CASE(SUITE("Engine - non-static keys"))
{
	TranslationSettings s;
	s.KeyMethod = KeyTranslationMethod::Template;
	s.DefaultKeyTemplate = "k/<INDEX>/<TIMESTAMP>";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 1}]})");

	SECTION("WARN rejects")
	{
		s.ClashDetection = KeyClashDetection::Warn;
		CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
		CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Reverse));
	}
	SECTION("STRICT rejects")
	{
		s.ClashDetection = KeyClashDetection::Strict;
		CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
	}
	SECTION("DYNAMIC evaluates per event")
	{
		s.ClashDetection = KeyClashDetection::Dynamic;
		s.DefaultTemplate = "<PAYLOAD>";
		auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
		CHECK(Str(engine->Serialise(MakeAnalog(1, 1, "s", 100), "").Key) == "k/1/100");
		CHECK(Str(engine->Serialise(MakeAnalog(1, 1, "s", 200), "").Key) == "k/1/200");
		//also for points that aren't mapped
		CHECK(Str(engine->Serialise(MakeAnalog(8, 1, "s", 300), "").Key) == "k/8/300");
	}
}

TEST_CASE(SUITE("Engine - sender/source placeholders in keys"))
{
	TranslationSettings s;
	s.KeyMethod = KeyTranslationMethod::Template;
	s.DefaultKeyTemplate = "<SENDERNAME>/<INDEX>";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 1, "Source": "Snd"}]})");

	s.PointTraslationSource = SourceLookupMethod::None;
	CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
	s.PointTraslationSource = SourceLookupMethod::SourcePort;
	CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
	s.PointTraslationSource = SourceLookupMethod::SenderName;
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(1, 1), "Snd").Key) == "Snd/1");
}

TEST_CASE(SUITE("Engine - key clashes"))
{
	TranslationSettings s;
	s.DefaultChannel = "c";
	s.DefaultKey = OctetStringBuffer(std::string("shared"));
	s.DefaultTemplate = "A:<INDEX>,<EVENTTYPE>,<PAYLOAD>";
	//two points share a key, but have different deserialisers
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 1}, {"Index": 2, "Template": "B:<INDEX>,<EVENTTYPE>,<PAYLOAD>"}]})");

	SECTION("WARN - first wins")
	{
		s.ClashDetection = KeyClashDetection::Warn;
		auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Reverse);
		std::string a = "A:1,Analog,5", b = "B:2,Analog,5";
		auto key = std::string("shared");
		CHECK(engine->Deserialise("c", Bytes(key), key.size(), Bytes(a), a.size()) != nullptr);
		CHECK(engine->Deserialise("c", Bytes(key), key.size(), Bytes(b), b.size()) == nullptr);
	}
	SECTION("STRICT - error")
	{
		s.ClashDetection = KeyClashDetection::Strict;
		CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Reverse));
		//only consumers care
		CHECK_NOTHROW(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
	}
	SECTION("many points with the same deserialiser is fine")
	{
		s.ClashDetection = KeyClashDetection::Strict;
		auto same = ParsePTM(R"({"Analog": [{"Range": {"Start": 1, "Stop": 100}}]})");
		CHECK_NOTHROW(TranslationEngine::Create(s, same, TranslationEngine::Direction::Reverse));
	}
}

TEST_CASE(SUITE("Engine - literal keys never give a hint"))
{
	//Existing behaviour: with a literal key, the payload says which point it is
	TranslationSettings s;
	s.DefaultChannel = "c";
	s.DefaultKey = OctetStringBuffer(std::string("k"));
	s.DefaultTemplate = "<INDEX>,<EVENTTYPE>,<PAYLOAD>";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 5}]})");
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Reverse);
	std::string key = "k", val = "7,Analog,1.5";
	auto event = engine->Deserialise("c", Bytes(key), key.size(), Bytes(val), val.size());
	REQUIRE(event);
	CHECK(event->GetIndex() == 7);
}

TEST_CASE(SUITE("Engine - DYNAMIC falls back to the default deserialiser"))
{
	TranslationSettings s;
	s.DefaultChannel = "c";
	s.ClashDetection = KeyClashDetection::Dynamic;
	s.KeyMethod = KeyTranslationMethod::Template;
	s.DefaultKeyTemplate = "k/<TIMESTAMP>";
	s.DefaultTemplate = "<INDEX>,<EVENTTYPE>,<PAYLOAD>";
	auto ptm = ParsePTM(R"({"Analog": [{"Index": 5}]})");
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);

	std::string key = "k/12345", val = "7,Analog,1.5";
	bool matched = false;
	auto event = engine->Deserialise("anything", Bytes(key), key.size(), Bytes(val), val.size(), &matched);
	CHECK(matched);
	REQUIRE(event);
	CHECK(event->GetIndex() == 7);
}

TEST_CASE(SUITE("Engine - unavailable Lua"))
{
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = "function serialise() end";
	PointTranslationMap ptm;
	if(!TranslationFactory::IsAvailable(EventTranslationMethod::Lua))
		CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));

	//no code or file at all
	s.DefaultLua = "";
	CHECK_THROWS(TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward));
}

TEST_CASE(SUITE("Engine - Lua is loaded when needed"))
{
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = "function serialise(event) return 'lua:'..event.Index end";
	if(!TranslationFactory::IsAvailable(EventTranslationMethod::Lua))
	{
		WARN("The LuaWrappers library isn't available, so the lazy loading of Lua translation wasn't tested");
		CHECK_THROWS_WITH(TranslationEngine::Create(s, PointTranslationMap(), TranslationEngine::Direction::Forward), Catch::Contains("LuaWrappers"));
		return;
	}
	//nothing in this executable links to LuaWrappers or registers it, so the factory must have loaded it
	auto engine = TranslationEngine::Create(s, PointTranslationMap(), TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(3, 1), "").Value) == "lua:3");
}

namespace
{
//a trivial 'scripted' method, to test the registration hook
class UpperSerialiser: public ISerialiser
{
public:
	std::vector<uint8_t> Serialise(const std::shared_ptr<const EventInfo>& event, const std::string&, const ExtraPointFields&) const override
	{
		auto str = "IDX" + std::to_string(event->GetIndex());
		return std::vector<uint8_t>(str.begin(), str.end());
	}
};
}

TEST_CASE(SUITE("Factory - registered method"))
{
	TranslationFactory::Register(EventTranslationMethod::Lua,
		[](const TranslationFactory::Spec&, const TranslationConfig&, TranslationRole){ return std::make_shared<UpperSerialiser>(); },
		[](const TranslationFactory::Spec&, const TranslationConfig&)->std::shared_ptr<IDeserialiser> { return nullptr; });

	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = "anything";
	PointTranslationMap ptm;
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(3, 1), "").Value) == "IDX3");
}
