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
 * C_Helpers.cpp
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#include "CAPI/C_Port.h"
#include "C_Internal.h"

namespace odc
{
std::unordered_map<void*, C_Port*> C_Port_instances;
}

void SerializePayload(const odc::EventInfo& src, union C_Payload* dst)
{
	using namespace odc;
	switch(src.GetEventType())
	{
		case EventType::Binary:
		case EventType::BinaryOutputStatus:
			dst->binary_val = src.GetPayload<EventType::Binary>() ? 1 : 0;
			break;
		case EventType::DoubleBitBinary:
		{
			auto dbb = src.GetPayload<EventType::DoubleBitBinary>();
			dst->dbb_val.a = dbb.first ? 1 : 0;
			dst->dbb_val.b = dbb.second ? 1 : 0;
			break;
		}
		case EventType::Analog:
		case EventType::AnalogOutputStatus:
			dst->analog_val = src.GetPayload<EventType::Analog>();
			break;
		case EventType::Counter:
		case EventType::FrozenCounter:
			dst->counter_val = src.GetPayload<EventType::Counter>();
			break;
		case EventType::BinaryCommandEvent:
		case EventType::AnalogCommandEvent:
			dst->cmd_status = static_cast<uint8_t>(src.GetPayload<EventType::BinaryCommandEvent>());
			break;
		case EventType::OctetString:
		{
			auto& osb = src.GetPayload<EventType::OctetString>();
			dst->octet_string.data = static_cast<const uint8_t*>(osb.data());
			dst->octet_string.size = osb.size();
			break;
		}
		case EventType::TimeAndInterval:
		{
			auto tai = src.GetPayload<EventType::TimeAndInterval>();
			dst->tai.time = std::get<0>(tai);
			dst->tai.interval = std::get<1>(tai);
			dst->tai.sequence = std::get<2>(tai);
			break;
		}
		case EventType::SecurityStat:
		{
			auto ss = src.GetPayload<EventType::SecurityStat>();
			dst->security_stat.assoc_id = ss.first;
			dst->security_stat.stat = ss.second;
			break;
		}
		case EventType::ControlRelayOutputBlock:
		{
			auto& crob = src.GetPayload<EventType::ControlRelayOutputBlock>();
			dst->crob.function_code = static_cast<uint8_t>(crob.functionCode);
			dst->crob.count = crob.count;
			dst->crob.on_time_ms = crob.onTimeMS;
			dst->crob.off_time_ms = crob.offTimeMS;
			dst->crob.status = static_cast<uint8_t>(crob.status);
			break;
		}
		case EventType::AnalogOutputInt16:
		{
			auto ao = src.GetPayload<EventType::AnalogOutputInt16>();
			dst->ao16.value = ao.first;
			dst->ao16.status = static_cast<uint8_t>(ao.second);
			break;
		}
		case EventType::AnalogOutputInt32:
		{
			auto ao = src.GetPayload<EventType::AnalogOutputInt32>();
			dst->ao32.value = ao.first;
			dst->ao32.status = static_cast<uint8_t>(ao.second);
			break;
		}
		case EventType::AnalogOutputFloat32:
		{
			auto ao = src.GetPayload<EventType::AnalogOutputFloat32>();
			dst->aof32.value = ao.first;
			dst->aof32.status = static_cast<uint8_t>(ao.second);
			break;
		}
		case EventType::AnalogOutputDouble64:
		{
			auto ao = src.GetPayload<EventType::AnalogOutputDouble64>();
			dst->aod64.value = ao.first;
			dst->aod64.status = static_cast<uint8_t>(ao.second);
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
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::BinaryQuality>());
			break;
		case EventType::ConnectState:
			dst->connect_state = static_cast<uint8_t>(src.GetPayload<EventType::ConnectState>());
			break;
		case EventType::TimeSync:
		{
			auto ts = src.GetPayload<EventType::TimeSync>();
			dst->time_sync.abs_time_ms = ts.first;
			dst->time_sync.sys_offset_ms = ts.second;
			break;
		}
		default:
			dst->stub = 0;
			break;
	}
}

extern "C" void odc_InvokeStatusCallback(C_StatusCallback** cb, uint8_t status)
{
	if(!cb || !*cb)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->warn("odc_InvokeStatusCallback: null callback (already invoked?)");
		return;
	}
	auto* p = *cb;
	(*(p->cb))(static_cast<odc::CommandStatus>(status));
	delete p;
	*cb = nullptr;
}

extern "C" void odc_PublishEvent(void* inst, const struct C_EventInfo* cevt,
	C_StatusCallbackFunc_t callback, void* handle)
{
	if(!inst || !cevt)
		return;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end())
		return;

	// Deserialize C_EventInfo → EventInfo
	auto event_type = static_cast<odc::EventType>(cevt->event_type);
	auto event = std::make_shared<odc::EventInfo>(event_type, cevt->index,
		"", static_cast<odc::QualityFlags>(cevt->quality), cevt->timestamp);

	// Set source port
	if(cevt->source_port)
		event->SetSource(cevt->source_port);

	// Copy payload
	switch(event_type)
	{
		case odc::EventType::Binary:
		case odc::EventType::BinaryOutputStatus:
			event->template SetPayload<odc::EventType::Binary>(cevt->payload.binary_val != 0);
			break;
		case odc::EventType::DoubleBitBinary:
		{
			auto dbb = odc::DBB(cevt->payload.dbb_val.a != 0, cevt->payload.dbb_val.b != 0);
			event->template SetPayload<odc::EventType::DoubleBitBinary>(std::move(dbb));
			break;
		}
		case odc::EventType::Analog:
		case odc::EventType::AnalogOutputStatus:
		{
			auto tmp_a = cevt->payload.analog_val;
			event->template SetPayload<odc::EventType::Analog>(std::move(tmp_a));
			break;
		}
		case odc::EventType::Counter:
		case odc::EventType::FrozenCounter:
		{
			auto tmp_c = cevt->payload.counter_val;
			event->template SetPayload<odc::EventType::Counter>(std::move(tmp_c));
			break;
		}
		case odc::EventType::BinaryCommandEvent:
		case odc::EventType::AnalogCommandEvent:
			event->template SetPayload<odc::EventType::BinaryCommandEvent>(
				static_cast<odc::CommandStatus>(cevt->payload.cmd_status));
			break;
		case odc::EventType::OctetString:
		{
			auto os_data = cevt->payload.octet_string;
			if(os_data.data && os_data.size > 0)
			{
				auto buf = std::string(reinterpret_cast<const char*>(os_data.data), os_data.size);
				event->template SetPayload<odc::EventType::OctetString>(odc::OctetStringBuffer(std::move(buf)));
			}
			else
			{
				event->template SetPayload<odc::EventType::OctetString>(odc::OctetStringBuffer());
			}
			break;
		}
		case odc::EventType::TimeAndInterval:
		{
			auto tai = odc::TAI(cevt->payload.tai.time, cevt->payload.tai.interval, cevt->payload.tai.sequence);
			event->template SetPayload<odc::EventType::TimeAndInterval>(std::move(tai));
			break;
		}
		case odc::EventType::SecurityStat:
		{
			auto ss = odc::SS(cevt->payload.security_stat.assoc_id, cevt->payload.security_stat.stat);
			event->template SetPayload<odc::EventType::SecurityStat>(std::move(ss));
			break;
		}
		case odc::EventType::ControlRelayOutputBlock:
		{
			odc::ControlRelayOutputBlock crob;
			crob.functionCode = static_cast<odc::ControlCode>(cevt->payload.crob.function_code);
			crob.count = cevt->payload.crob.count;
			crob.onTimeMS = cevt->payload.crob.on_time_ms;
			crob.offTimeMS = cevt->payload.crob.off_time_ms;
			crob.status = static_cast<odc::CommandStatus>(cevt->payload.crob.status);
			event->template SetPayload<odc::EventType::ControlRelayOutputBlock>(std::move(crob));
			break;
		}
		case odc::EventType::AnalogOutputInt16:
		{
			auto ao16 = odc::AO16(cevt->payload.ao16.value,
				static_cast<odc::CommandStatus>(cevt->payload.ao16.status));
			event->template SetPayload<odc::EventType::AnalogOutputInt16>(std::move(ao16));
			break;
		}
		case odc::EventType::AnalogOutputInt32:
		{
			auto ao32 = odc::AO32(cevt->payload.ao32.value,
				static_cast<odc::CommandStatus>(cevt->payload.ao32.status));
			event->template SetPayload<odc::EventType::AnalogOutputInt32>(std::move(ao32));
			break;
		}
		case odc::EventType::AnalogOutputFloat32:
		{
			auto aof = odc::AOF(cevt->payload.aof32.value,
				static_cast<odc::CommandStatus>(cevt->payload.aof32.status));
			event->template SetPayload<odc::EventType::AnalogOutputFloat32>(std::move(aof));
			break;
		}
		case odc::EventType::AnalogOutputDouble64:
		{
			auto aod = odc::AOD(cevt->payload.aod64.value,
				static_cast<odc::CommandStatus>(cevt->payload.aod64.status));
			event->template SetPayload<odc::EventType::AnalogOutputDouble64>(std::move(aod));
			break;
		}
		case odc::EventType::BinaryQuality:
		case odc::EventType::DoubleBitBinaryQuality:
		case odc::EventType::AnalogQuality:
		case odc::EventType::CounterQuality:
		case odc::EventType::BinaryOutputStatusQuality:
		case odc::EventType::FrozenCounterQuality:
		case odc::EventType::AnalogOutputStatusQuality:
		case odc::EventType::OctetStringQuality:
			event->template SetPayload<odc::EventType::BinaryQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::ConnectState:
			event->template SetPayload<odc::EventType::ConnectState>(
				static_cast<odc::ConnectState>(cevt->payload.connect_state));
			break;
		case odc::EventType::TimeSync:
		{
			auto ts = odc::AbsTime_n_SysOffs(cevt->payload.time_sync.abs_time_ms,
				cevt->payload.time_sync.sys_offset_ms);
			event->template SetPayload<odc::EventType::TimeSync>(std::move(ts));
			break;
		}
		default:
			// Stub types: no meaningful payload to copy
			break;
	}

	if(callback)
	{
		// Wrap the C function pointer + handle into a SharedStatusCallback_t
		auto sharedCb = std::make_shared<odc::SharedStatusCallback_t::element_type>(
			[callback, handle](odc::CommandStatus s)
			{
				callback(static_cast<uint8_t>(s), handle);
			});
		it->second->PublicPublishEvent(event, sharedCb);
	}
	else
	{
		it->second->PublicPublishEvent(event);
	}
}

extern "C" void* odc_msTimerCallback(void* inst, uint64_t ms,
	C_StatusCallbackFunc_t callback, void* handle)
{
	if(!inst || !callback)
		return nullptr;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end())
		return nullptr;

	auto* port = it->second;
	auto sync = port->GetStrand();
	auto tracker = port->GetHandlerTracker();

	auto timer = odc::asio_service::Get()->make_steady_timer();
	timer->expires_from_now(std::chrono::milliseconds(ms));

	auto active = std::make_shared<std::atomic<bool>>(true);

	auto pTimer = std::shared_ptr<asio::steady_timer>(std::move(timer));

	pTimer->async_wait(sync->wrap(
		[pTimer, active, callback, handle, tracker](asio::error_code err)
		{
			if(!active->exchange(false))
				return;
			callback(
				static_cast<uint8_t>(err ? odc::CommandStatus::UNDEFINED : odc::CommandStatus::SUCCESS),
				handle);
		}));

	return new C_TimerHandle{std::weak_ptr<asio::steady_timer>(pTimer), active};
}

extern "C" void odc_cancelTimer(void* timer_handle)
{
	if(!timer_handle)
		return;
	auto* handle = static_cast<C_TimerHandle*>(timer_handle);
	handle->active->store(false);
	if(auto timer = handle->weak_timer.lock())
		timer->cancel();
	delete handle;
}

extern "C" void odc_PublishConnectState(void* inst, int state)
{
	if(!inst)
		return;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end())
		return;

	auto cs = static_cast<odc::ConnectState>(state);
	auto event = std::make_shared<odc::EventInfo>(odc::EventType::ConnectState, 0, it->second->GetName());
	event->template SetPayload<odc::EventType::ConnectState>(std::move(cs));
	it->second->PublicPublishEvent(event);
}
