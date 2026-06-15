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
 * LuaPipe.h
 *
 *  Created on: 15/06/2026
 *      Author: Neil Stephens
 */

#ifndef LUAPIPE_H
#define LUAPIPE_H

#include <lua.h>
#include <lauxlib.h>

/*
** Push a full Lua file handle (LUA_FILEHANDLE) onto the stack, backed by
** the given OS-level pipe endpoint.
**
**   os_handle : HANDLE on Windows, (void*)(intptr_t)fd on POSIX.
**   mode      : "r" for the read end, "w" for the write end.
**
** Compiled into lua54, so FILE*s don't cross lib boundaries
**
** Pushes nil on failure.
*/
LUALIB_API void lua_pushpipe(lua_State* L, void* os_handle, const char* mode);

#endif /* LUAPIPE_H */
