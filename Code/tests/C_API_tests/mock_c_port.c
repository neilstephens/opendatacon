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
 * mock_c_port.c
 *
 *  Created on: 24/05/2026
 *      Author: C API test mock
 *
 *  Copy-paste-able template for implementing a C port library.
 *  Compile this into a shared library (MODULE) that exports
 *  the required odc_port_* symbols.
 *
 *  Key points:
 *    - odc_port_create() receives (type, name) — type comes from config
 *    - During odc_port_build(), call odc_GetConfigJSON(inst) to read
 *      the full resolved config JSON
 *    - Use odc_Log(inst, level, msg) for logging
 *    - Use odc_PublishEvent(inst, event, cb, handle) to publish events
 *    - Use odc_InvokeStatusCallback(&cb, status) to respond
 */

#include <opendatacon/odc_c_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Per-instance state */
struct mock_port_state
{
	char type[64];
	char name[64];
	int built;
	int enabled;
	int event_count;
};

const char* odc_c_api_version(void)
{
	return "1.0";
}

void* odc_port_create(const char* type, const char* name)
{
	struct mock_port_state* state = (struct mock_port_state*)calloc(1, sizeof(struct mock_port_state));
	if(state)
	{
		if(type)
			strncpy(state->type, type, sizeof(state->type) - 1);
		if(name)
			strncpy(state->name, name, sizeof(state->name) - 1);
	}
	return state;
}

void odc_port_destroy(void* inst)
{
	if(inst)
		free(inst);
}

void odc_port_build(void* inst)
{
	if(!inst)
		return;
	struct mock_port_state* state = (struct mock_port_state*)inst;
	state->built = 1;

	/* Demonstrate retrieving config JSON during build */
	const char* json = odc_GetConfigJSON(inst);
	if(json)
		odc_Log(inst, 2, "mock_c_port: build with config available");
}

void odc_port_enable(void* inst)
{
	if(!inst)
		return;
	((struct mock_port_state*)inst)->enabled = 1;
}

void odc_port_disable(void* inst)
{
	if(!inst)
		return;
	((struct mock_port_state*)inst)->enabled = 0;
}

void odc_port_event(void* inst, const struct C_EventInfo* event, const char* sender, C_StatusCallback* cb)
{
	if(!inst)
		return;
	((struct mock_port_state*)inst)->event_count++;

	/* Always invoke the callback to confirm receipt */
	if(cb)
		odc_InvokeStatusCallback(&cb, C_CommandStatus_SUCCESS);
}

/* Optional exports */

const char* odc_port_stats_json(void* inst)
{
	if(!inst)
		return NULL;
	struct mock_port_state* state = (struct mock_port_state*)inst;
	/* malloc'd string — C++ side calls odc_port_free_string() */
	char* buf = (char*)malloc(64);
	if(buf)
		snprintf(buf, 64, "{\"event_count\":%d}", state->event_count);
	return buf;
}

const char* odc_port_state_json(void* inst)
{
	if(!inst)
		return NULL;
	struct mock_port_state* state = (struct mock_port_state*)inst;
	char* buf = (char*)malloc(128);
	if(buf)
		snprintf(buf, 128, "{\"built\":%d,\"enabled\":%d}", state->built, state->enabled);
	return buf;
}

const char* odc_port_status_json(void* inst)
{
	return odc_port_state_json(inst);
}

void odc_port_free_string(const char* str)
{
	free((void*)str);
}
