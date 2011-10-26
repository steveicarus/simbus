/*
 * Copyright (c) 2008 Stephen Williams (steve@icarus.com)
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

# include  "simbus_priv.h"
# include  <unistd.h>
# include  <sys/types.h>
# include  <sys/socket.h>
# include  <sys/un.h>
# include  <netdb.h>
# include  <stdlib.h>
# include  <string.h>
# include  <limits.h>
# include  <stdio.h>
# include  <errno.h>
# include  <assert.h>

char __bitval_to_char(bus_bitval_t val)
{
      return "01xz"[val];
}

bus_bitval_t __char_to_bitval(char val)
{
      switch (val) {
	  case '0': return BIT_0;
	  case '1': return BIT_1;
	  case 'z': return BIT_Z;
	  default:  return BIT_X;
      }
}

static int tcp_socket(const char*addr)
{
      char*host_name = 0;
      char*host_port = 0;

	/* Parse the server string to a host name and port. If the
	   host name is missing, then assume the rest is the port and
	   use "localhost" as the server. */
      char*cp;
      if ( (cp = strchr(addr, ':')) != 0 ) {
	    host_name = malloc(cp - addr + 1);
	    strncpy(host_name, addr, cp-addr);
	    host_name[cp-addr] - 0;

	    host_port = strdup(cp+1);
      } else {
	    host_name = strdup("localhost");
	    host_port = strdup(addr);
      }

	/* Look up the host name and port to get the host/port
	   address. Try to connect to the service. */
      struct addrinfo hints, *res;
      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      int rc = getaddrinfo(host_name, host_port, &hints, &res);
      assert(rc == 0);

	/* Try to connect to the server. */
      int server_fd = -1;
      struct addrinfo *rp;
      for (rp = res ; rp != 0 ; rp = rp->ai_next) {
	      /* Make the right kind of socket. */
	    server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	    if (server_fd < 0)
		  continue;

	      /* Try to connect. */
	    if (connect(server_fd, rp->ai_addr, rp->ai_addrlen) < 0) {
		  close(server_fd);
		  server_fd = -1;
		  continue;
	    }

	    break;
      }

      freeaddrinfo(res);
      free(host_name);
      free(host_port);
      return server_fd;
}

static int pipe_socket(const char*path)
{
      int fd = socket(PF_UNIX, SOCK_STREAM, 0);
      assert(fd >= 0);

      struct sockaddr_un addr;
      memset(&addr, 0, sizeof addr);

      assert(strlen(path) < sizeof addr.sun_path);
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, path);

      int rc = connect(fd, (const struct sockaddr*)&addr, sizeof addr);
      assert(rc >= 0);

      return fd;
}

int __simbus_server_socket(const char*addr)
{
      int server_fd = -1;

      if (strncmp(addr, "tcp:", 4) == 0) {
	    server_fd = tcp_socket(addr+4);

      } else if (strncmp(addr, "pipe:", 5) == 0) {
	    server_fd = pipe_socket(addr+5);

      } else {
	    server_fd = tcp_socket(addr);
      }

      return server_fd;
}


int __simbus_server_hello(int server_fd, const char*name, unsigned*ident)
{
      char buf[4096];

      	/* Send HELLO message to the server. */
      snprintf(buf, sizeof buf, "HELLO %s\n", name);

      int rc = write(server_fd, buf, strlen(buf));
      assert(rc == strlen(buf));

	/* Read response from server. */
      rc = read(server_fd, buf, sizeof buf);
      assert(rc > 0);
      assert(strchr(buf, '\n'));

	/* If the server NAKs me, then give up. */
      if (strcmp(buf, "NAK\n") == 0) {
	    close(server_fd);
	    return -1;
      }

      *ident = 0;
      if (strncmp(buf, "YOU-ARE ", 8) == 0) {
	    sscanf(buf, "YOU-ARE %u", ident);
      } else {
	    close(server_fd);
	    return -1;
      }

      return 0;
}

int __simbus_server_finish(int server_fd)
{
      int rc;

	/* Send the FINISH command */
      rc = write(server_fd, "FINISH\n", 7);
      assert(rc >= 0);
      assert(rc == 7);

	/* Now read the response, which should be a FINISH command */
      char buf[128];
      rc = read(server_fd, buf, sizeof(buf)-1);
      assert( rc >= 0 );

	/* The response from the server should be FINISH. */
      buf[rc] = 0;
      assert(strcmp(buf,"FINISH\n") == 0);

      return 0;
}

int __simbus_server_send_recv(int server_fd, char*buf, size_t buf_size,
			      int max_argc, char*argv[], FILE*debug)
{
      int rc;
      size_t buf_len = strlen(buf);

	/* Send the READY command */
      rc = write(server_fd, buf, buf_len);
      if (rc < 0) {
	    fprintf(stderr, "__simbus_server_send_recv: rc = %d, errno=%d\n", rc, errno);
	    if (debug)
		  fprintf(debug, "__simbus_server_send_recv: rc = %d, errno=%d\n", rc, errno);
	    return 0;
      }
      assert(rc == buf_len);

	/* Now read the response, which should be an UNTIL command */
      rc = read(server_fd, buf, buf_size-1);
      assert( rc >= 0 );

	/* Detect an EOF from the connection. */
      if (rc == 0) {
	    return 0;
      }

      buf[rc] = 0;

      char*cp = strchr(buf, '\n');
      assert(cp && *cp=='\n');

      *cp = 0;
      if (debug) {
	    fprintf(debug, "RECV %s\n", buf);
      }

	/* Chop the response int tokens. */
      int cur_argc = 0;

      cp = buf + strspn(buf, " ");
      while (*cp && cur_argc < max_argc) {
	    argv[cur_argc++] = cp;
	    cp += strcspn(cp, " ");
	    *cp++ = 0;
	    cp += strspn(cp, " ");
      }
      argv[cur_argc] = 0;

      return cur_argc;
}

void __parse_time_token(const char*token, uint64_t*time_mant, int*time_exp)
{
      char*cp;

      if (sizeof(uint64_t) <= sizeof(unsigned long)) {
	    *time_mant = strtoul(token, &cp, 10);
      } else if (sizeof(uint64_t) <= sizeof(unsigned long long)) {
	    *time_mant = strtoull(token, &cp, 10);
      } else {
	    *time_mant = strtoull(token, &cp, 10);
#if defined(ULLONG_MAX)
	    assert(*time_mant < ULLONG_MAX);
#endif
      }

      assert(*cp == 'e');
      cp += 1;
      *time_exp = strtol(cp, 0, 10);
}
