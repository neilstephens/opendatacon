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
 * LuaTranslation.cpp
 *
 *  Tests for the Lua translation method (registered by LuaWrappers)
 */

#include <Lua/Translation.h>
#include <opendatacon/TranslationEngine.h>
#include <opendatacon/CBORTranslation.h>
#include <catch.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace odc;

#define SUITE(name) "LuaTranslationTestSuite - " name

namespace
{
std::shared_ptr<EventInfo> MakeAnalog(size_t index, double value, const std::string& source = "SrcPort")
{
	auto event = std::make_shared<EventInfo>(EventType::Analog, index, source, QualityFlags::ONLINE, 1700000000123);
	event->SetPayload<EventType::Analog>(std::move(value));
	return event;
}

std::string Str(const std::vector<uint8_t>& v)
{
	return std::string(v.begin(), v.end());
}

const std::string payload_script = R"lua(
function serialise(event, sender, extra)
	return "idx=" .. event.Index .. ";val=" .. event.Payload .. ";by=" .. sender .. ";tag=" .. (extra.Tag or "none")
end
function deserialise(payload, key, hint)
	local idx, val = string.match(payload, "idx=(%d+);val=([%d%.%-]+)")
	if idx == nil then return nil end
	return { EventType = odc.EventType.Analog, Index = math.tointeger(tonumber(idx)), Payload = tonumber(val),
		QualityFlags = odc.QualityFlag.ONLINE, Timestamp = 12345 }
end
)lua";

PointTranslationMap EmptyPTM()
{
	return PointTranslationMap();
}
}

TEST_CASE(SUITE("Serialise and deserialise"))
{
	RegisterLuaTranslation();
	TranslationSettings s;
	s.DefaultChannel = "c";
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = payload_script;

	PointTranslationMap ptm;
	PointTranslationEntry pte;
	pte.ExtraFields["Tag"] = "special";
	ptm[{"",2,EventType::Analog}] = pte;

	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);

	auto msg = engine->Serialise(MakeAnalog(1, 2.5), "snd");
	CHECK(Str(msg.Value) == "idx=1;val=2.5;by=snd;tag=none");
	msg = engine->Serialise(MakeAnalog(2, 2.5), "snd");
	CHECK(Str(msg.Value) == "idx=2;val=2.5;by=snd;tag=special");

	//literal (empty) default key, so the reverse lookup works
	bool matched = false;
	auto event = engine->Deserialise("c", msg.Key.data(), msg.Key.size(), msg.Value.data(), msg.Value.size(), &matched);
	CHECK(matched);
	REQUIRE(event);
	CHECK(event->GetIndex() == 2);
	CHECK(event->GetEventType() == EventType::Analog);
	CHECK(event->GetPayload<EventType::Analog>() == 2.5);
	CHECK(event->GetTimestamp() == 12345);

	//script returns nil for garbage
	std::string garbage = "garbage";
	CHECK(engine->Deserialise("c", msg.Key.data(), msg.Key.size(), reinterpret_cast<const uint8_t*>(garbage.data()), garbage.size()) == nullptr);
}

TEST_CASE(SUITE("Per point scripts"))
{
	RegisterLuaTranslation();
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = payload_script;
	PointTranslationMap ptm;
	PointTranslationEntry pte;
	pte.Lua = R"lua(function serialise(event) return "special" end)lua";
	ptm[{"",2,EventType::Analog}] = pte;
	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(2, 1), "").Value) == "special");
	CHECK(Str(engine->Serialise(MakeAnalog(3, 1), "").Value) == "idx=3;val=1.0;by=;tag=none");
}

TEST_CASE(SUITE("Lua key with hint"))
{
	RegisterLuaTranslation();
	TranslationSettings s;
	s.DefaultChannel = "c";
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.KeyMethod = KeyTranslationMethod::Lua;
	s.DefaultKeyLua = R"lua(
		function key(event, sender, extra) return "pt/" .. event.Index .. "/" .. event.EventType end
	)lua";
	s.DefaultLua = R"lua(
		function serialise(event) return tostring(event.Payload) end
		function deserialise(payload, key, hint)
			-- the key says which point it is
			return { EventType = hint.EventType, Index = hint.Index + 1000, SourcePort = "wrong", Payload = tonumber(payload) }
		end
	)lua";

	PointTranslationMap ptm;
	for(size_t i : {1,2,3})
		ptm[{"",i,EventType::Analog}] = PointTranslationEntry();

	auto engine = TranslationEngine::Create(s, ptm, TranslationEngine::Direction::Both);

	auto msg = engine->Serialise(MakeAnalog(2, 4.5), "");
	CHECK(Str(msg.Key) == "pt/2/"+std::to_string(static_cast<int>(EventType::Analog)));

	auto event = engine->Deserialise("c", msg.Key.data(), msg.Key.size(), msg.Value.data(), msg.Value.size());
	REQUIRE(event);
	CHECK(event->GetIndex() == 2); //the key wins over what the script returned
	CHECK(event->GetPayload<EventType::Analog>() == 4.5);
}

TEST_CASE(SUITE("Wrapper functions in scripts"))
{
	RegisterLuaTranslation();
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLua = R"lua(
		function serialise(event, sender, extra)
			return odc.FillTemplate("<INDEX>/<EVENTTYPE>/<PAYLOAD>/<SENDERNAME>/<POINT:x>", event, sender, {x = "ex"})
		end
	)lua";
	auto engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(4, 1.5), "me").Value) == "4/Analog/1.500000/me/ex");

	s.DefaultLua = R"lua(
		function serialise(event, sender, extra)
			return odc.CBOREncode('["INDEX","EVENTTYPE"]', event, sender, extra)
		end
	)lua";
	engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward);
	auto direct = CBORSerialiser(R"(["INDEX","EVENTTYPE"])").Serialise(MakeAnalog(4, 1.5), "me", {});
	CHECK(engine->Serialise(MakeAnalog(4, 1.5), "me").Value == direct);
}

TEST_CASE(SUITE("Lua file"))
{
	RegisterLuaTranslation();
	auto file = (std::filesystem::temp_directory_path() / "odc_lua_translation_test.lua").string();
	{
		std::ofstream f(file);
		f << payload_script;
	}
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;
	s.DefaultLuaFile = file;
	auto engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward);
	CHECK(Str(engine->Serialise(MakeAnalog(1, 1), "x").Value) == "idx=1;val=1.0;by=x;tag=none");
	std::filesystem::remove(file);

	s.DefaultLuaFile = file; //gone
	CHECK_THROWS(TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward));
}

TEST_CASE(SUITE("Bad scripts"))
{
	RegisterLuaTranslation();
	TranslationSettings s;
	s.TranslationMethod = EventTranslationMethod::Lua;

	//syntax error
	s.DefaultLua = "function serialise( end end";
	CHECK_THROWS(TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward));

	//only has the other direction
	s.DefaultLua = "function deserialise() end";
	CHECK_THROWS(TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward));
	CHECK_NOTHROW(TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Reverse));

	//runtime errors are exceptions on serialise
	s.DefaultLua = "function serialise() error('boom') end";
	auto engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward);
	CHECK_THROWS(engine->Serialise(MakeAnalog(1, 1), ""));

	//and wrong return type
	s.DefaultLua = "function serialise() return {} end";
	engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Forward);
	CHECK_THROWS(engine->Serialise(MakeAnalog(1, 1), ""));

	//deserialise errors are just nullptr
	s.DefaultLua = "function deserialise() error('boom') end";
	engine = TranslationEngine::Create(s, EmptyPTM(), TranslationEngine::Direction::Reverse);
	std::string val = "x";
	CHECK(engine->Deserialise("", nullptr, 0, reinterpret_cast<const uint8_t*>(val.data()), val.size()) == nullptr);
}
