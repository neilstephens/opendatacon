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
 * Translation.cpp
 */

#include <opendatacon/Translation.h>
#include <opendatacon/TemplateTranslation.h>
#include <opendatacon/CBORTranslation.h>
#include <opendatacon/Platform.h>
#include <map>
#include <mutex>
#include <stdexcept>

namespace odc
{

std::string DefaultJSONwPlaceholders()
{
	return R"(
{
	"EventType": "<EVENTTYPE>",
	"NumericEventType": <EVENTTYPE_RAW>,
	"Index": <INDEX>,
	"Description": "<POINT:Name>",
	"Timestamp": <TIMESTAMP>,
	"DateTime": "<DATETIME>",
	"Quality": "<QUALITY>",
	"NumericQuality": <QUALITY_RAW>,
	"Payload": <PAYLOAD>,
	"SourcePort": "<SOURCEPORT>",
	"SenderName": "<SENDERNAME>"
}
)";
}

std::string DefaultCBORStructure()
{
	return R"( ["TIMESTAMP","EVENTTYPE_RAW","SOURCEPORT","INDEX","QUALITY_RAW","PAYLOAD"] )";
}

namespace
{
struct Registry
{
	std::mutex mtx;
	std::map<EventTranslationMethod,TranslationFactory::SerialiserCreator> serialisers;
	std::map<EventTranslationMethod,TranslationFactory::DeserialiserCreator> deserialisers;
	//the handle of the lazily loaded Lua implementation
	module_ptr lua_module = nullptr;
};
Registry& GetRegistry()
{
	//Deliberately leaked. The creators are code that lives in (possibly dynamically loaded) modules,
	//	which may be unloaded before static destruction.
	static Registry& registry = *new Registry;
	return registry;
}

//Load the LuaWrappers library, and let it register the Lua method. Returns whether Lua is then registered.
//	The module is never unloaded, because the registered creators are code in it.
bool TryLoadLua()
{
	auto& reg = GetRegistry();
	{
		std::scoped_lock lck(reg.mtx);
		if(reg.serialisers.count(EventTranslationMethod::Lua))
			return true;
		if(reg.lua_module)
			return false; //already tried, and it didn't work
	}

	//don't hold the lock while loading, because registering needs it
	auto lib_name = GetLibFileName("LuaWrappers");
	auto handle = LoadModule(lib_name);
	if(!handle)
		return false;
	auto register_func = reinterpret_cast<void (*)()>(LoadSymbol(handle, "RegisterLuaTranslation"));
	{
		std::scoped_lock lck(reg.mtx);
		reg.lua_module = handle;
	}
	if(!register_func)
		return false;
	register_func();

	std::scoped_lock lck(reg.mtx);
	return reg.serialisers.count(EventTranslationMethod::Lua) > 0;
}

template <typename T, typename M>
T FindCreator(M& creators, EventTranslationMethod method)
{
	auto& reg = GetRegistry();
	{
		std::scoped_lock lck(reg.mtx);
		auto it = creators.find(method);
		if(it != creators.end())
			return it->second;
	}
	if(method == EventTranslationMethod::Lua && TryLoadLua())
	{
		std::scoped_lock lck(reg.mtx);
		auto it = creators.find(method);
		if(it != creators.end())
			return it->second;
	}
	return nullptr;
}

std::string UnavailableMessage(EventTranslationMethod method)
{
	if(method == EventTranslationMethod::Lua)
		return "Lua translation isn't available: the "+GetLibFileName("LuaWrappers")+" library couldn't be loaded";
	return "Translation method "+std::to_string(static_cast<int>(method))+" is not available (no implementation registered)";
}
}

void TranslationFactory::Register(EventTranslationMethod method, SerialiserCreator serialiser, DeserialiserCreator deserialiser)
{
	auto& reg = GetRegistry();
	std::scoped_lock lck(reg.mtx);
	reg.serialisers[method] = std::move(serialiser);
	reg.deserialisers[method] = std::move(deserialiser);
}

bool TranslationFactory::IsRegistered(EventTranslationMethod method)
{
	if(method == EventTranslationMethod::Template || method == EventTranslationMethod::CBOR)
		return true;
	auto& reg = GetRegistry();
	std::scoped_lock lck(reg.mtx);
	return reg.serialisers.count(method) > 0;
}

bool TranslationFactory::IsAvailable(EventTranslationMethod method)
{
	return IsRegistered(method) || (method == EventTranslationMethod::Lua && TryLoadLua());
}

std::shared_ptr<ISerialiser> TranslationFactory::CreateSerialiser(EventTranslationMethod method, const Spec& spec, const TranslationConfig& config, TranslationRole role)
{
	switch(method)
	{
		case EventTranslationMethod::Template:
			return std::make_shared<TemplateSerialiser>(spec.Content, config);
		case EventTranslationMethod::CBOR:
			return std::make_shared<CBORSerialiser>(spec.Content, config);
		default:
		{
			auto creator = FindCreator<SerialiserCreator>(GetRegistry().serialisers, method);
			if(!creator)
				throw std::runtime_error(UnavailableMessage(method));
			auto pSer = creator(spec, config, role);
			if(!pSer)
				throw std::runtime_error("Failed to create serialiser");
			return pSer;
		}
	}
}

std::shared_ptr<IDeserialiser> TranslationFactory::CreateDeserialiser(EventTranslationMethod method, const Spec& spec, const TranslationConfig& config)
{
	switch(method)
	{
		case EventTranslationMethod::Template:
			return std::make_shared<TemplateDeserialiser>(spec.Content, config);
		case EventTranslationMethod::CBOR:
			return std::make_shared<CBORDeserialiser>(std::make_shared<const CBORSerialiser>(spec.Content, config), config);
		default:
		{
			auto creator = FindCreator<DeserialiserCreator>(GetRegistry().deserialisers, method);
			if(!creator)
				throw std::runtime_error(UnavailableMessage(method));
			auto pDes = creator(spec, config);
			if(!pDes)
				throw std::runtime_error("Failed to create deserialiser");
			return pDes;
		}
	}
}

} //namespace odc
