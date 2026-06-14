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
 * C_Transform.h
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#ifndef C_TRANSFORM_H
#define C_TRANSFORM_H

#include <future>
#include <opendatacon/Transform.h>
#include <opendatacon/asio.h>
#include <opendatacon/odc_c_api.h>
#include <opendatacon/Platform.h>
#include <json/json.h>
#include <string>

namespace odc
{

class C_Transform;
extern std::unordered_map<void*, C_Transform*> C_Transform_instances;

class C_Transform: public Transform
{
public:
	C_Transform(const std::string& aType, const std::string& aName,
		const Json::Value& params, module_ptr lib_handle);
	~C_Transform() override;

	void Enable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Enable_();}); }

	void Disable() override
	{ pSyncStrand->post([this,h{handler_tracker}](){Disable_();}); }

	void Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow) override
	{
		if(!event)
		{
			if(pAllow)
				(*pAllow)(event);
			return;
		}
		pSyncStrand->post([=,this,h{handler_tracker}](){Event_(event,pAllow);});
	}

	void Log(uint8_t level, const std::string& msg);
	bool ShouldLog(uint8_t level) const;
	void* GetCInst() const { return c_inst; }

private:
	std::shared_ptr<void> handler_tracker = std::make_shared<char>();
	std::shared_ptr<asio_service> pIOS = asio_service::Get();
	std::shared_ptr<odc::strand_t> pSyncStrand = pIOS->make_strand();

	void Enable_();
	void Disable_();
	void Event_(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow);

	std::string Type;
	void* c_inst;

	decltype(&odc_transform_create) p_create;
	decltype(&odc_transform_destroy) p_destroy;
	decltype(&odc_transform_enable) p_enable;
	decltype(&odc_transform_disable) p_disable;
	decltype(&odc_transform_event) p_event;
};

} // namespace odc

#endif // C_TRANSFORM_H
