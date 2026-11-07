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
 *  TranslationWrappers.h
 *
 *  Both functions take the same trailing arguments, and raise a Lua error (rather than returning nil)
 *  if the template/structure is invalid or the event table can't be converted to an EventInfo.
 *  The extra_fields table supplies the values of <POINT:name> placeholders (or "POINT:name" CBOR strings).
 *
 *  Exports the libODC translation facilities to Lua, as functions in the 'odc' table:
 *    odc.FillTemplate(template, event [, sender_name [, extra_fields]]) -> string
 *    odc.CBOREncode(structure_json, event [, sender_name [, extra_fields]]) -> string (binary)
 */
#ifndef TRANSLATIONWRAPPERS_H
#define TRANSLATIONWRAPPERS_H

#include <Lua/CLua.h>
#include <opendatacon/Translation.h>

//pConf is used for datetime/octet string formatting. It must outlive the lua_State.
//	If null, the defaults are used.
void ExportTranslationWrappers(lua_State* const L, const odc::TranslationConfig* pConf = nullptr);

#endif // TRANSLATIONWRAPPERS_H
