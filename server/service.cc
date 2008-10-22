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

# include  "priv.h"
# include  <assert.h>

using namespace std;

/*
 * The bus_state describes a bus. The fd is the posix file-descriptor
 * for the server port, and the name is the configured bus name. The
 * TCP port is the map key.
 */
struct bus_state {
	// Human-readable name for the bus.
      string name;
	// posix fd for the bus.
      int fd;
	// List uf unbound devices
      bus_device_map_t device_map;
};
map <unsigned, struct bus_state> bus_map;
typedef map<unsigned, bus_state>::iterator bus_map_idx_t;

/*
 * A client is mapped using its file descriptor as the key. The client
 * contains the "bus", which is the number of the bus that it belongs
 * to, and can be used to look up the port.
 */
struct client_state {
	// Key for the bus that client belongs to
      unsigned bus;
	// Device name and ident.
      string dev_name;
      unsigned dev_ident;
	// Keep an input buffer of data read from the connection.
      char buffer[4096];
      size_t buffer_fill;
};

map<int,struct client_state> client_map;
typedef map<int, struct client_state>::iterator client_map_idx_t;

void process_client_command(client_map_idx_t&client, int argc, char*argv[]);

void service_add_bus(unsigned port, const std::string&name,
		     const bus_device_map_t&dev)
{
      bus_state tmp;
      tmp.name = name;
      tmp.fd = -1;
      tmp.device_map = dev;

      bus_map[port] = tmp;
}

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
	      // The port is the map key.
	    server_socket.sin_port = htons(cur->first);
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
      tmp.dev_name = "";
      tmp.dev_ident = 0;
      tmp.buffer_fill = 0;

      client_map[use_fd] = tmp;
}

const char white_space[] = " \r";

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

      client->second.buffer_fill += rc;
      *(client->second.buffer+client->second.buffer_fill) = 0;

      if (char*eol = strchr(client->second.buffer, '\n')) {
	      // Remote the new-line.
	    *eol++ = 0;
	    int argc = 0;
	    char*argv[2048];

	    char*cp = client->second.buffer;
	    while (*cp != 0) {
		  argv[argc++] = cp;
		  cp += strcspn(cp, white_space);
		  if (*cp) {
			*cp++ = 0;
			cp += strspn(cp, white_space);
		  }
	    }
	    argv[argc] = 0;

	      // Process the client command.
	    process_client_command(client, argc, argv);

	      // Remove the command line from the input buffer
	    client->second.buffer_fill -= eol - client->second.buffer;
	    memmove(client->second.buffer, eol, client->second.buffer_fill);
      }
}

void process_client_command(client_map_idx_t&client, int argc, char*argv[])
{
      char outbuf[4096];

      if (client->second.dev_name == "") {
	    if (strcmp(argv[0], "HELLO") != 0) {
		  fprintf(stderr, "Expecting HELLO from client, got %s\n", argv[0]);
		  return;
	    }

	    assert(argc > 1);
	    string use_name = argv[1];

	    bus_map_idx_t bus_info = bus_map.find(client->second.bus);
	    assert(bus_info != bus_map.end());

	    bus_device_map_t::iterator cur = bus_info->second.device_map.find(use_name);
	    if (cur == bus_info->second.device_map.end()) {
		  write(client->first, "NAK\n", 4);
		  cerr << "Device " << use_name
		       << " not found in bus " << bus_info->second.name
		       << endl;
		  return;
	    }

	    client->second.dev_name = use_name;
	    client->second.dev_ident = cur->second.ident;

	    snprintf(outbuf, sizeof outbuf, "YOU-ARE %u\n", client->second.dev_ident);
	    int rc = write(client->first, outbuf, strlen(outbuf));
	    assert(rc == strlen(outbuf));
	    return;
      }

      if (strcmp(argv[0],"HELLO") == 0) {
	    cerr << "Extra HELLO from " << client->second.dev_name << endl;
      } else {
	    cerr << "Unknown command " << argv[0]
		 << " from client " << client->second.dev_name << endl;
      }
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

