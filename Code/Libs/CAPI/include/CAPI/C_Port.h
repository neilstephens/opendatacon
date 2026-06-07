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
 * C_Port.h
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#ifndef C_PORT_H
#define C_PORT_H

#include <future>
#include <opendatacon/DataPort.h>
#include <opendatacon/odc_c_api.h>
#include <opendatacon/Platform.h>
#include <opendatacon/MergeJsonConf.h>
#include <json/json.h>
#include <string>
#include <unordered_map>

namespace odc
{

class C_Port;

extern std::unordered_map<void*, C_Port*> C_Port_instances;

class C_Port: public DataPort
{
public:
	C_Port(const std::string& aType, const std::string& aName,
		const std::string& aConfFilename, const Json::Value& aConfOverrides,
		module_ptr lib_handle);
	~C_Port() override;

	void Enable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Enable_();}); }

	void Disable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Disable_();}); }

	void Build() override;

	void ProcessElements(const Json::Value& JSONRoot) override;

	void Log(uint8_t level, const std::string& msg);
	bool ShouldLog(uint8_t level) const;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName,
		SharedStatusCallback_t pStatusCallback) override
	{ pSyncStrand->post([=,this,h{handler_tracker}](){Event_(event,SenderName,pStatusCallback);}); }

	// Public wrappers around protected PublishEvent — callable from C helper functions
	void PublicPublishEvent(const std::shared_ptr<const EventInfo>& event,
		const SharedStatusCallback_t& pStatusCallback) const
	{ PublishEvent(event, pStatusCallback); }
	void PublicPublishEvent(const std::shared_ptr<const EventInfo>& event) const
	{ PublishEvent(event); }

	bool PublicInDemand() const { return InDemand(); }

	// Accessors for C helper functions
	std::shared_ptr<odc::strand_t> GetStrand() const { return pSyncStrand; }
	std::shared_ptr<void> GetHandlerTracker() const { return handler_tracker; }
	void* GetCInst() const { return c_inst; }
	const std::string& GetConfigStr() const { return configJSONstr; }
	const std::string& GetType() const { return Type; }

	const Json::Value GetStatistics() const override
	{
		auto p = std::make_shared<std::promise<Json::Value>>();
		auto f = p->get_future();
		pSyncStrand->post([this,p,h{handler_tracker}](){p->set_value(GetStatistics_());});
		return f.get();
	}

	const Json::Value GetCurrentState() const override
	{
		auto p = std::make_shared<std::promise<Json::Value>>();
		auto f = p->get_future();
		pSyncStrand->post([this,p,h{handler_tracker}](){p->set_value(GetCurrentState_());});
		return f.get();
	}

	const Json::Value GetStatus() const override
	{
		auto p = std::make_shared<std::promise<Json::Value>>();
		auto f = p->get_future();
		pSyncStrand->post([this,p,h{handler_tracker}](){p->set_value(GetStatus_());});
		return f.get();
	}

private:
	mutable std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	mutable std::shared_ptr<odc::strand_t> pSyncStrand = pIOS->make_strand();

	void Enable_();
	void Disable_();
	void Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName,
		SharedStatusCallback_t pStatusCallback);

	const Json::Value GetStatistics_() const;
	const Json::Value GetCurrentState_() const;
	const Json::Value GetStatus_() const;

	std::string Type;
	Json::Value configJSON;
	std::string configJSONstr;
	module_ptr lib_handle;
	void* c_inst;

	// required function pointers — types derived from odc_c_api.h
	decltype(&odc_port_create) p_create;
	decltype(&odc_port_destroy) p_destroy;
	decltype(&odc_port_build) p_build;
	decltype(&odc_port_enable) p_enable;
	decltype(&odc_port_disable) p_disable;
	decltype(&odc_port_event) p_event;

	// optional function pointers
	decltype(&odc_port_stats_json) p_stats_json;
	decltype(&odc_port_state_json) p_state_json;
	decltype(&odc_port_status_json) p_status_json;
	decltype(&odc_port_free_string) p_free_str;
};

} // namespace odc

/* Host API vtable — populated once by C_Helpers.cpp and passed to each loaded
   C port/transform/UI library via odc_library_init().  Exposed here so that
   host-side C++ code (tests, etc.) can call host services through the same
   struct without needing separate extern "C" forward declarations. */
extern C_ODC_HostAPI g_host_api;

#endif // C_PORT_H
