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
 * C_UI.cpp
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#include "CAPI/C_UI.h"
#include "C_Internal.h"

namespace odc
{

std::unordered_map<void*, C_UI*> C_UI_instances;

C_UI::C_UI(const std::string& aType, const std::string& aName,
	const std::string& aConfFilename,
	const Json::Value& aConfOverrides, void* lib_handle):
	Type(aType),
	lib_handle(lib_handle),
	c_inst(nullptr),
	p_create(nullptr), p_destroy(nullptr),
	p_build(nullptr), p_enable(nullptr), p_disable(nullptr)
{
	// Check the API version matches
	auto c_api_version = reinterpret_cast<decltype(&odc_c_api_version)>(LoadSymbol(lib_handle, "odc_c_api_version"));
	if(!c_api_version)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_UI '{}': missing C API version symbol", aName);
		throw std::runtime_error("C_UI missing C API version symbol");
	}
	if(std::string(c_api_version()) != ODC_C_API_VERSION)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_UI '{}': C API version mismatch (expected {}, got {})", aName, ODC_C_API_VERSION, c_api_version());
		throw std::runtime_error("C_UI C API version mismatch");
	}

	p_create  = reinterpret_cast<decltype(p_create)>(LoadSymbol(lib_handle, "odc_plugin_create"));
	p_destroy = reinterpret_cast<decltype(p_destroy)>(LoadSymbol(lib_handle, "odc_plugin_destroy"));
	p_build   = reinterpret_cast<decltype(p_build)>(LoadSymbol(lib_handle, "odc_plugin_build"));
	p_enable  = reinterpret_cast<decltype(p_enable)>(LoadSymbol(lib_handle, "odc_plugin_enable"));
	p_disable = reinterpret_cast<decltype(p_disable)>(LoadSymbol(lib_handle, "odc_plugin_disable"));

	if(!p_create || !p_destroy || !p_build || !p_enable || !p_disable)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_UI '{}': missing required C API symbols", aName);
		throw std::runtime_error("C_UI missing required C API symbols");
	}

	std::string overrides_str;
	if(!aConfOverrides.isNull())
		overrides_str = Json::FastWriter().write(aConfOverrides);

	c_inst = p_create(aName.c_str(), aConfFilename.c_str(), overrides_str.c_str());
	C_UI_instances[c_inst] = this;
}

C_UI::~C_UI()
{
	C_UI_instances.erase(c_inst);
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

void C_UI::Build_()
{
	if(p_build)
		p_build(c_inst);
}

void C_UI::Enable_()
{
	if(p_enable)
		p_enable(c_inst);
}

void C_UI::Disable_()
{
	if(p_disable)
		p_disable(c_inst);
}

void C_UI::Log(uint8_t level, const std::string& msg)
{
	if(auto log = odc::spdlog_get(Type))
		log->log(static_cast<spdlog::level::level_enum>(level), "{}", msg);
}

bool C_UI::ShouldLog(uint8_t level) const
{
	if(auto log = odc::spdlog_get(Type))
		return log->should_log(static_cast<spdlog::level::level_enum>(level));
	return false;
}

} // namespace odc
