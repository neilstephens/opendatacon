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

C_UI::C_UI(const std::string& aName, const std::string& aConfFilename,
           const Json::Value& aConfOverrides, void* lib_handle):
	lib_handle(lib_handle),
	c_inst(nullptr),
	p_create(nullptr), p_destroy(nullptr),
	p_build(nullptr), p_enable(nullptr), p_disable(nullptr)
{
	p_create  = reinterpret_cast<void*(*)(const char*,const char*,const char*)>(LoadSymbol(lib_handle, "odc_plugin_create"));
	p_destroy = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_plugin_destroy"));
	p_build   = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_plugin_build"));
	p_enable  = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_plugin_enable"));
	p_disable = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_plugin_disable"));

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
}

C_UI::~C_UI()
{
	if(c_inst && p_destroy)
		p_destroy(c_inst);
	UnLoadModule(lib_handle);
}

void C_UI::Build()
{
	if(p_build)
		p_build(c_inst);
}

void C_UI::Enable()
{
	if(p_enable)
		p_enable(c_inst);
}

void C_UI::Disable()
{
	if(p_disable)
		p_disable(c_inst);
}

} // namespace odc
