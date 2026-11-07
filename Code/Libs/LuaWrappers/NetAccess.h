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
 * NetAccess.h
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  Whether a Lua script gets the network objects (odc.TCPSocket, odc.TLSSocket, odc.HTTPServer, odc.HTTPClient, odc.HTTP): the "NetEnabled"
 *  configuration item of each Lua host (port, transform, log sink, UI command scripts).
 *
 *	true                   every script
 *	false                  none
 *	"FILE_SCRIPTS_ONLY"    (the default, when it is absent or null) scripts loaded from a file, not code given directly (UI Base64Execute)
 *	{ "Allow": [...] }     scripts loaded from a file that is, or is under a directory that is, in the list
 *
 *  Any other value is an error: no network access (fail closed). Paths are compared after resolving symbolic links and "..", and relative paths
 *  are relative to the working directory, as the script files themselves are.
 */
#ifndef NETACCESS_H
#define NETACCESS_H

#include <json/json.h>
#include <string>

struct NetAccessDecision
{
	bool allowed = false;
	bool config_error = false; //NetEnabled has a value that is not valid (then allowed is false)
	std::string why;           //for the log
};

//script_file: the file the script was loaded from, or nullptr for code given directly
NetAccessDecision DecideNetAccess(const Json::Value& NetEnabled, const std::string* script_file);

#endif // NETACCESS_H
