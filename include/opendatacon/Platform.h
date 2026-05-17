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
inline HMODULE LoadModule(const std::string& a, bool global = false)
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
	// Avoid "Abort, Retry, Ignore" dialog boxes
	_set_error_mode(_OUT_TO_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);

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

#ifdef MAIN_EXE
#define DllImport   __declspec( dllimport )
#else
#define DllImport
#endif

#else
#include <dlfcn.h>
static const char* DYNLIBPRE = "lib";
#if defined(__APPLE__)
static const char* DYNLIBEXT = ".so";
#else
static const char* DYNLIBEXT = ".so";
#endif

typedef void* module_ptr;
inline void* LoadModule(const std::string& a, bool global = false)
{
	auto flags = global ? RTLD_LAZY|RTLD_GLOBAL : RTLD_LAZY|RTLD_LOCAL;
	//dlopen is ref counted, so you can call dlclose for every time you call dlopen
	return dlopen(a.c_str(), flags);
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
#define DllImport

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
decltype (SIG_IGNORE) SIG_RELOAD = {  };
#else
static const std::initializer_list<u_int8_t> SIG_SHUTDOWN = { SIGTERM, SIGABRT, SIGQUIT };
static const std::initializer_list<u_int8_t> SIG_IGNORE = { SIGINT, SIGTSTP };
static const std::initializer_list<u_int8_t> SIG_RELOAD = { SIGHUP };
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

/// Process Spawning
struct spawn_attached_result
{
	int pid;
	FILE* stdin_file;
	FILE* stdout_file;
	FILE* stderr_file;
};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

#include <io.h>
#include <fcntl.h>

/// args ignored on windows - put it all in the command
inline DWORD spawn_detached(const std::string& cmd, const std::vector<std::string>& args = {})
{
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	std::string commandLine = cmd;
	for (const auto& arg : args)
		commandLine += " " + arg;
	commandLine.push_back('\0'); //null terminator

	//full command needs to be writeable (.data(), not .c_str())
	BOOL success = CreateProcessA(
		NULL, commandLine.data(), NULL, NULL, FALSE,
		DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
		NULL, NULL, &si, &pi
		);

	if (!success)
		throw std::runtime_error(std::string("CreateProcess failed: ") + std::to_string(GetLastError()));

	DWORD pid = pi.dwProcessId;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return pid;
}

inline spawn_attached_result spawn_attached(const std::string& cmd, const std::vector<std::string>& args = {})
{
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// Create pipes for stdin, stdout, stderr
	HANDLE stdin_read = NULL, stdin_write = NULL;
	HANDLE stdout_read = NULL, stdout_write = NULL;
	HANDLE stderr_read = NULL, stderr_write = NULL;

	if(!CreatePipe(&stdin_read, &stdin_write, &sa, 0)
	   || !CreatePipe(&stdout_read, &stdout_write, &sa, 0)
	   || !CreatePipe(&stderr_read, &stderr_write, &sa, 0))
	{
		CloseHandle(stdin_read); CloseHandle(stdin_write);
		CloseHandle(stdout_read); CloseHandle(stdout_write);
		CloseHandle(stderr_read); CloseHandle(stderr_write);
		throw std::runtime_error("CreatePipe failed.");
	}

	// Ensure the read/write handles that parent uses are not inherited
	SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA si = { sizeof(si) };
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = stdin_read;
	si.hStdOutput = stdout_write;
	si.hStdError = stderr_write;

	PROCESS_INFORMATION pi;

	std::string commandLine = cmd;
	for (const auto& arg : args)
		commandLine += " " + arg;
	commandLine.push_back('\0');

	BOOL success = CreateProcessA(
		NULL, commandLine.data(), NULL, NULL, TRUE,
		0, NULL, NULL, &si, &pi
		);

	// Close child's ends of pipes in parent
	CloseHandle(stdin_read);
	CloseHandle(stdout_write);
	CloseHandle(stderr_write);

	if (!success)
	{
		CloseHandle(stdin_write);
		CloseHandle(stdout_read);
		CloseHandle(stderr_read);
		throw std::runtime_error(std::string("CreateProcess failed: ") + std::to_string(GetLastError()));
	}

	CloseHandle(pi.hThread);

	// Convert Windows handles to C FILE*
	int stdin_fd = _open_osfhandle((intptr_t)stdin_write, _O_WRONLY | _O_TEXT);
	int stdout_fd = _open_osfhandle((intptr_t)stdout_read, _O_RDONLY | _O_TEXT);
	int stderr_fd = _open_osfhandle((intptr_t)stderr_read, _O_RDONLY | _O_TEXT);

	FILE* stdin_file = _fdopen(stdin_fd, "w");
	FILE* stdout_file = _fdopen(stdout_fd, "r");
	FILE* stderr_file = _fdopen(stderr_fd, "r");

	if (!stdin_file || !stdout_file || !stderr_file)
	{
		if (stdin_file) fclose(stdin_file);else { _close(stdin_fd); CloseHandle(stdin_write); }
		if (stdout_file) fclose(stdout_file);else { _close(stdout_fd); CloseHandle(stdout_read); }
		if (stderr_file) fclose(stderr_file);else { _close(stderr_fd); CloseHandle(stderr_read); }
		CloseHandle(pi.hProcess);
		throw std::runtime_error("_fdopen failed");
	}

	return spawn_attached_result{(int)pi.dwProcessId, stdin_file, stdout_file, stderr_file};
}

inline std::pair<bool,int> spawn_wait(int pid, bool nohang)
{
	HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (!hProcess)
		throw std::runtime_error(std::string("OpenProcess failed: ") + std::to_string(GetLastError()));

	DWORD timeout = nohang ? 0 : INFINITE;
	DWORD result = WaitForSingleObject(hProcess, timeout);

	if (result == WAIT_FAILED)
	{
		CloseHandle(hProcess);
		throw std::runtime_error(std::string("WaitForSingleObject failed: ") + std::to_string(GetLastError()));
	}

	if (result == WAIT_TIMEOUT)
	{
		CloseHandle(hProcess);
		return { false, 0 };
	}

	DWORD exitCode;
	if (!GetExitCodeProcess(hProcess, &exitCode))
	{
		CloseHandle(hProcess);
		throw std::runtime_error(std::string("GetExitCodeProcess failed: ") + std::to_string(GetLastError()));
	}

	CloseHandle(hProcess);
	return { true, (int)exitCode };
}

inline void spawn_kill(int pid, int sig)
{
	DWORD access = (sig == 0) ? PROCESS_QUERY_INFORMATION : (PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION);
	HANDLE hProcess = OpenProcess(access, FALSE, pid);
	if (!hProcess)
		throw std::runtime_error(std::string("OpenProcess failed: ") + std::to_string(GetLastError()));

	if (sig == 0)
	{
		// sig 0 just checks if process exists/is running
		DWORD exitCode;
		if (GetExitCodeProcess(hProcess, &exitCode))
		{
			CloseHandle(hProcess);
			if (exitCode == STILL_ACTIVE)
				return; // Process is running
			throw std::runtime_error("Process " + std::to_string(pid) + " is not running");
		}
		CloseHandle(hProcess);
		throw std::runtime_error(std::string("GetExitCodeProcess failed: ") + std::to_string(GetLastError()));
	}

	// For non-zero signals, terminate the process
	if (!TerminateProcess(hProcess, 1))
	{
		CloseHandle(hProcess);
		throw std::runtime_error(std::string("TerminateProcess failed: ") + std::to_string(GetLastError()));
	}

	CloseHandle(hProcess);
}

#else

#include <whereami++.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#include <spawn.h>
#include <sys/wait.h>
#include <filesystem>
#include <csignal>

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

inline void add_actions_close_all_fds(posix_spawn_file_actions_t& actions)
{
	#ifdef __linux__
	DIR *dir = opendir("/proc/self/fd");
	if (dir)
	{
		int dir_fd = dirfd(dir);
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)
		{
			int fd = atoi(entry->d_name);
			if(fd > 2 && fd != dir_fd)
				posix_spawn_file_actions_addclose(&actions, fd);
		}
		closedir(dir);
		return;
	}
	#endif
	#ifdef __APPLE__
	posix_spawn_file_actions_addinherit_np(&actions,0);
	posix_spawn_file_actions_addinherit_np(&actions,1);
	posix_spawn_file_actions_addinherit_np(&actions,2);
	return;
	#endif
	// Fallback: use getrlimit
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < 200000)
	{
		for (int fd = 3; fd < (int)rl.rlim_cur; fd++)
			posix_spawn_file_actions_addclose(&actions, fd);
	}
	else
	{
		// Last resort: assume 200,000
		for (int fd = 3; fd < 200000; fd++)
			posix_spawn_file_actions_addclose(&actions, fd);
	}
}

inline void spawn_init(posix_spawnattr_t& attr, posix_spawn_file_actions_t& actions)
{
	// Initialize attributes and file actions
	posix_spawnattr_init(&attr);
	posix_spawn_file_actions_init(&actions);

	// Set flags: new session, reset signals
	short flags = POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
	#ifdef __APPLE__
	flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
	#endif
	posix_spawnattr_setflags(&attr, flags);

	sigset_t empty, all_signals;
	sigemptyset(&empty);
	sigfillset(&all_signals);
	posix_spawnattr_setsigmask(&attr, &empty);
	posix_spawnattr_setsigdefault(&attr, &all_signals);
}

inline int spawn_detached(const std::string& cmd, const std::vector<std::string>& args = {})
{
	pid_t pid;
	posix_spawnattr_t attr;
	posix_spawn_file_actions_t actions;
	spawn_init(attr,actions);

	//open a pipe so the dettached process can report it's pid
	int pid_pipe_fd[2], err_pipe_fd[2]; // [0] = read end, [1] = write end
	if (pipe(pid_pipe_fd) == -1 || pipe(err_pipe_fd) == -1)
	{
		close(pid_pipe_fd[0]);close(pid_pipe_fd[1]);
		close(err_pipe_fd[0]);close(err_pipe_fd[1]);
		throw std::runtime_error("pipe() failed.");
	}

	// spawn_detached writes PID on stdout and errors on stderr
	posix_spawn_file_actions_adddup2(&actions, pid_pipe_fd[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, err_pipe_fd[1], STDERR_FILENO);
	// close all child fds except for stdio
	add_actions_close_all_fds(actions);

	std::vector<char *> argv;
	auto exe_path = std::filesystem::canonical(std::string(whereami::getExecutablePath().dirname())+"/spawn_detached");
	argv.push_back(const_cast<char*>(exe_path.c_str()));
	argv.push_back(const_cast<char*>(cmd.c_str()));
	for (const auto &arg : args)
		argv.push_back(const_cast<char*>(arg.c_str()));
	argv.push_back(nullptr);

	int status = posix_spawn(&pid, exe_path.c_str(), &actions, &attr, argv.data(), environ);

	//post-spawn cleanup
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);
	close(pid_pipe_fd[1]); //don't need write-end of pipes in parent anymore
	close(err_pipe_fd[1]);

	if (status != 0)
	{
		close(pid_pipe_fd[0]);
		close(err_pipe_fd[0]);
		throw std::runtime_error("posix_spawn(...'"+exe_path.string()+"'...) failed with return value: "+std::to_string(status));
	}

	int child_status;
	waitpid(pid, &child_status, 0);
	if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0)
	{
		close(pid_pipe_fd[0]);
		//read any error message from stderr
		char err_msg[256] = {'\0'};
		for(size_t i=0; i<sizeof(err_msg)-1; i++)
			if(read(err_pipe_fd[0], &err_msg[i], 1) != 1 || err_msg[i]=='\0') break;
		close(err_pipe_fd[0]);
		throw std::runtime_error("spawn_detached process failed with message: "+std::string(err_msg));
	}
	close(err_pipe_fd[0]);

	char pid_str[33] = {'\0'};
	for(size_t i=0; i<sizeof(pid_str)-1; i++)
		if(read(pid_pipe_fd[0], &pid_str[i], 1) != 1 || pid_str[i]=='\0') break;
	close(pid_pipe_fd[0]);

	try
	{
		pid = std::stoi(pid_str);
	}
	catch(const std::exception& e)
	{
		throw std::runtime_error("Failed to read detached PID from pipe: "+std::string(e.what()));
	}

	return pid;
}

inline spawn_attached_result spawn_attached(const std::string& cmd, const std::vector<std::string>& args = {})
{
	pid_t pid;
	posix_spawnattr_t attr;
	posix_spawn_file_actions_t actions;
	spawn_init(attr,actions);

	// Create pipes for stdin, stdout, stderr
	int stdin_pipe[2]; // [0] = read, [1] = write
	int stdout_pipe[2];
	int stderr_pipe[2];

	if (pipe(stdin_pipe) == -1)
		throw std::runtime_error("stdin pipe() failed");
	else if (pipe(stdout_pipe) == -1)
	{
		close(stdin_pipe[0]);close(stdin_pipe[1]);
		throw std::runtime_error("stdout pipe() failed");
	}
	else if (pipe(stderr_pipe) == -1)
	{
		close(stdin_pipe[0]);close(stdin_pipe[1]);
		close(stdout_pipe[0]);close(stdout_pipe[1]);
		throw std::runtime_error("stderr pipe() failed");
	}

	// Child reads from stdin_pipe[0], writes to stdout_pipe[1] and stderr_pipe[1]
	// Parent writes to stdin_pipe[1], reads from stdout_pipe[0] and stderr_pipe[0]
	posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, stderr_pipe[1], STDERR_FILENO);
	// Close all other fds except stdio
	add_actions_close_all_fds(actions);

	std::vector<char *> argv;
	argv.push_back(const_cast<char*>(cmd.c_str()));
	for (const auto &arg : args)
		argv.push_back(const_cast<char*>(arg.c_str()));
	argv.push_back(nullptr);

	int status = posix_spawnp(&pid, cmd.c_str(), &actions, &attr, argv.data(), environ);

	// Post-spawn cleanup
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);
	// Close child's ends of pipes in parent
	close(stdin_pipe[0]);
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	if (status != 0)
	{
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stderr_pipe[0]);
		throw std::runtime_error("posix_spawn(...'"+cmd+"'...) failed with return value: " + std::to_string(status));
	}

	// Convert fds to FILE*
	FILE* stdin_file = fdopen(stdin_pipe[1], "w");
	FILE* stdout_file = fdopen(stdout_pipe[0], "r");
	FILE* stderr_file = fdopen(stderr_pipe[0], "r");

	if (!stdin_file || !stdout_file || !stderr_file)
	{
		// Clean up on failure
		if (stdin_file) fclose(stdin_file);else close(stdin_pipe[1]);
		if (stdout_file) fclose(stdout_file);else close(stdout_pipe[0]);
		if (stderr_file) fclose(stderr_file);else close(stderr_pipe[0]);
		throw std::runtime_error("fdopen() failed");
	}

	return spawn_attached_result{(int)pid, stdin_file, stdout_file, stderr_file};
}

inline std::pair<bool,int> spawn_wait(int pid, bool nohang)
{
	int status;
	int options = 0;
	if (nohang)
		options |= WNOHANG;

	pid_t result = waitpid(pid, &status, options);
	if (result == -1)
		throw std::runtime_error("waitpid() failed for PID " + std::to_string(pid));

	if (result != 0)
		return { true, status }
	;

	return { false, 0 };
}

inline void spawn_kill(int pid, int sig)
{
	if (kill(pid, sig) != 0)
		throw std::runtime_error("kill() failed for PID " + std::to_string(pid) + " with signal " + std::to_string(sig));
}

#endif


inline std::string GetLibFileName(const std::string& LibName)
{
	return DYNLIBPRE + LibName + DYNLIBEXT;
}

// Convert std::tm to time_t, using UTC or local time as specified.
// If UTC is true, uses timegm (if available), otherwise uses mktime.
inline time_t time_t_from_tm_tz(const std::tm& tm, const bool UTC)
{
	#if defined(_WIN32) || defined(WIN32) || defined(__WIN32)
	// Windows: _mkgmtime for UTC, mktime for local time
	std::tm tm_copy = tm; // mktime/_mkgmtime may modify struct
	if(UTC)
		return _mkgmtime(&tm_copy);
	else
		return mktime(&tm_copy);
	#else
	// POSIX: timegm for UTC, mktime for local time
	std::tm tm_copy = tm;
	if(UTC)
		return timegm(&tm_copy);
	else
		return mktime(&tm_copy);
	#endif
}

#endif //ODC_PLATFORM_H_
