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
* Platform.h
*
*  Created on: 26/11/2014
*      Author: Alan Murray
*/

#ifndef ODC_PLATFORM_H_
#define ODC_PLATFORM_H_

#include <string>

/// Dynamic library loading
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	const std::string DYNLIBPRE = "";
	const std::string DYNLIBEXT = ".dll";
	#define DYNLIBLOAD(a) LoadLibraryExA(a, 0, DWORD(0))
	#define DYNLIBGETSYM(a,b) GetProcAddress(a, b)
#else
	const std::string DYNLIBPRE = "lib";
	#if defined(__APPLE__)
		const std::string DYNLIBEXT = ".dylib";
	#else
		const std::string DYNLIBEXT = ".so";
	#endif
	#define DYNLIBLOAD(a) dlopen(a, RTLD_LAZY)
	#define DYNLIBGETSYM(a,b) dlsym(a, b)
#endif

/// Posix file system directory manipulation - e.g. chdir
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	#include <direct.h>
	#define CHDIR(a) _chdir(a)
#else
	#include <unistd.h>
	#define CHDIR(a) chdir(a)
#endif

#endif
