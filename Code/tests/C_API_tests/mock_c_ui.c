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
 * mock_c_ui.c
 *
 *  Created on: 24/05/2026
 *      Author: C API test mock
 *
 *  Copy-paste-able template for implementing a C UI plugin library.
 */

#include <opendatacon/odc_c_api.h>

#include <stdlib.h>
#include <string.h>

struct mock_ui_state
{
	char name[64];
	int built;
	int enabled;
};

void* odc_plugin_create(const char* name, const char* conf_filename, const char* conf_overrides_json)
{
	(void)conf_filename;
	(void)conf_overrides_json;
	struct mock_ui_state* state = (struct mock_ui_state*)calloc(1, sizeof(struct mock_ui_state));
	if(state && name)
		strncpy(state->name, name, sizeof(state->name) - 1);
	return state;
}

void odc_plugin_destroy(void* inst)
{
	if(inst)
		free(inst);
}

void odc_plugin_build(void* inst)
{
	if(!inst)
		return;
	((struct mock_ui_state*)inst)->built = 1;
}

void odc_plugin_enable(void* inst)
{
	if(!inst)
		return;
	((struct mock_ui_state*)inst)->enabled = 1;
}

void odc_plugin_disable(void* inst)
{
	if(!inst)
		return;
	((struct mock_ui_state*)inst)->enabled = 0;
}
