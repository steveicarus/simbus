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

struct bus_state {
      int fd;
      unsigned port;
      bool running_flag;
};
map <string, struct bus_state> bus_map;

typedef map<string, bus_state>::iterator bus_map_idx_t;

struct client_state {
      string bus;
      char buffer[4096];
      size_t buffer_fill;
};

map<int,struct client_state> client_map;

typedef map<int, struct client_state>::iterator client_map_idx_t;

void service_init(void)
{
      int rc;

      for (bus_map_idx_t cur = bus_map.begin() ; cur != bus_map.end(); cur++) {

	      // Create the server socket..
	    cur->second.fd = socket(PF_INET, SOCK_STREAM, 0);
	    assert(cur->second.fd >= 0);

	      // Bind the service port address to the socket.
	    struct sockaddr_in server_socket;
	    memset(&server_socket, 0, sizeof server_socket);
	    server_socket.sin_family = AF_INET;
	    server_socket.sin_port = htons(cur->second.port);
	    server_socket.sin_addr.s_addr = INADDR_ANY;

	    rc = bind(cur->second.fd,
		      (const sockaddr*)&server_socket,
		      sizeof(server_socket));
	    assert(rc >= 0);

	      // Put the socket into listen mode.
	    rc = listen(cur->second.fd, 2);
	    assert(rc >= 0);
      }
}

/*
 * This function is called when the select loop finds that the service
 * socket is ready. The only thing that can happen on that fd is a
 * client is attempting a connect. Accept the connection.
 */
static void listen_ready(bus_map_idx_t&cur)
{
      struct sockaddr remote_addr;
      socklen_t remote_addr_len;

      int use_fd = accept(cur->second.fd, &remote_addr, &remote_addr_len);
      assert(use_fd >= 0);

      struct client_state tmp;
      tmp.bus = cur->first;
      tmp.buffer_fill = 0;

      client_map[use_fd] = tmp;
}

/*
 * A client connection is ready. Read the data from the connection and
 * process it.
 */
static void client_ready(client_map_idx_t&client)
{
      size_t trans = sizeof client->second.buffer - client->second.buffer_fill - 1;
      assert(trans > 0);

      int rc = read(client->first, client->second.buffer + client->second.buffer_fill, trans);
      assert(rc >= 0);
      if (rc == 0)
	    return;

      *(client->second.buffer+client->second.buffer_fill+rc) = 0;
}


void service_run(void)
{
      int rc;

      while (true) {
	    int nfds = 0;
	    fd_set rfds;
	    FD_ZERO(&rfds);

	      // Add the server ports to the fd list.
	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end(); idx++) {
		  int fd = idx->second.fd;
		  FD_SET(fd, &rfds);
		  if (fd >= nfds)
			nfds = fd + 1;
	    }

	      // Add the client ports to the fd list.
	    for (client_map_idx_t idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  int fd = idx->first;
		  FD_SET(fd, &rfds);
		  if (fd >= nfds)
			nfds = fd + 1;
	    }

	    rc = select(nfds, &rfds, 0, 0, 0);
	    if (rc == 0)
		  continue;

	    assert(rc >= 0);

	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end(); idx++) {
		  int fd = idx->second.fd;
		  if (FD_ISSET(fd, &rfds))
			listen_ready(idx);
	    }

	    for (client_map_idx_t idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  int fd = idx->first;
		  if (FD_ISSET(fd, &rfds))
			client_ready(idx);
	    }
      }
}

/*
 * $Log: $
 */

