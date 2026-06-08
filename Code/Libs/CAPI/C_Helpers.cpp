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
#include "CAPI/C_Transform.h"
#include "CAPI/C_UI.h"
#include "C_Internal.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>
#include <whereami++.h>
#include <filesystem>
#include <cstring>

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
			dst->binary_val = src.GetPayload<EventType::Binary>() ? 1 : 0;
			break;
		case EventType::BinaryOutputStatus:
			dst->binary_val = src.GetPayload<EventType::BinaryOutputStatus>() ? 1 : 0;
			break;
		case EventType::DoubleBitBinary:
		{
			auto dbb = src.GetPayload<EventType::DoubleBitBinary>();
			dst->dbb_val.a = dbb.first ? 1 : 0;
			dst->dbb_val.b = dbb.second ? 1 : 0;
			break;
		}
		case EventType::Analog:
			dst->analog_val = src.GetPayload<EventType::Analog>();
			break;
		case EventType::AnalogOutputStatus:
			dst->analog_val = src.GetPayload<EventType::AnalogOutputStatus>();
			break;
		case EventType::Counter:
			dst->counter_val = src.GetPayload<EventType::Counter>();
			break;
		case EventType::FrozenCounter:
			dst->counter_val = src.GetPayload<EventType::FrozenCounter>();
			break;
		case EventType::BinaryCommandEvent:
			dst->cmd_status = static_cast<uint8_t>(src.GetPayload<EventType::BinaryCommandEvent>());
			break;
		case EventType::AnalogCommandEvent:
			dst->cmd_status = static_cast<uint8_t>(src.GetPayload<EventType::AnalogCommandEvent>());
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
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::BinaryQuality>());
			break;
		case EventType::DoubleBitBinaryQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::DoubleBitBinaryQuality>());
			break;
		case EventType::AnalogQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::AnalogQuality>());
			break;
		case EventType::CounterQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::CounterQuality>());
			break;
		case EventType::BinaryOutputStatusQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::BinaryOutputStatusQuality>());
			break;
		case EventType::FrozenCounterQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::FrozenCounterQuality>());
			break;
		case EventType::AnalogOutputStatusQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::AnalogOutputStatusQuality>());
			break;
		case EventType::OctetStringQuality:
			dst->quality_val = static_cast<uint16_t>(src.GetPayload<EventType::OctetStringQuality>());
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
			event->template SetPayload<odc::EventType::Binary>(cevt->payload.binary_val != 0);
			break;
		case odc::EventType::BinaryOutputStatus:
			event->template SetPayload<odc::EventType::BinaryOutputStatus>(cevt->payload.binary_val != 0);
			break;
		case odc::EventType::DoubleBitBinary:
		{
			auto dbb = odc::DBB(cevt->payload.dbb_val.a != 0, cevt->payload.dbb_val.b != 0);
			event->template SetPayload<odc::EventType::DoubleBitBinary>(std::move(dbb));
			break;
		}
		case odc::EventType::Analog:
		{
			auto tmp_a = cevt->payload.analog_val;
			event->template SetPayload<odc::EventType::Analog>(std::move(tmp_a));
			break;
		}
		case odc::EventType::AnalogOutputStatus:
		{
			auto tmp_aos = cevt->payload.analog_val;
			event->template SetPayload<odc::EventType::AnalogOutputStatus>(std::move(tmp_aos));
			break;
		}
		case odc::EventType::Counter:
		{
			auto tmp_c = cevt->payload.counter_val;
			event->template SetPayload<odc::EventType::Counter>(std::move(tmp_c));
			break;
		}
		case odc::EventType::FrozenCounter:
		{
			auto tmp_fc = cevt->payload.counter_val;
			event->template SetPayload<odc::EventType::FrozenCounter>(std::move(tmp_fc));
			break;
		}
		case odc::EventType::BinaryCommandEvent:
			event->template SetPayload<odc::EventType::BinaryCommandEvent>(
				static_cast<odc::CommandStatus>(cevt->payload.cmd_status));
			break;
		case odc::EventType::AnalogCommandEvent:
			event->template SetPayload<odc::EventType::AnalogCommandEvent>(
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
			event->template SetPayload<odc::EventType::BinaryQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::DoubleBitBinaryQuality:
			event->template SetPayload<odc::EventType::DoubleBitBinaryQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::AnalogQuality:
			event->template SetPayload<odc::EventType::AnalogQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::CounterQuality:
			event->template SetPayload<odc::EventType::CounterQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::BinaryOutputStatusQuality:
			event->template SetPayload<odc::EventType::BinaryOutputStatusQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::FrozenCounterQuality:
			event->template SetPayload<odc::EventType::FrozenCounterQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::AnalogOutputStatusQuality:
			event->template SetPayload<odc::EventType::AnalogOutputStatusQuality>(
				static_cast<odc::QualityFlags>(cevt->payload.quality_val));
			break;
		case odc::EventType::OctetStringQuality:
			event->template SetPayload<odc::EventType::OctetStringQuality>(
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

extern "C" const char* odc_GetConfigJSON(void* inst)
{
	if(!inst)
		return nullptr;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end())
		return nullptr;

	return it->second->GetConfigStr().c_str();
}

extern "C" void odc_Log(void* inst, uint8_t level, const char* message)
{
	if(!inst || !message)
		return;
	{   auto it = odc::C_Port_instances.find(inst);
	    if(it != odc::C_Port_instances.end()) { it->second->Log(level, message); return; } }
	{   auto it = odc::C_Transform_instances.find(inst);
	    if(it != odc::C_Transform_instances.end()) { it->second->Log(level, message); return; } }
	{   auto it = odc::C_UI_instances.find(inst);
	    if(it != odc::C_UI_instances.end()) { it->second->Log(level, message); return; } }
}

extern "C" int odc_ShouldLog(void* inst, uint8_t level)
{
	if(!inst) return 0;
	{   auto it = odc::C_Port_instances.find(inst);
	    if(it != odc::C_Port_instances.end()) return it->second->ShouldLog(level) ? 1 : 0;}
	{   auto it = odc::C_Transform_instances.find(inst);
	    if(it != odc::C_Transform_instances.end()) return it->second->ShouldLog(level) ? 1 : 0;}
	{   auto it = odc::C_UI_instances.find(inst);
	    if(it != odc::C_UI_instances.end()) return it->second->ShouldLog(level) ? 1 : 0;}
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Port introspection                                                  */
/* ------------------------------------------------------------------ */

extern "C" int odc_InDemand(void* inst)
{
	if(!inst) return 0;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end()) return 0;
	return it->second->PublicInDemand() ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  Date/time utilities                                                 */
/* ------------------------------------------------------------------ */

extern "C" uint64_t odc_msSinceEpoch(void)
{
	return odc::msSinceEpoch();
}

extern "C" int odc_msSinceEpochToDateTime(uint64_t ms, const char* format,
	char* buf, size_t buflen)
{
	if(!buf || buflen == 0) return -1;
	try
	{
		std::string result = format
		      ? odc::since_epoch_to_datetime(ms, format)
		      : odc::since_epoch_to_datetime(ms);
		if(result.size() + 1 > buflen) return -1;
		std::memcpy(buf, result.c_str(), result.size() + 1);
		return 0;
	}
	catch(...)
	{
		return -1;
	}
}

extern "C" int odc_DateTimeToMsSinceEpoch(const char* datetime, const char* format,
	uint64_t* out_ms)
{
	if(!datetime || !out_ms) return -1;
	try
	{
		*out_ms = format
		      ? odc::datetime_to_since_epoch(datetime, format)
		      : odc::datetime_to_since_epoch(datetime);
		return 0;
	}
	catch(...)
	{
		return -1;
	}
}

/* ------------------------------------------------------------------ */
/*  Hex encode / decode                                                 */
/* ------------------------------------------------------------------ */

extern "C" size_t odc_String2Hex(const uint8_t* data, size_t len,
	char* buf, size_t buflen)
{
	if(!data && len > 0) return 0;
	std::string hex = odc::buf2hex(data, len);
	if(buf && buflen > hex.size())
		std::memcpy(buf, hex.c_str(), hex.size() + 1);
	return hex.size();
}

extern "C" int odc_Hex2String(const char* hex, uint8_t* buf, size_t buflen)
{
	if(!hex) return -1;
	try
	{
		std::vector<uint8_t> result = odc::hex2buf(hex);
		if(!buf) return static_cast<int>(result.size());
		if(result.size() > buflen) return -1;
		std::memcpy(buf, result.data(), result.size());
		return static_cast<int>(result.size());
	}
	catch(...)
	{
		return -1;
	}
}

/* ------------------------------------------------------------------ */
/*  Path resolution                                                     */
/* ------------------------------------------------------------------ */

static int fill_buf(const std::string& s, char* buf, size_t buflen)
{
	if(!buf || s.size() + 1 > buflen) return -1;
	std::memcpy(buf, s.c_str(), s.size() + 1);
	return 0;
}

extern "C" int odc_GetWorkingDir(char* buf, size_t buflen)
{
	try
	{
		return fill_buf(std::filesystem::canonical(std::filesystem::current_path()).string(), buf, buflen);
	}
	catch(...)
	{
		return -1;
	}
}

extern "C" int odc_GetExecutableDir(char* buf, size_t buflen)
{
	try
	{
		return fill_buf(whereami::getExecutablePath().dirname(), buf, buflen);
	}
	catch(...)
	{
		return -1;
	}
}

/* ------------------------------------------------------------------ */
/*  Process spawning                                                    */
/* ------------------------------------------------------------------ */

// Build a std::vector<std::string> of extra args from a NULL-terminated argv
// (skip argv[0] — cmd is the executable, argv[0] is just convention)
static std::vector<std::string> argv_to_args(const char* const* argv)
{
	std::vector<std::string> args;
	if(!argv) return args;
	// skip argv[0]
	for(int i = 1; argv[i] != nullptr; ++i)
		args.push_back(argv[i]);
	return args;
}

extern "C" int64_t odc_SpawnDetached(const char* cmd, const char* const* argv)
{
	if(!cmd) return -1;
	try
	{
		return static_cast<int64_t>(spawn_detached(cmd, argv_to_args(argv)));
	}
	catch(...)
	{
		return -1;
	}
}

extern "C" int64_t odc_SpawnAttached(const char* cmd, const char* const* argv,
	FILE** stdin_file, FILE** stdout_file, FILE** stderr_file)
{
	if(stdin_file)  *stdin_file  = nullptr;
	if(stdout_file) *stdout_file = nullptr;
	if(stderr_file) *stderr_file = nullptr;
	if(!cmd) return -1;
	try
	{
		auto r = spawn_attached(cmd, argv_to_args(argv));
		if(stdin_file)  *stdin_file  = r.stdin_file;
		if(stdout_file) *stdout_file = r.stdout_file;
		if(stderr_file) *stderr_file = r.stderr_file;
		return static_cast<int64_t>(r.pid);
	}
	catch(...)
	{
		return -1;
	}
}

extern "C" int odc_KillPid(int64_t pid, int sig)
{
	try
	{
		spawn_kill(static_cast<int>(pid), sig);
		return 0;
	}
	catch(...)
	{
		return -1;
	}
}

extern "C" int odc_WaitPid(int64_t pid, int nohang, int* exit_code)
{
	try
	{
		auto [exited, code] = spawn_wait(static_cast<int>(pid), nohang != 0);
		if(exited && exit_code) *exit_code = code;
		return exited ? 1 : 0;
	}
	catch(...)
	{
		return -1;
	}
}

/* ------------------------------------------------------------------ */
/*  Repeating timer                                                     */
/* ------------------------------------------------------------------ */

extern "C" void* odc_msRepeatingCallback(void* inst, uint64_t initial_ms,
	C_RepeatingCallbackFunc_t callback, void* handle)
{
	if(!inst || !callback) return nullptr;
	auto it = odc::C_Port_instances.find(inst);
	if(it == odc::C_Port_instances.end()) return nullptr;

	auto* port   = it->second;
	auto sync    = port->GetStrand();
	auto tracker = port->GetHandlerTracker();

	// make_steady_timer returns a unique_ptr; move into shared_ptr so the
	// lambda (stored in std::function) can capture it by copy.
	auto pTimer = std::shared_ptr<asio::steady_timer>(
		odc::asio_service::Get()->make_steady_timer());
	auto active = std::make_shared<std::atomic<bool>>(true);

	// pFn holds the handler std::function alive for the duration of the loop.
	// The lambda captures a weak_ptr to pFn — this breaks the ownership cycle
	// while still allowing the lambda to re-schedule itself.  pFn is stored in
	// C_TimerHandle::extra so it stays alive until the handle is cancelled/deleted.
	auto pFn = std::make_shared<std::function<void(asio::error_code)>>();
	std::weak_ptr<std::function<void(asio::error_code)>> weakFn(pFn);

	*pFn = [pTimer, active, callback, handle, sync, tracker, weakFn]
	       (asio::error_code err) mutable
		 {
			 if(!active->load() || err) return;

			 int64_t next_ms = callback(handle);
			 if(next_ms < 0 || !active->load()) return;

			 auto strongFn = weakFn.lock();
			 if(!strongFn) return;

			 pTimer->expires_from_now(std::chrono::milliseconds(next_ms));
			 pTimer->async_wait(sync->wrap(*strongFn));
		 };

	pTimer->expires_from_now(std::chrono::milliseconds(initial_ms));
	pTimer->async_wait(sync->wrap(*pFn));

	auto* th = new C_TimerHandle{
		std::weak_ptr<asio::steady_timer>(pTimer),
		active,
		pFn // keeps weakFn lockable for the life of the handle
	};
	return th;
}

/* ------------------------------------------------------------------ */
/*  Enum → string helpers                                               */
/* ------------------------------------------------------------------ */

extern "C" const char* odc_EventTypeToString(uint8_t event_type)
{
	static thread_local std::string buf;
	buf = odc::ToString(static_cast<odc::EventType>(event_type));
	return buf.c_str();
}

extern "C" const char* odc_CommandStatusToString(uint8_t status)
{
	static thread_local std::string buf;
	buf = odc::ToString(static_cast<odc::CommandStatus>(status));
	return buf.c_str();
}

extern "C" const char* odc_ControlCodeToString(uint8_t code)
{
	static thread_local std::string buf;
	buf = odc::ToString(static_cast<odc::ControlCode>(code));
	return buf.c_str();
}

extern "C" const char* odc_ConnectStateToString(uint8_t state)
{
	static thread_local std::string buf;
	buf = odc::ToString(static_cast<odc::ConnectState>(state));
	return buf.c_str();
}

extern "C" int odc_QualityFlagsToString(uint16_t flags, char* buf, size_t buflen)
{
	if(!buf || buflen == 0) return -1;
	std::string s = odc::ToString(static_cast<odc::QualityFlags>(flags));
	if(s.size() + 1 > buflen)
	{
		// truncate but always NUL-terminate
		std::memcpy(buf, s.c_str(), buflen - 1);
		buf[buflen - 1] = '\0';
		return -1;
	}
	std::memcpy(buf, s.c_str(), s.size() + 1);
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Host API vtable — populated once, passed to port/transform/plugin */
/*  libraries via odc_library_init() so they have no link-time deps.  */
/* ------------------------------------------------------------------ */
C_ODC_HostAPI g_host_api = {
	odc_InvokeStatusCallback,
	odc_PublishEvent,
	odc_PublishConnectState,
	odc_GetConfigJSON,
	odc_Log,
	odc_ShouldLog,
	odc_msTimerCallback,
	odc_cancelTimer,
	odc_msRepeatingCallback,
	odc_InDemand,
	odc_msSinceEpoch,
	odc_msSinceEpochToDateTime,
	odc_DateTimeToMsSinceEpoch,
	odc_String2Hex,
	odc_Hex2String,
	odc_GetWorkingDir,
	odc_GetExecutableDir,
	odc_SpawnDetached,
	odc_SpawnAttached,
	odc_KillPid,
	odc_WaitPid,
	odc_EventTypeToString,
	odc_CommandStatusToString,
	odc_ControlCodeToString,
	odc_ConnectStateToString,
	odc_QualityFlagsToString
};
