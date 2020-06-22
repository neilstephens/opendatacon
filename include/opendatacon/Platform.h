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
#include <algorithm>
#include <signal.h>
#include <opendatacon/asio.h>

/// Dynamic library loading
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
static const char* DYNLIBPRE = "";
#ifdef _DEBUG
static const char* DYNLIBEXT = "d.dll";
#else
static const char* DYNLIBEXT = ".dll";
#endif

typedef HMODULE module_ptr;
inline HMODULE LoadModule(const std::string& a)
{
	//LoadLibrary is ref counted so you can call FreeLibrary the same number of times to unload
	return LoadLibraryExA(a.c_str(), 0, DWORD(0));
}
inline BOOL WINAPI UnLoadModule(HMODULE handle)
{
	//LoadLibrary is ref counted so you can call FreeLibrary the same number of times to unload
	return FreeLibrary(handle);
}

typedef FARPROC symbol_ptr;
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

inline void PlatformSetEnv(const char* var, const char* val, int overwrite)
{
	_putenv_s(var, val);
}
static constexpr const char* OSPATHSEP = ";";

#else
#include <dlfcn.h>
static const char* DYNLIBPRE = "lib";
#if defined(__APPLE__)
static const char* DYNLIBEXT = ".so";
#else
static const char* DYNLIBEXT = ".so";
#endif

typedef void* module_ptr;
inline void* LoadModule(const std::string& a)
{
	//dlopen is ref counted, so you can call dlclose for every time you call dlopen
	return dlopen(a.c_str(), RTLD_LAZY|RTLD_LOCAL);
}
inline int UnLoadModule(void* handle)
{
	//dlopen is ref counted, so you can call dlclose for every time you call dlopen
	return dlclose(handle);
}

typedef void* symbol_ptr;
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
	if ((error = dlerror()) != nullptr)
		message = error;
	else
		message = "Unknown error";

	return message;
}

inline void PlatformSetEnv(const char* var, const char* val, int overwrite)
{
	setenv(var, val, overwrite);
}
static constexpr const char* OSPATHSEP = ":";

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
//#elif defined(_GNU_SOURCE)
//// non-posix GNU-specific function
//#include <string.h>
//inline char* strerror_rp(int therr, char* buf, size_t len)
//{
//	return strerror_r(therr, buf, len);
//}
#else
// posix function
#include <string.h>
inline char* strerror_rp(int therr, char* buf, size_t len)
{
	if(strerror_r(therr, buf, len) == 0)
		return buf;
	else
		return nullptr;
}
#endif

/// Platform specific signal definitions
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
const auto SIG_SHUTDOWN = { SIGTERM, SIGABRT, SIGBREAK };
const auto SIG_IGNORE = { SIGINT };
#else
static const std::initializer_list<u_int8_t> SIG_SHUTDOWN = { SIGTERM, SIGABRT, SIGQUIT };
static const std::initializer_list<u_int8_t> SIG_IGNORE = { SIGINT, SIGTSTP };
#endif

/// Platform specific socket options
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <Mstcpip.h>
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	DWORD dwBytesRet;
	tcp_keepalive alive;
	alive.onoff = enable;
	alive.keepalivetime = initial_interval_s * 1000;
	alive.keepaliveinterval = subsequent_interval_s * 1000;

	if(WSAIoctl(tcpsocket.native_handle(), SIO_KEEPALIVE_VALS, &alive, sizeof(alive), nullptr, 0, &dwBytesRet, nullptr, nullptr) == SOCKET_ERROR)
		throw std::runtime_error("Failed to set TCP SIO_KEEPALIVE_VALS");
}
#elif __APPLE__
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	int set = enable ? 1 : 0;
	if(setsockopt(tcpsocket.native_handle(), SOL_SOCKET,  SO_KEEPALIVE, &set, sizeof(set)) != 0)
		throw std::runtime_error("Failed to set TCP SO_KEEPALIVE");
	if(setsockopt(tcpsocket.native_handle(), IPPROTO_TCP, TCP_KEEPALIVE, &initial_interval_s, sizeof(initial_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPALIVE");
}
#else
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	int set = enable ? 1 : 0;
	if(setsockopt(tcpsocket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &set, sizeof(set)) != 0)
		throw std::runtime_error("Failed to set TCP SO_KEEPALIVE");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPIDLE, &initial_interval_s, sizeof(initial_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPIDLE");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPINTVL, &subsequent_interval_s, sizeof(subsequent_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPINTVL");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPCNT, &fail_count, sizeof(fail_count)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPCNT");
}
#endif


inline std::string GetLibFileName(const std::string& LibName)
{
	return DYNLIBPRE + LibName + DYNLIBEXT;
}

#endif //ODC_PLATFORM_H_
