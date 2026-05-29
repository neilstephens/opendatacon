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
 * SpawnDetached.c
 *
 *  Created on: 6/12/2025
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <stdio.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <child command line as single arg>\n", argv[0]);
		return 1;
	}

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	BOOL success = CreateProcessA(
		NULL, argv[1], NULL, NULL, FALSE,
		DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
		NULL, NULL, &si, &pi
		);

	if(!success)
	{
		fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
		return 1;
	}
	DWORD pid = pi.dwProcessId;
	fprintf(stdout, "%lu\n", pid);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}

#else //POSIX

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/wait.h>

//close any inherited file descriptors except stdio
void close_all_fds()
{
	#ifdef HAVE_CLOSEFROM
	closefrom(3);
	return;
	#endif
	#ifdef __linux__
	//fallback to reading /proc
	DIR *dir = opendir("/proc/self/fd");
	if (dir)
	{
		int dir_fd = dirfd(dir);
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)
		{
			int fd = atoi(entry->d_name);
			if(fd > 2 && fd != dir_fd) close(fd);
		}
		closedir(dir);
		return;
	}
	#endif
	// fallback to getrlimit
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < 200000)
	{
		for (int fd = 3; fd < (int)rl.rlim_cur; fd++)
			close(fd);
		return;
	}
	// Last resort: assume 200,000
	for (int fd = 3; fd < 200000; fd++)
		close(fd);
}

//spawn a detached process using double 'fork' pattern
int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s <child command> [<child arg> ...]\n", argv[0]);
		return 1;
	}

	pid_t pid1 = fork();
	if(pid1 < 0)
	{
		fprintf(stderr, "first fork() failed.\n");
		return 1;
	}

	if(pid1 == 0) /*intermediate process*/
	{
		/*close all descriptors except stdio*/
		close_all_fds();
		if(setsid() < 0)
		{
			fprintf(stderr, "setsid() failed.\n");
			_Exit(1);
		}

		pid_t pid = fork();
		if(pid < 0)
		{
			fprintf(stderr, "second fork() failed.\n");
			_Exit(1);
		}

		if(pid == 0) /*final child process*/
		{
			/*redirect stdin/out/err*/
			int fd_null = open("/dev/null", O_RDWR);
			if (fd_null >= 0)
			{
				dup2(fd_null, 0);
				dup2(fd_null, 1);
				dup2(fd_null, 2);
				if(fd_null > 2) close(fd_null);
			}
			execvp(argv[1], &argv[1]);
			/*exec only returns on fail*/
			_Exit(1);
		}
		fprintf(stdout,"%d\n",pid);
		fflush(stdout);
		_Exit(0); /*exit intermediate success*/
	}

	waitpid(pid1, NULL, 0);
	return 0;
}

#endif
