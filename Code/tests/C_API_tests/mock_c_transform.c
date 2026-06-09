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
 * mock_c_transform.c
 *
 *  Created on: 24/05/2026
 *      Author: C API test mock
 *
 *  Copy-paste-able template for implementing a C transform library.
 *
 *  Key points:
 *    - odc_transform_event() receives a pass callback — call it to
 *      forward the (possibly modified) event downstream, or skip it
 *      to drop the event.
 */

#include <opendatacon/odc_c_api.h>

#include <stdlib.h>
#include <string.h>

struct mock_transform_state
{
	char name[64];
	int built;
	int enabled;
	int event_count;
};

void* odc_transform_create(const char* name, const char* params_json)
{
	(void)params_json;
	struct mock_transform_state* state = (struct mock_transform_state*)calloc(1, sizeof(struct mock_transform_state));
	if(state && name)
		strncpy(state->name, name, sizeof(state->name) - 1);
	return state;
}

void odc_transform_destroy(void* inst)
{
	if(inst)
		free(inst);
}

void odc_transform_enable(void* inst)
{
	if(!inst)
		return;
	((struct mock_transform_state*)inst)->enabled = 1;
}

void odc_transform_disable(void* inst)
{
	if(!inst)
		return;
	((struct mock_transform_state*)inst)->enabled = 0;
}

void odc_transform_event(void* inst, struct C_EventInfo* event, C_PassContext* pass_ctx, void (*pass)(C_PassContext* ctx, struct C_EventInfo* evt))
{
	if(!inst || !pass_ctx || !pass)
		return;
	((struct mock_transform_state*)inst)->event_count++;
	/* Forward the event downstream unchanged */
	pass(pass_ctx, event);
}
