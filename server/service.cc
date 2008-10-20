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
#ident "$Id:$"

# include  <sys/types.h>
# include  <sys/socket.h>
# include  <netinet/ip.h>
# include  <arpa/inet.h>
# include  <string.h>
# include  <unistd.h>

# include  <iostream>
# include  <map>

# include  <assert.h>

using namespace std;

static int service_fd;
static bool running_flag;

struct client_state {
      int fd;
      char buffer[4096];
      size_t buffer_fill;
};

map<int,struct client_state> client_map;

void service_init(unsigned server_port)
{
      int rc;

	// Create the server socket..
      service_fd = socket(PF_INET, SOCK_STREAM, 0);
      assert(service_fd >= 0);

	// Bind the service port address to the socket.
      struct sockaddr_in server_socket;
      memset(&server_socket, 0, sizeof server_socket);
      server_socket.sin_family = AF_INET;
      server_socket.sin_port = htons(server_port);
      server_socket.sin_addr.s_addr = INADDR_ANY;

      rc = bind(service_fd, (const sockaddr*)&server_socket, sizeof(server_socket));
      assert(rc >= 0);

	// Put the socket into listen mode.
      rc = listen(service_fd, 2);
      assert(rc >= 0);
}

/*
 * This function is called when the select loop finds that the service
 * socket is ready. The only thing that can happen on that fd is a
 * client is attempting a connect. Accept the connection.
 */
static void listen_ready(void)
{
      struct sockaddr remote_addr;
      socklen_t remote_addr_len;

      int rc = accept(service_fd, &remote_addr, &remote_addr_len);
      assert(rc >= 0);

      struct client_state tmp;
      tmp.fd = rc;
      tmp.buffer_fill = 0;

      client_map[tmp.fd] = tmp;
}

/*
 * A client connection is ready. Read the data from the connection and
 * process it.
 */
static void client_ready(struct client_state&client)
{
      size_t trans = sizeof client.buffer - client.buffer_fill - 1;
      assert(trans > 0);

      int rc = read(client.fd, client.buffer + client.buffer_fill, trans);
      assert(rc >= 0);
      if (rc == 0)
	    return;

      *(client.buffer+client.buffer_fill+rc) = 0;
}


void service_run(void)
{
      int rc;

      running_flag = true;
      while (running_flag) {
	    int nfds = 0;
	    fd_set rfds;
	    FD_ZERO(&rfds);

	      // Add the server port to the fd list.
	    FD_SET(service_fd, &rfds);
	    if (service_fd >= nfds)
		  nfds = service_fd + 1;

	    for (map<int,struct client_state>::iterator idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  FD_SET(idx->second.fd, &rfds);
		  if (idx->second.fd >= nfds)
			nfds = idx->second.fd + 1;
	    }

	    rc = select(nfds, &rfds, 0, 0, 0);
	    if (rc == 0)
		  continue;

	    assert(rc >= 0);

	    if (FD_ISSET(service_fd, &rfds))
		  listen_ready();

	    for (map<int,struct client_state>::iterator idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  if (FD_ISSET(idx->second.fd, &rfds))
			client_ready(idx->second);
	    }
      }
}

/*
 * $Log: $
 */

