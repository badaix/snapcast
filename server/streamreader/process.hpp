#ifndef TINY_PROCESS_LIBRARY_HPP_
#define TINY_PROCESS_LIBRARY_HPP_

#include <string>
#include <mutex>
#include <sys/wait.h>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>

// Forked from: https://github.com/eidheim/tiny-process-library
// Copyright (c) 2015-2016 Ole Christian Eidheim
// Thanks, Christian :-)

///Create a new process given command and run path.
///Thus, at the moment, if read_stdout==nullptr, read_stderr==nullptr and open_stdin==false,
///the stdout, stderr and stdin are sent to the parent process instead.
///Compile with -DMSYS_PROCESS_USE_SH to run command using "sh -c [command]" on Windows as well.
class Process {

public:
	typedef int fd_type;

	Process(const std::string &command, const std::string &path = "") : closed(true)
	{
		open(command, path);
	}

	~Process() 
	{
		close_fds();
	}

	///Get the process id of the started process.
	pid_t getPid() 
	{
		return pid;
	}

	///Write to stdin. Convenience function using write(const char *, size_t).
	bool write(const std::string &data) 
	{
		return write(data.c_str(), data.size());
	}

	///Wait until process is finished, and return exit status.
	int get_exit_status() 
	{
		if (pid <= 0)
			return -1;

		int exit_status;
		waitpid(pid, &exit_status, 0);
		{
			std::lock_guard<std::mutex> lock(close_mutex);
			closed=true;
		}
		close_fds();

		if (exit_status >= 256)
			exit_status = exit_status>>8;

		return exit_status;
	}

	///Write to stdin.
	bool write(const char *bytes, size_t n) 
	{
		std::lock_guard<std::mutex> lock(stdin_mutex);
		if (::write(stdin_fd, bytes, n)>=0) 
			return true;
		else
			return false;
	}

	///Close stdin. If the process takes parameters from stdin, use this to notify that all parameters have been sent.
	void close_stdin() 
	{
		std::lock_guard<std::mutex> lock(stdin_mutex);
		if (pid > 0)
			close(stdin_fd);
	}

	///Kill the process.
	void kill(bool force=false) 
	{
		std::lock_guard<std::mutex> lock(close_mutex);
		if (pid > 0 && !closed) 
		{
			if(force)
				::kill(-pid, SIGTERM);
			else
				::kill(-pid, SIGINT);
		}
	}

	///Kill a given process id. Use kill(bool force) instead if possible.
	static void kill(pid_t id, bool force=false) 
	{
		if (id <= 0)
			return;
		if (force)
			::kill(-id, SIGTERM);
		else
			::kill(-id, SIGINT);
	}

	fd_type getStdout()
	{
		return stdout_fd;
	}

	fd_type getStderr()
	{
		return stderr_fd;
	}

	fd_type getStdin()
	{
		return stdin_fd;
	}


private:
	pid_t pid;
	bool closed;
	std::mutex close_mutex;
	std::mutex stdin_mutex;

	fd_type stdout_fd, stderr_fd, stdin_fd;

	void closePipe(int pipefd[2])
	{
		close(pipefd[0]);
		close(pipefd[1]);
	}

	pid_t open(const std::string &command, const std::string &path) 
	{
		int stdin_p[2], stdout_p[2], stderr_p[2];

		if (pipe(stdin_p) != 0)
			return -1;

		if (pipe(stdout_p) != 0) 
		{
			closePipe(stdin_p);
			return -1;
		}

		if (pipe(stderr_p) != 0) 
		{
			closePipe(stdin_p);
			closePipe(stdout_p);
			return -1;
		}
		
		pid = fork();

		if (pid < 0) 
		{
			closePipe(stdin_p);
			closePipe(stdout_p);
			closePipe(stderr_p);
			return pid;
		}
		else if (pid == 0) 
		{
			dup2(stdin_p[0], 0);
			dup2(stdout_p[1], 1);
			dup2(stderr_p[1], 2);

			closePipe(stdin_p);
			closePipe(stdout_p);
			closePipe(stderr_p);

			//Based on http://stackoverflow.com/a/899533/3808293
			int fd_max = sysconf(_SC_OPEN_MAX);
			for (int fd=3; fd<fd_max; fd++)
				close(fd);

			setpgid(0, 0);
			
			if (!path.empty()) 
			{
				auto path_escaped = path;
				size_t pos=0;
				//Based on https://www.reddit.com/r/cpp/comments/3vpjqg/a_new_platform_independent_process_library_for_c11/cxsxyb7
				while ((pos = path_escaped.find('\'', pos)) != std::string::npos) 
				{
					path_escaped.replace(pos, 1, "'\\''");
					pos += 4;
				}
				execl("/bin/sh", "sh", "-c", ("cd '" + path_escaped + "' && " + command).c_str(), NULL);
			}
			else
				execl("/bin/sh", "sh", "-c", command.c_str(), NULL);
			
			_exit(EXIT_FAILURE);
		}

		close(stdin_p[0]);
		close(stdout_p[1]);
		close(stderr_p[1]);
		
		stdin_fd = stdin_p[1];
		stdout_fd = stdout_p[0];
		stderr_fd = stderr_p[0];

		closed = false;
		return pid;
	}


	void close_fds() 
	{
		close_stdin();
		if (pid > 0)
		{
			close(stdout_fd);
			close(stderr_fd);
		}
	}

};

#endif  // TINY_PROCESS_LIBRARY_HPP_

