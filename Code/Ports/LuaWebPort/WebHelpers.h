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
//  WebHelpers.h
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#ifndef __opendatacon__WebHelpers__
#define __opendatacon__WebHelpers__

#include <opendatacon/asio.h>
#include <opendatacon/util.h>
#include <opendatacon/ParamCollection.h>
#ifdef USE_HTTPS
#include <server_https.hpp>
using WebServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
#define OPTIONAL_CERTS web_crt, web_key
#else
#include <server_http.hpp>
using WebServer = SimpleWeb::Server<SimpleWeb::HTTP>;
#define OPTIONAL_CERTS
#endif
#include <fstream>
#include <string>
#include <unordered_map>

const std::string& GetMimeType(const std::string& rUrl);

#endif /* defined(__opendatacon__WebHelpers__) */
