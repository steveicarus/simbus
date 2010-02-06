/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  "priv.h"

# include  <string>
# include  <map>
# include  <iostream>

# include  <sys/types.h>
# include  <sys/stat.h>
# include  <fcntl.h>
# include  <signal.h>

using namespace std;

struct process_info {
      string use_exec;
      string use_stdin;
      string use_stdout;
      string use_stderr;
      map<std::string,std::string>use_env;
};

static map<string,process_info> proc_table;

void process_add(const std::string&name,
		 const std::string&use_exec,
		 const std::string&use_stdin,
		 const std::string&use_stdout,
		 const std::string&use_stderr,
		 const std::map<std::string,std::string>&use_env)
{
      process_info&info = proc_table[name];

      info.use_exec = use_exec;
      info.use_stdin = use_stdin;
      info.use_stdout = use_stdout;
      info.use_stderr = use_stderr;
      info.use_env = use_env;
}

static void do_process_run(const string&name, const process_info&info)
{
      cerr << "Process: " << name << endl;
      cerr << "  exec: " << info.use_exec << endl;
      cerr << "  stdin : " << info.use_stdin << endl;
      cerr << "  stdout: " << info.use_stdout << endl;
      cerr << "  stderr: " << info.use_stderr << endl;

	// Open the stdin for the process. This must have been
	// specified, even if specified as /dev/null.
      int fd_stdin = open(info.use_stdin.c_str(), O_RDONLY, 0);
      if (fd_stdin < 0) {
	    perror(info.use_stdin.c_str());
	    return;
      }

	// If not otherwise specified, or explicitly given as "-",
	// then set this process to write to the server's stdout.
      int fd_stdout = 1;

      if (info.use_stdout != "" && info.use_stdout != "-") {
	    fd_stdout = open(info.use_stdout.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
	    if (fd_stdout < 0) {
		  perror(info.use_stdout.c_str());
		  return;
	    }
      }

      int fd_stderr = 1;
      if (info.use_stderr == "") {
	      // If not otherwise specified, set stderr to the
	      // server's stderr.
	    fd_stderr = 2;

      } else if (info.use_stderr == "-") {
	      // If explicitly set to "-", then stderr is a duplicate
	      // of its stdout.
	    fd_stderr = fd_stdout;
      } else {
	      // Otherwise, open stderr.
	    fd_stderr = open(info.use_stderr.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
	    if (fd_stderr < 0) {
		  perror(info.use_stderr.c_str());
		  return;
	    }
      }

	// Spawn the process for the new command.
      if (pid_t pid = fork()) {
	    cerr << "  pid: " << pid << endl;
	    if (fd_stdin > 2) close(fd_stdin);
	    if (fd_stdout > 2) close(fd_stdout);
	    if (fd_stderr > 2 && fd_stderr != fd_stdout) close(fd_stderr);
	    return;
      }

	// Here we are running in the child process. shuffle file
	// descriptors around and execute the actual command.
      if (fd_stdin != 0)  { close(0); dup2(fd_stdin, 0); }
      if (fd_stdout != 1) { close(1); dup2(fd_stdout, 1); }
      if (fd_stderr != 2) { close(2); dup2(fd_stderr, 2); }

      char*use_argv[4];
      use_argv[0] = strdup("/bin/sh");
      use_argv[1] = strdup("-c");
      use_argv[2] = strdup(info.use_exec.c_str());
      use_argv[3] = 0;
      int rc = execv("/bin/sh", use_argv);

	// The execv cannot fail. But if it does, print an error
	// message and exit.
      if (rc < 0) perror(use_argv[2]);
      exit(1);
}

void process_run(void)
{
	// Ignore children so that they don't become zombies.
      signal(SIGCHLD, SIG_IGN);
      for (map<string,process_info>::iterator idx = proc_table.begin()
		 ; idx != proc_table.end() ; idx ++) {
	    do_process_run(idx->first, idx->second);
      }
}
