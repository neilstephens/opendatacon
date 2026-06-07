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
 * C_Internal.h
 *
 *  Created on: 23/05/2026
 *      Author: C API auto-generated
 */

#ifndef C_INTERNAL_H
#define C_INTERNAL_H

#include <opendatacon/Platform.h>
#include <opendatacon/IOHandler.h>
#include <opendatacon/Transform.h>
#include <opendatacon/LogHelpers.h>
#include <atomic>
#include <cstdint>
#include <unordered_map>

#include "CAPI/C_Port.h"

//  Internal C_StatusCallback — opaque to C, wraps pSharedStatusCallback
struct C_StatusCallback
{
	odc::SharedStatusCallback_t cb;
	explicit C_StatusCallback(odc::SharedStatusCallback_t&& c): cb(std::move(c)) {}
};

//  Internal C_PassContext — opaque to C, wraps the pAllow callback
struct C_PassContext
{
	odc::EvtHandler_ptr pAllow;
	explicit C_PassContext(odc::EvtHandler_ptr&& a): pAllow(std::move(a)) {}
};

//  Internal C_TimerHandle — opaque cancel handle for odc_msTimerCallback
struct C_TimerHandle
{
	std::weak_ptr<asio::steady_timer> weak_timer;
	std::shared_ptr<std::atomic<bool>> active;
	std::shared_ptr<void> extra; // optional — used by odc_msRepeatingCallback to keep the handler alive
};

namespace odc
{
extern std::unordered_map<void*, C_Port*> C_Port_instances;
}

void SerializePayload(const odc::EventInfo& src, union C_Payload* dst);

/* Host API vtable — defined in C_Helpers.cpp, used by C_Port/Transform/UI
   constructors to call odc_library_init() on each loaded library. */
extern C_ODC_HostAPI g_host_api;

#endif // C_INTERNAL_H
