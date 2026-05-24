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

#include <opendatacon/DataPort.h>
#include <opendatacon/odc_c_api.h>
#include <opendatacon/Platform.h>
#include <json/json.h>
#include <string>
#include <unordered_map>

namespace odc
{

class C_Port;

extern std::unordered_map<void*, C_Port*> C_Port_instances;

class C_Port : public DataPort
{
public:
	C_Port(const std::string& aName, const std::string& aConfFilename,
	       const Json::Value& aConfOverrides, void* lib_handle);
	~C_Port() override;

	void Enable() override;
	void Disable() override;
	void Build() override;
	void ProcessElements(const Json::Value& JSONRoot) override;
	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName,
	           SharedStatusCallback_t pStatusCallback) override;

	// Public wrappers around protected PublishEvent — callable from C helper functions
	void PublicPublishEvent(const std::shared_ptr<const EventInfo>& event,
	                        const SharedStatusCallback_t& pStatusCallback) const
	{ PublishEvent(event, pStatusCallback); }
	void PublicPublishEvent(const std::shared_ptr<const EventInfo>& event) const
	{ PublishEvent(event); }

	const Json::Value GetStatistics() const override;
	const Json::Value GetCurrentState() const override;
	const Json::Value GetStatus() const override;

private:
	void* lib_handle;
	void* c_inst;

	// required function pointers
	const char* (*p_port_type)();
	void* (*p_create)(const char*, const char*, const char*);
	void  (*p_destroy)(void*);
	void  (*p_build)(void*);
	void  (*p_enable)(void*);
	void  (*p_disable)(void*);
	void  (*p_event)(void*, const C_EventInfo*, const char*, C_StatusCallback*);

	// optional function pointers
	const char* (*p_stats_json)(void*);
	const char* (*p_state_json)(void*);
	const char* (*p_status_json)(void*);
	void        (*p_free_str)(const char*);
};

} // namespace odc

#endif // C_PORT_H
