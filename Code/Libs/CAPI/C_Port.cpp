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

#include <cstring>
#include <memory>
#include <sstream>

namespace
{

Json::Value ParseJsonString(const char* json_str)
{
	thread_local std::unique_ptr<Json::CharReader> reader =
		[]
		{
			Json::CharReaderBuilder b;
			b["allowComments"] = true;
			return std::unique_ptr<Json::CharReader>(b.newCharReader());
		}();
	Json::Value val;
	std::string errs;
	reader->parse(json_str, json_str + std::strlen(json_str), &val, &errs);
	return val;
}

} // anonymous namespace

namespace odc
{

C_Port::C_Port(const std::string& aType, const std::string& aName,
	const std::string& aConfFilename, const Json::Value& aConfOverrides,
	module_ptr lib_handle):
	DataPort(aName, aConfFilename, aConfOverrides),
	Type(aType),
	c_inst(nullptr),
	p_create(nullptr), p_destroy(nullptr),
	p_build(nullptr), p_enable(nullptr), p_disable(nullptr),
	p_event(nullptr),
	p_stats_json(nullptr), p_state_json(nullptr),
	p_status_json(nullptr), p_free_str(nullptr)
{
	// Check the API version matches
	auto c_api_version = reinterpret_cast<decltype(&odc_c_api_version)>(LoadSymbol(lib_handle, "odc_c_api_version"));
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

	// Resolve required symbols — types derived from odc_c_api.h via decltype
	p_create  = reinterpret_cast<decltype(p_create)>(LoadSymbol(lib_handle, "odc_port_create"));
	p_destroy = reinterpret_cast<decltype(p_destroy)>(LoadSymbol(lib_handle, "odc_port_destroy"));
	p_build   = reinterpret_cast<decltype(p_build)>(LoadSymbol(lib_handle, "odc_port_build"));
	p_enable  = reinterpret_cast<decltype(p_enable)>(LoadSymbol(lib_handle, "odc_port_enable"));
	p_disable = reinterpret_cast<decltype(p_disable)>(LoadSymbol(lib_handle, "odc_port_disable"));
	p_event   = reinterpret_cast<decltype(p_event)>(LoadSymbol(lib_handle, "odc_port_event"));

	// Resolve optional symbols
	p_stats_json  = reinterpret_cast<decltype(p_stats_json)>(LoadSymbol(lib_handle, "odc_port_stats_json"));
	p_state_json  = reinterpret_cast<decltype(p_state_json)>(LoadSymbol(lib_handle, "odc_port_state_json"));
	p_status_json = reinterpret_cast<decltype(p_status_json)>(LoadSymbol(lib_handle, "odc_port_status_json"));
	p_free_str    = reinterpret_cast<decltype(p_free_str)>(LoadSymbol(lib_handle, "odc_port_free_string"));

	if(!p_create || !p_destroy || !p_build || !p_enable || !p_disable || !p_event)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_Port '{}': missing required C API symbols", aName);
		throw std::runtime_error("C_Port missing required C API symbols");
	}

	// Pass the host API vtable to the library — must happen before any
	// odc_port_create() call so the library can use host services immediately.
	{
		auto p_lib_init = reinterpret_cast<void (*)(C_ODC_HostAPI*)>(
			LoadSymbol(lib_handle, "odc_library_init"));
		if(p_lib_init)
			p_lib_init(&g_host_api);
	}

	c_inst = p_create(Type.c_str(), aName.c_str());
	C_Port_instances[c_inst] = this;

	ProcessFile();
}

C_Port::~C_Port()
{
	if(c_inst)
	{
		std::weak_ptr<void> tracker = handler_tracker;
		handler_tracker.reset();
		while(!tracker.expired() && !pIOS->stopped())
			if(!pIOS->poll_one()) std::this_thread::yield();

		if(p_destroy)
			p_destroy(c_inst);

		C_Port_instances.erase(c_inst);
	}
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

	thread_local std::unique_ptr<Json::StreamWriter> writer =
		[]
		{
			Json::StreamWriterBuilder b;
			b["commentStyle"] = "None";
			b["indentation"] = "";
			return std::unique_ptr<Json::StreamWriter>(b.newStreamWriter());
		}();
	std::ostringstream ss;
	writer->write(configJSON, &ss);
	configJSONstr = ss.str();
}

void C_Port::Log(uint8_t level, const std::string& msg)
{
	if(auto log = odc::spdlog_get(Type+"Port"))
		log->log(static_cast<spdlog::level::level_enum>(level), "{}: {}", Name, msg);
}

bool C_Port::ShouldLog(uint8_t level) const
{
	if(auto log = odc::spdlog_get(Type+"Port"))
		return log->should_log(static_cast<spdlog::level::level_enum>(level));
	return false;
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
	auto val = ParseJsonString(json_str);
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
	auto val = ParseJsonString(json_str);
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
	auto val = ParseJsonString(json_str);
	if(p_free_str)
		p_free_str(json_str);
	return val;
}

} // namespace odc
