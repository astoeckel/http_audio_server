/*
 *  Cypress -- C++ Neural Associative Memory Simulator
 *  Copyright (C) 2016  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <ext/stdio_filebuf.h>
#include <limits.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <http_audio_server/process.hpp>

namespace http_audio_server {

/*
 * Class ProcessImpl
 */

class ProcessImpl {
private:
	using Filebuf = __gnu_cxx::stdio_filebuf<std::ifstream::char_type>;

	pid_t m_pid;

	bool m_do_redirect;

	int m_child_stdout_pipe[2];
	int m_child_stderr_pipe[2];
	int m_child_stdin_pipe[2];

	std::unique_ptr<Filebuf> m_child_stdout_filebuf;
	std::unique_ptr<Filebuf> m_child_stderr_filebuf;
	std::unique_ptr<Filebuf> m_child_stdin_filebuf;

	std::unique_ptr<std::istream> m_child_stdout;
	std::unique_ptr<std::istream> m_child_stderr;
	std::unique_ptr<std::ostream> m_child_stdin;

	int m_exitcode = 0;

	bool update_status(bool block)
	{
		// If we already know that the child process no longer exists, return
		if (m_pid == 0) {
			return false;
		}

		// Otherweise query the child status using waitpid
		int status;
		pid_t res = waitpid(m_pid, &status, block ? 0 : WNOHANG);
		if (res == -1) {
			throw std::runtime_error("Error while querying child state!");
		}
		else if (res == 0) {
			return true;  // No status change, the child process is still alive!
		}
		else {
			if (WIFEXITED(status)) {
				m_exitcode = WEXITSTATUS(status);
				m_pid = 0;
				return false;
			}
			else if (WIFSIGNALED(status)) {
				m_exitcode = -WTERMSIG(status);
				m_pid = 0;
				return false;
			}
			// This was another state change
			return true;
		}
	}

public:
	ProcessImpl(const std::string &cmd, const std::vector<std::string> &args,
	            bool do_redirect = true)
	    : m_do_redirect(do_redirect)
	{
		// Setup the stdin, stdout, stderr pipes
		if (m_do_redirect && (pipe2(m_child_stdout_pipe, O_CLOEXEC) != 0 ||
		                      pipe2(m_child_stderr_pipe, O_CLOEXEC) != 0 ||
		                      pipe2(m_child_stdin_pipe, O_CLOEXEC) != 0)) {
			throw std::runtime_error(
			    "Cannot launch subprocess, error while creating communication "
			    "pipes");
		}

		m_pid = fork();  // Split the current process into two
		if (m_pid == 0) {
			// This is the child process -- redirect the child I/O to the pipe
			if (m_do_redirect) {
				dup2(m_child_stdout_pipe[1], STDOUT_FILENO);
				dup2(m_child_stderr_pipe[1], STDERR_FILENO);
				dup2(m_child_stdin_pipe[0], STDIN_FILENO);
			}

			// Assemble the arguments array
			std::vector<char const *> argv(args.size() + 2);
			argv[0] = cmd.c_str();
			for (size_t i = 0; i < args.size(); i++) {
				argv[i + 1] = args[i].c_str();
			}
			argv[args.size() + 1] = nullptr;

			// Launch the subprocessm, on success, there is no return from
			// execvp
			execvp(cmd.c_str(), (char *const *)&argv[0]);
			std::cerr << "Command not found: " << cmd << std::endl;
			exit(1);  // Panic!
		}
		else if (m_pid > 0) {
			// This is the parent process -- close the corresponding ends of
			// the pipe
			if (m_do_redirect) {
				close(m_child_stdout_pipe[1]);
				close(m_child_stderr_pipe[1]);
				close(m_child_stdin_pipe[0]);
			}
			else {
				m_child_stdout_pipe[0] = dup(STDOUT_FILENO);
				m_child_stderr_pipe[0] = dup(STDERR_FILENO);
				m_child_stdin_pipe[1] = dup(STDIN_FILENO);
			}

			// Create the file buffer object
			m_child_stdout_filebuf =
			    std::make_unique<Filebuf>(m_child_stdout_pipe[0], std::ios::in);
			m_child_stdout_filebuf->pubsetbuf(nullptr, 0);
			m_child_stderr_filebuf =
			    std::make_unique<Filebuf>(m_child_stderr_pipe[0], std::ios::in);
			m_child_stderr_filebuf->pubsetbuf(nullptr, 0);
			m_child_stdin_filebuf =
			    std::make_unique<Filebuf>(m_child_stdin_pipe[1], std::ios::out);

			// Create the stream objects
			m_child_stdout =
			    std::make_unique<std::istream>(m_child_stdout_filebuf.get());
			m_child_stderr =
			    std::make_unique<std::istream>(m_child_stderr_filebuf.get());
			m_child_stdin =
			    std::make_unique<std::ostream>(m_child_stdin_filebuf.get());
		}
		else {
			throw std::runtime_error(
			    "Cannot launch subprocess, error while forking!");
		}
	}

	~ProcessImpl() { wait(); }
	std::istream &child_stdout() { return *m_child_stdout; }
	std::istream &child_stderr() { return *m_child_stderr; }
	std::ostream &child_stdin() { return *m_child_stdin; }
	void close_child_stdin()
	{
		m_child_stdin->flush();
		m_child_stdin = nullptr;
		m_child_stdin_filebuf = nullptr;
	}

	bool running() { return update_status(false); }
	int exitcode() { return m_exitcode; }
	int wait()
	{
		update_status(true);
		return m_exitcode;
	}

	bool signal(int signal) { return kill(m_pid, signal) == 0; }
};

/*
 * Class Process
 */

Process::Process(const std::string &cmd, const std::vector<std::string> &args,
                 bool do_redirect)
    : impl(std::make_unique<ProcessImpl>(cmd, args, do_redirect))
{
}

Process::~Process()
{
	// Just needed for the unique_ptr destructor to work
}

std::istream &Process::child_stdout() { return impl->child_stdout(); }
std::istream &Process::child_stderr() { return impl->child_stderr(); }
std::ostream &Process::child_stdin() { return impl->child_stdin(); }
void Process::close_child_stdin() { impl->close_child_stdin(); }
bool Process::running() { return impl->running(); }
int Process::exitcode() { return impl->exitcode(); }
int Process::wait() { return impl->wait(); }
bool Process::signal(int signal) { return impl->signal(signal); }
void Process::generic_writer(Process &proc, std::istream &input)
{
	generic_pipe(input, proc.child_stdin());
	proc.close_child_stdin();
}

void Process::generic_pipe(std::istream &source, std::ostream &output)
{
	constexpr size_t BUF_SIZE = 4096;
	char buf[BUF_SIZE];
	size_t buf_ptr = 0;

	while (source.good() && output.good()) {
		char c;
		source.read(&c, 1);
		if (source.gcount() == 1) {
			buf[buf_ptr++] = c;
			if (buf_ptr == BUF_SIZE || c == '\n' || c == '\r') {
				output.write(buf, buf_ptr);
				output.flush();
				buf_ptr = 0;
			}
		}
	}
	if (buf_ptr > 0 && output.good()) {
		output.write(buf, buf_ptr);
		output.flush();
	}
}
int Process::exec(const std::string &cmd, const std::vector<std::string> &args,
                  std::istream &cin, std::ostream &cout, std::ostream &cerr)
{
	Process proc(cmd, args);

	std::thread t1(generic_writer, std::ref(proc), std::ref(cin));
	std::thread t2(generic_pipe, std::ref(proc.child_stdout()), std::ref(cout));
	std::thread t3(generic_pipe, std::ref(proc.child_stderr()), std::ref(cerr));

	int res = proc.wait();

	// Wait for all threads to complete
	t1.join();
	t2.join();
	t3.join();

	return res;
}

int Process::exec(const std::string &cmd, const std::vector<std::string> &args,
                  std::ostream &cout, std::ostream &cerr,
                  const std::string &input)
{
	std::stringstream ss_in;
	ss_in << input;
	return exec(cmd, args, ss_in, cout, cerr);
}

int Process::exec_no_redirect(const std::string &cmd,
                              const std::vector<std::string> &args)
{
	Process proc(cmd, args, false);
	return proc.wait();
}

std::tuple<int, std::string, std::string> Process::exec(
    const std::string &cmd, const std::vector<std::string> &args,
    const std::string &input)
{
	// Concurrently write the input data into the child process and read the
	// output back
	std::stringstream ss_out, ss_err;

	int res = exec(cmd, args, ss_out, ss_err, input);

	// Wait for the child process to exit, return the exit code and the stream
	// from standard out
	return std::make_tuple(res, ss_out.str(), ss_err.str());
}
}
