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
//
//  MhdWrapper.h
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#ifndef __opendatacon__MhdWrapper__
#define __opendatacon__MhdWrapper__


#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <cerrno>

#ifdef _WIN32
#include <io.h>
#include <ws2tcpip.h>
#include <stdarg.h>
#include <stdint.h>
typedef SSIZE_T ssize_t;
//#define S_ISREG(a) true
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG) // regular file
#include <sys/types.h>                              // off_t, ssize_t
#define MHD_PLATFORM_H
#else
inline void fopen_s(FILE** file, const char * name, const char * mode)
{
	*file = fopen(name, mode);
}
#endif

#include <microhttpd.h>
#include <opendatacon/ParamCollection.h>
#include <unordered_map>

struct connection_info_struct
{
	ParamCollection PostValues;
	struct MHD_PostProcessor *postprocessor = nullptr;
};


const std::string& GetMimeType(const std::string& rUrl);
const std::string GetPath(const std::string& rUrl);
const std::string GetFile(const std::string& rUrl);

int CreateNewRequest(void *cls,
	struct MHD_Connection *connection,
	const std::string& url,
	const std::string& method,
	const std::string& version,
	const std::string& upload_data,
	size_t& upload_data_size,
	void **con_cls);
void request_completed(void *cls, struct MHD_Connection *connection,
	void **con_cls,
	enum MHD_RequestTerminationCode toe);
int ReturnFile(struct MHD_Connection *connection, const std::string& url);
int ReturnJSON(struct MHD_Connection *connection, const std::string& json_str);

#endif /* defined(__opendatacon__MhdWrapper__) */
