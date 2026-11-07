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
 *  Translation.h
 *
 *  Lua implementation of the libODC translation methods
 *
 *  A Lua script is used for a translation by defining some of these global functions:
 *
 *    serialise(event, sender_name, extra_fields) -> string
 *        Serialise an event to a message payload.
 *        extra_fields is a table of the point's extra (<POINT:...>) name/value strings.
 *
 *    deserialise(payload, key, hint) -> event | nil
 *        Deserialise a message payload to an event table, or nil if it can't be.
 *        key is the message key (a string), or nil if there wasn't one.
 *        hint is nil, or a table { EventType, Index, SourcePort } if the key identifies the point.
 *        If there's a hint, the key wins: the returned event's Index and SourcePort are replaced with
 *        the hint's, and the returned event must have the hint's EventType.
 *
 *    key(event, sender_name, extra_fields) -> string
 *        Generate the key for a point. The event only has the point's identity (no real payload,
 *        quality or timestamp), unless KeyClashDetection is DYNAMIC. The result must not depend on anything else.
 *
 *  Events are tables, as in the LuaPort. The odc table has FillTemplate() and CBOREncode() functions.
 *  Timer functions aren't supported in translation scripts.
 */
#ifndef LUA_TRANSLATION_H
#define LUA_TRANSLATION_H

//Registers the Lua translation method with the odc::TranslationFactory.
//	There's no need to call this: the factory loads LuaWrappers and calls it when Lua is first needed
//	(that's why it has C linkage - so it can be found by name). Linking to LuaWrappers and calling it
//	explicitly is fine too.
extern "C" void RegisterLuaTranslation();

#endif // LUA_TRANSLATION_H
