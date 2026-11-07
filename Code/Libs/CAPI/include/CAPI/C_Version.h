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
 * C_Version.h
 *
 *  C API version negotiation between the host (this library) and loaded C libraries.
 *
 *  Versions are strings of the form "major.minor", as returned by a C library's odc_c_api_version().
 *    - minor increases for backward compatible additions, which are only ever appended
 *      to the end of C_ODC_HostAPI, so a library built against an older minor never reads past
 *      the members it knows about.
 *    - major increases when anything is removed, reordered or changes signature.
 *  A library is compatible with a host of the same major version and an equal or newer minor version.
 *  A library built against a newer minor than the host is refused, because the host's vtable would be
 *  too short for the members it expects.
 */

#ifndef C_VERSION_H
#define C_VERSION_H

#include <string>

namespace odc
{

//Whether a C API library that declares lib_version (from odc_c_api_version()) can be loaded by this host.
//	The major versions must be equal, and the library's minor version can't be greater than the host's
//	(ODC_C_API_VERSION). A version that isn't "major.minor" is incompatible.
bool CAPIVersionCompatible(const std::string& lib_version);

}

#endif // C_VERSION_H
