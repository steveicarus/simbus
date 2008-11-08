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
# include  "client.h"
# include  "protocol.h"
# include  <assert.h>

using namespace std;

/*
 * A client is mapped using its file descriptor as the key. The client
 * contains the "bus", which is the number of the bus that it belongs
 * to, and can be used to look up the port.
 */
map<int,client_state_t> client_map;
typedef map<int,client_state_t>::iterator client_map_idx_t;

map <unsigned, struct bus_state> bus_map;

void service_add_bus(unsigned port, const std::string&name,
		     const std::string&bus_protocol_name,
		     const bus_device_map_t&dev)
{
      bus_state&tmp = bus_map[port];
      tmp.name = name;
      tmp.fd = -1;
      tmp.need_initialization = true;
      tmp.device_map = dev;

      if (bus_protocol_name == "pci") {
	    tmp.proto = new PciProtocol(tmp);
      } else {
	    tmp.proto = 0;
      }
}

/*
 * This function is called once to start the service running. It binds
 * to the network sockets for all the busses.
 */
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

      client_state_t tmp;
      tmp.set_bus (cur->first);

      client_map[use_fd] = tmp;
}

/*
 * A client connection is ready. Read the data from the connection and
 * process it.
 */
static void client_ready(client_map_idx_t&client)
{
      client->second.read_from_socket(client->first);
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

	      // Bus sockets that become ready...
	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end(); idx++) {
		  int fd = idx->second.fd;
		  if (FD_ISSET(fd, &rfds))
			listen_ready(idx);
	    }

	      // Client sockets that become ready...
	    for (client_map_idx_t idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  int fd = idx->first;
		  if (FD_ISSET(fd, &rfds))
			client_ready(idx);
	    }

	      // Now check the busses to see if they can have their
	      // protocol run.
	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end() ; idx ++) {

		  bool flag = true;
		  for (bus_device_map_t::iterator dev = idx->second.device_map.begin()
			     ; dev != idx->second.device_map.end() ;  dev ++) {
			if (dev->second.ready_flag == false) {
			      flag = false;
			      break;
			}
		  }

		  if (flag == true) {
			  // If this is the first time all the devices
			  // are ready, then the bus is finally
			  // assembled, and needs final initialization.
			if (idx->second.need_initialization) {
			      cout << idx->second.name << ": "
				   << "Bus assembly complete." << endl;
			      idx->second.proto->run_init();
			      idx->second.need_initialization = false;
			}

			  // Run the first bus state.
			idx->second.proto->bus_ready();
		  }
	    }

      }
}
