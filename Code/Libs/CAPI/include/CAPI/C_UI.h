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
 * C_UI.h
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#ifndef C_UI_H
#define C_UI_H

#include <future>
#include <opendatacon/IUI.h>
#include <opendatacon/odc_c_api.h>
#include <opendatacon/Platform.h>
#include <json/json.h>
#include <string>

namespace odc
{

class C_UI;
extern std::unordered_map<void*, C_UI*> C_UI_instances;

class C_UI: public IUI
{
public:
	C_UI(const std::string& aType, const std::string& aName,
		const std::string& aConfFilename,
		const Json::Value& aConfOverrides, module_ptr lib_handle);
	~C_UI() override;

	void Build() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Build_();}); }

	void Enable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Enable_();}); }

	void Disable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Disable_();}); }

	void AddCommand(const std::string& name, CmdFunc_t callback, const std::string& desc) override {}

	void Log(uint8_t level, const std::string& msg);
	bool ShouldLog(uint8_t level) const;
	void* GetCInst() const { return c_inst; }

private:
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	std::shared_ptr<odc::strand_t> pSyncStrand = pIOS->make_strand();

	void Build_();
	void Enable_();
	void Disable_();

	std::string Type;
	module_ptr lib_handle;
	void* c_inst;

	decltype(&odc_plugin_create) p_create;
	decltype(&odc_plugin_destroy) p_destroy;
	decltype(&odc_plugin_build) p_build;
	decltype(&odc_plugin_enable) p_enable;
	decltype(&odc_plugin_disable) p_disable;
};

} // namespace odc

#endif // C_UI_H
