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
 * HttpWrappers.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  HTTP objects for Lua scripts (odc.HTTPServer, odc.HTTPClient and the odc.HTTP helpers), on Simple-Web-Server.
 *  Like the socket objects of NetWrappers.h, ExportWrappersToLua() registers them when the script is to have network access (NetAccess.h), and
 *  CloseNetObjects() (NetWrappers.h) closes them with the sockets.
 */
#ifndef HTTPWRAPPERS_H
#define HTTPWRAPPERS_H

#include <Lua/CLua.h>
#include <opendatacon/asio.h>
#include <cstddef>
#include <memory>
#include <string>

//Register the HTTP objects in the global 'odc' table (which must exist). Call on the Lua state's strand, or while only one thread is active.
void ExportHttpWrappers(lua_State* const L,
	std::shared_ptr<odc::strand_t> pSyncStrand,
	std::shared_ptr<void> handler_tracker,
	const std::string& Name,
	const std::string& LogName);

//Stop every HTTP server and client created through L, with no more handler calls (see CloseNetObjects). On the Lua strand.
void CloseHttpObjects(lua_State* const L, bool permanent);

//The number of native HTTP objects (servers and clients) that exist and are not yet destroyed
size_t HttpObjectCount();

#endif // HTTPWRAPPERS_H
