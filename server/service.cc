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

# include  <sys/types.h>
# include  <sys/socket.h>
# include  <sys/un.h>
# include  <netinet/ip.h>
# include  <arpa/inet.h>
# include  <string.h>
# include  <unistd.h>

# include  <iostream>
# include  <map>

# include  "priv.h"
# include  "client.h"
# include  "protocol.h"
# include  "PciProtocol.h"
# include  "lxt2_write.h"
# include  <assert.h>

using namespace std;

/*
 * The bus_map is a collection of all the configured busses. The key
 * is the port id string, which is unique for every bus. Clients that
 * attach to the system use the port id to identify their bus when
 * they first attach.
 */
map <string, struct bus_state> bus_map;

/*
 * The client_map is a collection of all the clients that have
 * connected to this server. Initially, this map is empty. When
 * clients connect to this server, the "listen_ready" function accepts
 * the connection to the bus port, accepts the new connection and uses
 * the generated fd as a key to the client_map. So this map is built
 * up as clients connect to the server.
 *
 * The client_map is used by the service_run() function. When the
 * "select" system call notices activity on an fd, the client_map is
 * used to map that fd to the client.
 */
static map<int,client_state_t> client_map;
typedef map<int,client_state_t>::iterator client_map_idx_t;

/*
 * If the server is supposed to write lxt output, this is the pointer
 * to the lxt writer.
 */
struct lxt2_wr_trace*service_lxt = 0;

static void service_uninit(void)
{
      if (service_lxt) lxt2_wr_close(service_lxt);
}

/*
 * The service_init() function is called by main() during startup to
 * do the initial setup of data structures. It is called before
 * anything else can happen with the service.
 */
void service_init(const char*trace_path)
{
      service_lxt = trace_path? lxt2_wr_init(trace_path) : 0;
      if (service_lxt) {
	    printf("Dumping bus signals to %s\n", trace_path);
	    lxt2_wr_set_timescale(service_lxt, SERVICE_TIME_PRECISION);
	    lxt2_wr_set_time(service_lxt, 0);
      }
      atexit(&service_uninit);
}

/*
 * While configuring, the config file parser calls this function to
 * create a bus. All the contents of the bus have been collected by
 * the parser and are handed to this function to actually create the
 * bus.
 */
void service_add_bus(const std::string&port, const std::string&name,
		     const std::string&bus_protocol_name,
		     const bus_device_map_t&dev)
{
	// The bus is stored in the bus_map with its port string as
	// the key. Each bus has its own port.
      bus_state&tmp = bus_map[port];

      tmp.name = name;
      tmp.fd = -1;
      tmp.need_initialization = true;
      tmp.finished = false;
      tmp.device_map = dev;

      if (bus_protocol_name == "pci") {
	    tmp.proto = new PciProtocol(tmp);
      } else {
	    tmp.proto = 0;
      }
}

/*
 * Given a port string, open a socket of the right type and return the
 * fd. The port strings come from the bus description:
 *
 *     tcp:<number>         -- TCP/IP port stream (port = <number>)
 *     pipe:<path>          -- named pipe         (pipe = <path>)
 */
static int socket_from_string(string astr, struct bus_state&bus_obj)
{
      int fd = -2;
      int rc;

      if (astr.substr(0,4) == "tcp:") {
	    astr.erase(0,4);

	    fd = socket(PF_INET, SOCK_STREAM, 0);

	    struct sockaddr_in addr;
	    memset(&addr, 0, sizeof addr);

	      // If the address string is a simple number, then assume
	      // it is a simple TCP port number.
	    if (astr.find_first_not_of("0123456789") == string::npos) {

		  addr.sin_family = AF_INET;
		    // The port is the map key.
		  addr.sin_port = htons(strtoul(astr.c_str(), 0, 10));
		  addr.sin_addr.s_addr = INADDR_ANY;

	    } else {
		  fprintf(stderr, "Invalid tcp port number: <%s> first bad char is %d of %d\n", astr.c_str(), astr.find_first_not_of("0123456789"), astr.size());
		  assert(0);
	    }

	    rc = bind(fd, (const struct sockaddr*)&addr, sizeof addr);

      } else if (astr.substr(0,5) == "pipe:") {
	    astr.erase(0,5);

	    fd = socket(PF_UNIX, SOCK_STREAM, 0);

	    struct sockaddr_un addr;
	    memset(&addr, 0, sizeof addr);

	    assert(astr.size() < sizeof addr.sun_path);
	    addr.sun_family = AF_UNIX;
	    strcpy(addr.sun_path, astr.c_str());

	    rc = bind(fd, (const struct sockaddr*)&addr, sizeof addr);

	      // Save the path to be unlinked on setup complete.
	    bus_obj.unlink_on_initialization.push_back(astr);

      } else {
	    assert(0);
      }

      return fd;
}

/*
 * This function is called once to start the service running. It binds
 * to the network sockets for all the busses. All the config files
 * have been parsed first.
 */
static void service_setup(void)
{
      int rc;

      for (bus_map_idx_t cur = bus_map.begin() ; cur != bus_map.end(); cur++) {

	      // Bind the service port address to the socket.
	      // The port is the map key.
	    cur->second.fd = socket_from_string(cur->first, cur->second);

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
 * This function is called when the select loop finds that the port
 * for a client is ready. Read the data from the connection and
 * process it.
 */
static void client_ready(client_map_idx_t&client)
{
      client->second.read_from_socket(client->first);
}

void service_run(void)
{
      int rc;

      service_setup();

      while (true) {
	    int nfds = 0;
	    fd_set rfds;
	    FD_ZERO(&rfds);

	      // Add the server ports to the fd list.
	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end(); idx++) {
		  int fd = idx->second.fd;
		  if (fd >= 0) {
			FD_SET(fd, &rfds);
			if (fd >= nfds)
			      nfds = fd + 1;
		  }
	    }

	      // Add the client ports to the fd list.
	    for (client_map_idx_t idx = client_map.begin()
		       ; idx != client_map.end() ; idx ++) {
		  int fd = idx->first;
		  if (! idx->second.is_exited()) {
			FD_SET(fd, &rfds);
			if (fd >= nfds)
			      nfds = fd + 1;
		  }
	    }

	    if (nfds == 0) {
		  printf("... Nothing more to do.\n");
		  break;
	    }

	      // Wait for bus or client ports.
	    rc = select(nfds, &rfds, 0, 0, 0);
	    if (rc == 0)
		  continue;

	    if (rc < 0) {
		  printf("... select returns %d (errno=%d)\n", rc, errno);
		  break;
	    }

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
	      // protocol run. The *_ready() function may have changed
	      // the bus state such that the protocol has something
	      // interesting to do.
	    for (bus_map_idx_t idx = bus_map.begin()
		       ; idx != bus_map.end() ; idx ++) {

		  bool flag = true;
		  for (bus_device_map_t::iterator dev = idx->second.device_map.begin()
			     ; dev != idx->second.device_map.end() ;  dev ++) {
			if (dev->second.ready_flag == false) {
			      flag = false;
			      break;
			}

			if (dev->second.finish_flag)
			      idx->second.finished = true;
		  }

		  if (flag == true) {
			  // If this is the first time all the devices
			  // are ready, then the bus is finally
			  // assembled, and needs final initialization.
			if (idx->second.need_initialization)
			      idx->second.assembly_complete();

			  // Run the first bus state.
			idx->second.proto->bus_ready();
		  }
	    }

      }
}

void bus_state::assembly_complete()
{
      cout << name << ": Bus assembly complete." << endl;

      proto->run_init();

      while (unlink_on_initialization.size() > 0) {
	    string path = unlink_on_initialization.front();
	    unlink_on_initialization.pop_front();
	    unlink(path.c_str());
      }

      need_initialization = false;
}
