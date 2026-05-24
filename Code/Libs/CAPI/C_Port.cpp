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
 * C_Port.cpp
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#include "CAPI/C_Port.h"
#include "C_Internal.h"

namespace odc
{

C_Port::C_Port(const std::string& aType, const std::string& aName,
	const std::string& aConfFilename, const Json::Value& aConfOverrides,
	void* lib_handle):
	DataPort(aName, aConfFilename, aConfOverrides),
	Type(aType),
	lib_handle(lib_handle),
	c_inst(nullptr),
	p_create(nullptr), p_destroy(nullptr),
	p_build(nullptr), p_enable(nullptr), p_disable(nullptr),
	p_event(nullptr),
	p_stats_json(nullptr), p_state_json(nullptr),
	p_status_json(nullptr), p_free_str(nullptr)
{
	// Check the API version matches
	auto c_api_version = reinterpret_cast<const char*(*)()>(LoadSymbol(lib_handle, "odc_c_api_version"));
	if(!c_api_version)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_Port '{}': missing C API version symbol", aName);
		throw std::runtime_error("C_Port missing C API version symbol");
	}
	if(std::string(c_api_version()) != ODC_C_API_VERSION)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_Port '{}': C API version mismatch (expected {}, got {})", aName, ODC_C_API_VERSION, c_api_version());
		throw std::runtime_error("C_Port C API version mismatch");
	}

	// Resolve required symbols
	p_create  = reinterpret_cast<void*(*)(const char*,const char*)>(LoadSymbol(lib_handle, "odc_port_create"));
	p_destroy = reinterpret_cast<void (*)(void*)>(LoadSymbol(lib_handle, "odc_port_destroy"));
	p_build   = reinterpret_cast<void (*)(void*)>(LoadSymbol(lib_handle, "odc_port_build"));
	p_enable  = reinterpret_cast<void (*)(void*)>(LoadSymbol(lib_handle, "odc_port_enable"));
	p_disable = reinterpret_cast<void (*)(void*)>(LoadSymbol(lib_handle, "odc_port_disable"));
	p_event   = reinterpret_cast<void (*)(void*,const C_EventInfo*,const char*,C_StatusCallback*)>(LoadSymbol(lib_handle, "odc_port_event"));

	// Resolve optional symbols
	p_stats_json  = reinterpret_cast<const char*(*)(void*)>(LoadSymbol(lib_handle, "odc_port_stats_json"));
	p_state_json  = reinterpret_cast<const char*(*)(void*)>(LoadSymbol(lib_handle, "odc_port_state_json"));
	p_status_json = reinterpret_cast<const char*(*)(void*)>(LoadSymbol(lib_handle, "odc_port_status_json"));
	p_free_str    = reinterpret_cast<void (*)(const char*)>(LoadSymbol(lib_handle, "odc_port_free_string"));

	if(!p_create || !p_destroy || !p_build || !p_enable || !p_disable || !p_event)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_Port '{}': missing required C API symbols", aName);
		throw std::runtime_error("C_Port missing required C API symbols");
	}

	c_inst = p_create(Type.c_str(), aName.c_str());
	C_Port_instances[c_inst] = this;
}

C_Port::~C_Port()
{
	C_Port_instances.erase(c_inst);
	if(c_inst)
	{
		std::weak_ptr<void> tracker = handler_tracker;
		handler_tracker.reset();
		while(!tracker.expired() && !pIOS->stopped())
			pIOS->poll_one();

		if(p_destroy)
			p_destroy(c_inst);
	}
	UnLoadModule(lib_handle);
}

void C_Port::Enable_()
{
	if(p_enable)
		p_enable(c_inst);
}

void C_Port::Disable_()
{
	if(p_disable)
		p_disable(c_inst);
}

void C_Port::Build()
{
	if(p_build)
		p_build(c_inst);
}

void C_Port::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject())
		return;
	MergeJsonConf(configJSON, JSONRoot);
	configJSONstr = Json::FastWriter().write(configJSON);
}

void C_Port::Log(uint8_t level, const std::string& msg)
{
	if(auto log = odc::spdlog_get(Type+"Port"))
		log->log(static_cast<spdlog::level::level_enum>(level), "{}", msg);
	else if(auto log = odc::spdlog_get("opendatacon"))
		log->log(static_cast<spdlog::level::level_enum>(level), "{}", msg);
}

void C_Port::Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName,
	SharedStatusCallback_t pStatusCallback)
{
	if(!p_event)
	{
		if(pStatusCallback)
			(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	C_EventInfo cevt;
	cevt.event_type  = static_cast<uint8_t>(event->GetEventType());
	cevt.index       = event->GetIndex();
	cevt.timestamp   = event->GetTimestamp();
	cevt.quality     = static_cast<uint16_t>(event->GetQuality());
	cevt.source_port = event->GetSourcePort().c_str();

	// Zero union before serializing (safety)
	memset(&cevt.payload, 0, sizeof(cevt.payload));
	if(event->HasPayload())
		SerializePayload(*event, &cevt.payload);

	// Wrap the status callback
	auto* cb_wrapper = pStatusCallback
	      ? new C_StatusCallback(std::make_shared<SharedStatusCallback_t::element_type>(
		[pStatusCallback](CommandStatus s) { (*pStatusCallback)(s); }))
	      : nullptr;

	p_event(c_inst, &cevt, SenderName.c_str(), cb_wrapper);
}

const Json::Value C_Port::GetStatistics_() const
{
	if(!p_stats_json)
		return Json::Value();
	const char* json_str = p_stats_json(c_inst);
	if(!json_str)
		return Json::Value();
	Json::Value val;
	Json::Reader().parse(json_str, val);
	if(p_free_str)
		p_free_str(json_str);
	return val;
}

const Json::Value C_Port::GetCurrentState_() const
{
	if(!p_state_json)
		return Json::Value();
	const char* json_str = p_state_json(c_inst);
	if(!json_str)
		return Json::Value();
	Json::Value val;
	Json::Reader().parse(json_str, val);
	if(p_free_str)
		p_free_str(json_str);
	return val;
}

const Json::Value C_Port::GetStatus_() const
{
	if(!p_status_json)
		return Json::Value();
	const char* json_str = p_status_json(c_inst);
	if(!json_str)
		return Json::Value();
	Json::Value val;
	Json::Reader().parse(json_str, val);
	if(p_free_str)
		p_free_str(json_str);
	return val;
}

} // namespace odc
