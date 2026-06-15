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
 * LuaPipe.c
 *
 *  Created on: 15/06/2026
 *      Author: Neil Stephens
 */

/* Must define LUA_LIB before including lua headers so LUALIB_API resolves to
** __declspec(dllexport) when building lua54.dll with LUA_BUILD_AS_DLL. */
#define LUA_LIB
#include "lua.h"
#include "lauxlib.h"
#include "LuaPipe.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/* Close function for pipe-backed file handles.
** Compiled into lua54 so fclose uses lua54's own CRT instance. */
static int pipe_fclose(lua_State* L)
{
	luaL_Stream* p = (luaL_Stream*)lua_touserdata(L, 1);
	if (p && p->f)
	{
		int res = fclose(p->f);
		p->f = NULL;
		p->closef = NULL;
		return luaL_fileresult(L, (res == 0), NULL);
	}
	return luaL_fileresult(L, 0, NULL);
}

LUALIB_API void lua_pushpipe(lua_State* L, void* os_handle, const char* mode)
{
	FILE* f;
	luaL_Stream* p;

	if(!os_handle)
	{
		lua_pushnil(L);
		return;
	}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	{
		int flags = (mode[0] == 'w') ? (_O_WRONLY | _O_TEXT) : (_O_RDONLY | _O_TEXT);
		int fd = _open_osfhandle((intptr_t)os_handle, flags);
		if(fd < 0) { lua_pushnil(L); return; }
		f = _fdopen(fd, mode);
		if(!f) { _close(fd); lua_pushnil(L); return; }
	}
#else
	f = fdopen((int)(intptr_t)os_handle, mode);
	if(!f) { lua_pushnil(L); return; }
#endif

	p = (luaL_Stream*)lua_newuserdatauv(L, sizeof(luaL_Stream), 1);
	p->f = f;
	p->closef = pipe_fclose;
	luaL_setmetatable(L, LUA_FILEHANDLE);
}
