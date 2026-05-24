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
 * C_Transform.cpp
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#include "CAPI/C_Transform.h"
#include "C_Internal.h"

namespace odc
{

C_Transform::C_Transform(const std::string& Name, const Json::Value& params, void* lib_handle):
	Transform(Name, params),
	lib_handle(lib_handle),
	c_inst(nullptr),
	p_create(nullptr), p_destroy(nullptr),
	p_enable(nullptr), p_disable(nullptr), p_event(nullptr)
{
	p_create  = reinterpret_cast<void*(*)(const char*,const char*)>(LoadSymbol(lib_handle, "odc_transform_create"));
	p_destroy = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_transform_destroy"));
	p_enable  = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_transform_enable"));
	p_disable = reinterpret_cast<void(*)(void*)>(LoadSymbol(lib_handle, "odc_transform_disable"));
	p_event   = reinterpret_cast<void(*)(void*,C_EventInfo*,C_PassContext*,void(*)(C_PassContext*,C_EventInfo*))>(
		LoadSymbol(lib_handle, "odc_transform_event"));

	if(!p_create || !p_destroy || !p_event)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->error("C_Transform '{}': missing required C API symbols", Name);
		throw std::runtime_error("C_Transform missing required C API symbols");
	}

	std::string params_str;
	if(!params.isNull())
		params_str = Json::FastWriter().write(params);

	c_inst = p_create(Name.c_str(), params_str.c_str());
}

C_Transform::~C_Transform()
{
	if(c_inst && p_destroy)
		p_destroy(c_inst);
	UnLoadModule(lib_handle);
}

void C_Transform::Enable()
{
	if(p_enable)
		p_enable(c_inst);
}

void C_Transform::Disable()
{
	if(p_disable)
		p_disable(c_inst);
}

void C_Transform::Event(std::shared_ptr<EventInfo> event, EvtHandler_ptr pAllow)
{
	if(!p_event || !event)
	{
		if(pAllow)
			(*pAllow)(event);
		return;
	}

	// Serialize EventInfo → C_EventInfo
	C_EventInfo cevt;
	cevt.event_type  = static_cast<uint8_t>(event->GetEventType());
	cevt.index       = event->GetIndex();
	cevt.timestamp   = event->GetTimestamp();
	cevt.quality     = static_cast<uint16_t>(event->GetQuality());
	cevt.source_port = event->GetSourcePort().c_str();
	memset(&cevt.payload, 0, sizeof(cevt.payload));
	if(event->HasPayload())
		SerializePayload(*event, &cevt.payload);

	// Build pass context so C can forward the event
	auto pass_ctx = std::make_shared<C_PassContext>(std::move(pAllow));

	// The pass callback that C code calls to forward the (possibly modified) event
	auto pass_fn = [](C_PassContext* ctx, C_EventInfo* evt)
	{
		if(!ctx || !ctx->pAllow || !evt)
			return;

		// Deserialize back to EventInfo
		auto event_type = static_cast<EventType>(evt->event_type);
		auto result = std::make_shared<EventInfo>(event_type, evt->index,
			"", static_cast<QualityFlags>(evt->quality), evt->timestamp);
		if(evt->source_port)
			result->SetSource(evt->source_port);

		switch(event_type)
		{
			case EventType::Binary:
			case EventType::BinaryOutputStatus:
				result->template SetPayload<EventType::Binary>(evt->payload.binary_val != 0);
				break;
			case EventType::DoubleBitBinary:
			{
				auto dbb = DBB(evt->payload.dbb_val.a != 0, evt->payload.dbb_val.b != 0);
				result->template SetPayload<EventType::DoubleBitBinary>(std::move(dbb));
				break;
			}
			case EventType::Analog:
			case EventType::AnalogOutputStatus:
				result->template SetPayload<EventType::Analog>(std::move(evt->payload.analog_val));
				break;
			case EventType::Counter:
			case EventType::FrozenCounter:
				result->template SetPayload<EventType::Counter>(std::move(evt->payload.counter_val));
				break;
			case EventType::BinaryCommandEvent:
			case EventType::AnalogCommandEvent:
				result->template SetPayload<EventType::BinaryCommandEvent>(
					static_cast<CommandStatus>(evt->payload.cmd_status));
				break;
			case EventType::OctetString:
			{
				auto os_data = evt->payload.octet_string;
				if(os_data.data && os_data.size > 0)
				{
					auto buf = std::string(reinterpret_cast<const char*>(os_data.data), os_data.size);
					result->template SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(buf)));
				}
				else
					result->template SetPayload<EventType::OctetString>(OctetStringBuffer());
				break;
			}
			case EventType::TimeAndInterval:
			{
				auto tai = TAI(evt->payload.tai.time, evt->payload.tai.interval, evt->payload.tai.sequence);
				result->template SetPayload<EventType::TimeAndInterval>(std::move(tai));
				break;
			}
			case EventType::SecurityStat:
			{
				auto ss = SS(evt->payload.security_stat.assoc_id, evt->payload.security_stat.stat);
				result->template SetPayload<EventType::SecurityStat>(std::move(ss));
				break;
			}
			case EventType::ControlRelayOutputBlock:
			{
				ControlRelayOutputBlock crob;
				crob.functionCode = static_cast<ControlCode>(evt->payload.crob.function_code);
				crob.count = evt->payload.crob.count;
				crob.onTimeMS = evt->payload.crob.on_time_ms;
				crob.offTimeMS = evt->payload.crob.off_time_ms;
				crob.status = static_cast<CommandStatus>(evt->payload.crob.status);
				result->template SetPayload<EventType::ControlRelayOutputBlock>(std::move(crob));
				break;
			}
			case EventType::AnalogOutputInt16:
			{
				auto ao16 = AO16(evt->payload.ao16.value,
					static_cast<CommandStatus>(evt->payload.ao16.status));
				result->template SetPayload<EventType::AnalogOutputInt16>(std::move(ao16));
				break;
			}
			case EventType::AnalogOutputInt32:
			{
				auto ao32 = AO32(evt->payload.ao32.value,
					static_cast<CommandStatus>(evt->payload.ao32.status));
				result->template SetPayload<EventType::AnalogOutputInt32>(std::move(ao32));
				break;
			}
			case EventType::AnalogOutputFloat32:
			{
				auto aof = AOF(evt->payload.aof32.value,
					static_cast<CommandStatus>(evt->payload.aof32.status));
				result->template SetPayload<EventType::AnalogOutputFloat32>(std::move(aof));
				break;
			}
			case EventType::AnalogOutputDouble64:
			{
				auto aod = AOD(evt->payload.aod64.value,
					static_cast<CommandStatus>(evt->payload.aod64.status));
				result->template SetPayload<EventType::AnalogOutputDouble64>(std::move(aod));
				break;
			}
			case EventType::BinaryQuality:
			case EventType::DoubleBitBinaryQuality:
			case EventType::AnalogQuality:
			case EventType::CounterQuality:
			case EventType::BinaryOutputStatusQuality:
			case EventType::FrozenCounterQuality:
			case EventType::AnalogOutputStatusQuality:
			case EventType::OctetStringQuality:
				result->template SetPayload<EventType::BinaryQuality>(
					static_cast<QualityFlags>(evt->payload.quality_val));
				break;
			case EventType::ConnectState:
				result->template SetPayload<EventType::ConnectState>(
					static_cast<ConnectState>(evt->payload.connect_state));
				break;
			case EventType::TimeSync:
			{
				auto ts = AbsTime_n_SysOffs(evt->payload.time_sync.abs_time_ms,
					evt->payload.time_sync.sys_offset_ms);
				result->template SetPayload<EventType::TimeSync>(std::move(ts));
				break;
			}
			default:
				break;
		}

		(*ctx->pAllow)(result);
	};

	p_event(c_inst, &cevt, pass_ctx.get(), pass_fn);
}

} // namespace odc
