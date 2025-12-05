#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

#include <opendatacon/Platform.h>

int spawn_detached(const char* cmd, char* argv[])
{
	try
	{
		spawn_detached(cmd);
	}
	catch(std::exception&)
	{
		return 1;
	}
	return 0;
}

#else //POSIX

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/wait.h>

inline void close_all_fds(int except_fd = -1)
{
	//redirect stdin/out/err if possible
	int fd_null = open("/dev/null", O_RDWR);
	if (fd_null >= 0)
	{
		dup2(fd_null, 0);
		dup2(fd_null, 1);
		dup2(fd_null, 2);
	}

	//close any inherrited file descriptors
	#ifdef HAVE_CLOSEHELPERS
	if (except_fd > 3) close_range(3,except_fd-1,0);
	closefrom((except_fd > 2) ? except_fd+1 : 3);
	return;
	#endif
	#ifdef __linux__
	//fallback to reading /proc
	DIR *dir = opendir("/proc/self/fd");
	if (dir)
	{
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)
		{
			int fd = atoi(entry->d_name);
			if(fd > 2 && fd != except_fd)
				close(fd);
		}
		closedir(dir);
		return;
	}
	#endif
	// fallback to getrlimit
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
	{
		for (int fd = 3; fd < (int)rl.rlim_max; fd++)
			if(fd != except_fd) close(fd);
		return;
	}
	// Last resort: assume 200,000
	for (int fd = 3; fd < 200000; fd++)
		if(fd != except_fd) close(fd);
}

//spawn a detached process using double fork pattern
//not thread safe - only executed from special invocation of main.
int spawn_detached(const char* cmd, char* argv[])
{
	// FD 3 is the pipe to the parent
	int pid_pipe_fd = 3;

	pid_t pid1 = fork();
	if(pid1 < 0)
	{
		close(pid_pipe_fd);
		_Exit(1);
	}

	if(pid1 == 0) //intermediate child process
	{
		//close all descriptors except the pid pipe write-end
		close_all_fds(pid_pipe_fd);
		//detach
		if(setsid() < 0)
		{
			close(pid_pipe_fd);
			_Exit(1);
		}

		pid_t pid2 = fork();
		if(pid2 < 0)
		{
			close(pid_pipe_fd);
			_Exit(1);
		}

		if(pid2 == 0) //final child proccess
		{
			int final_pid = getpid();
			char pid_str[32] = {'\0'};
			snprintf(pid_str, sizeof(pid_str), "%d\n", final_pid);
			write(pid_pipe_fd, pid_str, strlen(pid_str));
			close(pid_pipe_fd);
			execvp(cmd, argv);
			//exec only returns on fail
			_Exit(1);
		}

		close(pid_pipe_fd);
		_Exit(0);
	}

	int status; //get the status of the intermediate child
	if(waitpid(pid1,&status,0) < 0)
	{
		close(pid_pipe_fd);
		_Exit(1);
	}

	close(pid_pipe_fd); //close pid pipe write
	return status;
}

#endif
