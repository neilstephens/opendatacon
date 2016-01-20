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
#include <signal.h>

/// Dynamic library loading
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
const std::string DYNLIBPRE = "";
#ifdef _DEBUG
const std::string DYNLIBEXT = "d.dll";
#else
const std::string DYNLIBEXT = ".dll";
#endif

inline HMODULE LoadModule(const std::string& a)
{
	return LoadLibraryExA(a.c_str(), 0, DWORD(0));
}

inline FARPROC LoadSymbol(HMODULE a, const std::string& b)
{
	return GetProcAddress(a, b.c_str());
}

// Place any OS specific initilisation code for library loading here, run once on program startup
inline void InitLibaryLoading()
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	SetDllDirectory(L"plugin");
}

// Retrieve the system error message for the last-error code
inline std::string LastSystemError()
{
	//void* lpMsgBuf = nullptr;
	LPSTR lpMsgBuf = nullptr;
	DWORD dw = GetLastError();

	auto res = FormatMessageA(
	      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	      NULL,
	      dw,
	      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	      (LPSTR)&lpMsgBuf,
	      0,
	      NULL);
	std::string message;
	if (res > 0)
	{
		message = lpMsgBuf;
		message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
		message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
	}
	else
	{
		message = "Unknown error";
	}

	if (lpMsgBuf) LocalFree(lpMsgBuf);
	return message;
}

#else
#include <dlfcn.h>
const std::string DYNLIBPRE = "lib";
#if defined(__APPLE__)
const std::string DYNLIBEXT = ".so";
#else
const std::string DYNLIBEXT = ".so";
#endif

inline void* LoadModule(const std::string& a)
{
	return dlopen(a.c_str(), RTLD_LAZY);
}
inline void* LoadSymbol(void* a, const std::string& b)
{
	return dlsym(a, b.c_str());
}

// Place any OS specific initilisation code for library loading here, run once on program startup
inline void InitLibaryLoading()
{}

// Retrieve the system error message for the last-error code
inline std::string LastSystemError()
{
	std::string message;
	char *error;
	if ((error = dlerror()) != NULL)
		message = error;
	else
		message = "Unknown error";

	return message;
}
#endif

/// Posix file system directory manipulation - e.g. chdir
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <direct.h>
inline int ChangeWorkingDir(const std::string& dir)
{
	return _chdir(dir.c_str());
}
#else
#include <unistd.h>
inline int ChangeWorkingDir(const std::string& dir)
{
	return chdir(dir.c_str());
}
#endif

/// Implement reentrant and portable strerror function
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
inline char* strerror_rp(int therr, char* buf, size_t len)
{
	strerror_s(buf, len, therr);
	return buf;
}
#elif defined(_GNU_SOURCE)
// non-posix GNU-specific function
#include <string.h>
inline char* strerror_rp(int therr, char* buf, size_t len)
{
	return strerror_r(therr, buf, len);
}
#else
// posix function
inline char* strerror_rp(int therr, char* buf, size_t len)
{
	strerror_r(therr, buf, len);
	return buf;
}
#endif

/// Platform specific signal definitions
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
const auto SIG_SHUTDOWN = { SIGTERM, SIGABRT, SIGBREAK };
const auto SIG_IGNORE = { SIGINT };
#else
static const std::initializer_list<u_int8_t> SIG_SHUTDOWN = { SIGTERM, SIGABRT, SIGQUIT };
static const std::initializer_list<u_int8_t> SIG_IGNORE = { SIGINT };
#endif

inline std::string GetLibFileName(const std::string LibName)
{
	return DYNLIBPRE + LibName + DYNLIBEXT;
}

#endif
