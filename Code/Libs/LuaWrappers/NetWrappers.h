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
 * NetWrappers.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Network objects for Lua scripts (odc.TCPSocket, odc.TLSSocket). ExportWrappersToLua() registers them when the script is to have network access
 *  (NetAccess.h); a Lua host that builds its own set of wrappers calls ExportNetWrappers() and ExportHttpWrappers() itself.
 */
#ifndef NETWRAPPERS_H
#define NETWRAPPERS_H

#include <Lua/CLua.h>
#include <opendatacon/asio.h>
#include <cstddef>
#include <memory>
#include <string>

//Register the network objects in the global 'odc' table (which must exist). Call on the Lua state's strand, or while only one thread is active.
//The strand and tracker are the Lua host's own, as for ExportWrappersToLua(): callbacks run on the strand, holding the tracker.
void ExportNetWrappers(lua_State* const L,
	std::shared_ptr<odc::strand_t> pSyncStrand,
	std::shared_ptr<void> handler_tracker,
	const std::string& Name,
	const std::string& LogName);

//Close every network object created through L, so there are no more callbacks and no more open sockets (nothing is reported to the script).
//Must be called on the Lua strand. Afterwards objects can not be opened. Does nothing if ExportNetWrappers() was not called for L.
//(The Lua hosts don't call this: a script closes its own objects, and what is left open when the host is destroyed is torn down with the Lua state.)
void CloseNetObjects(lua_State* const L, bool permanent);

//The number of native network objects that exist (created and not yet destroyed): for tests, to show that nothing is leaked.
size_t NetObjectCount();

#endif // NETWRAPPERS_H
