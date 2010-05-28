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

# include  "client.h"
# include  "priv.h"
# include  <iostream>
# include  <assert.h>

using namespace std;

std::map<int, client_state_t> client_state_t::client_map;

client_state_t::client_state_t()
{
      bus_interface_ = 0;
      buffer_fill_ = 0;
}

void client_state_t::set_bus(const std::string&bus_key)
{
      assert(bus_.size() == 0);
      bus_ = bus_key;
}

bool client_state_t::is_exited(void) const
{
      if (bus_interface_ == 0)
	    return false;
      return bus_interface_->exited_flag;
}

const char white_space[] = " \r";

int client_state_t::read_from_socket(int fd)
{
      size_t trans = sizeof buffer_ - buffer_fill_ - 1;
      assert(trans > 0);

      int rc = read(fd, buffer_ + buffer_fill_, trans);
      assert(rc >= 0);
      if (rc == 0) {
	      // Locate the bus that I'm part of.
	    bus_map_idx_t bus_info = bus_map.find(bus_);
	    assert(bus_info != bus_map.end());
	    cerr << "EOF from "
		 << (bus_interface_->host_flag? "host" : "device")
		 << " " << dev_name_
		 << ", forcing detach from bus " << bus_info->second.name
		 << "." << endl;
	    bus_interface_->ready_flag  = true;
	    bus_interface_->finish_flag = true;
	    return rc;
      }

      buffer_fill_ += rc;
      *(buffer_+buffer_fill_) = 0;

      if (char*eol = strchr(buffer_, '\n')) {
	      // Remove the new-line.
	    *eol++ = 0;
	    int argc = 0;
	    char*argv[2048];

	    char*cp = buffer_;
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
	    process_client_command_(fd, argc, argv);

	      // Remove the command line from the input buffer
	    buffer_fill_ -= eol - buffer_;
	    memmove(buffer_, eol, buffer_fill_);
      }
}

void client_state_t::process_client_command_(int fd, int argc, char*argv[])
{
      if (dev_name_ == "") {
	    process_client_hello_(fd, argc, argv);
	    return;
      }

      protocol_log << dev_name_ << ":RECV:" << argv[0];
      for (int idx = 1 ; idx < argc ; idx += 1)
	    protocol_log << " " << argv[idx];
      protocol_log << endl;

      if (strcmp(argv[0],"HELLO") == 0) {
	    cerr << "Spurious HELLO from " << dev_name_ << endl;

      } else if (strcmp(argv[0],"READY") == 0) {
	    process_client_ready_(fd, argc, argv);

      } else if (strcmp(argv[0],"FINISH") == 0) {
	    process_client_finish_(fd, argc, argv);

      } else {
	    cerr << "Unknown command " << argv[0]
		 << " from client " << dev_name_ << endl;
      }
}

void client_state_t::process_client_hello_(int fd, int argc, char*argv[])
{
      if (strcmp(argv[0], "HELLO") != 0) {
	    fprintf(stderr, "Expecting HELLO from client, got %s\n", argv[0]);
	    return;
      }

      assert(argc > 1);
      string use_name = argv[1];

	// Locate the bus that I'm part of.
      bus_map_idx_t bus_info = bus_map.find(bus_);
      assert(bus_info != bus_map.end());

	// Find my device_map record on the bus. If it is not found,
	// then send a NAK message back to the client.
      bus_device_map_t::iterator cur = bus_info->second.device_map.find(use_name);
      if (cur == bus_info->second.device_map.end()) {
	    write(fd, "NAK\n", 4);
	    cerr << "Device " << use_name
		 << " not found in bus " << bus_info->second.name
		 << endl;
	    return;
      }

	// Now I have a name for myself. This name came from the
	// "HELLO" command, but it matches the device in the bus, so
	// its confirmed.
      dev_name_ = use_name;

	// Link to the bus plug for this client, and fill in the
	// client data that the plug needs.
      bus_interface_ = &(cur->second);
      bus_interface_->fd = fd;
      bus_interface_->ready_flag = false;

	// Send a message back to the client saying that it was found
	// in the bus and now has an identifier.
      char outbuf[4096];
      snprintf(outbuf, sizeof outbuf, "YOU-ARE %u\n",bus_interface_->ident);
      int rc = write(fd, outbuf, strlen(outbuf));
      assert(rc == strlen(outbuf));

      protocol_log << dev_name_ << ":SEND:YOU-ARE "
		   << bus_interface_->ident << endl;

      cerr << "Device " << use_name
	   << " is attached to bus " << bus_info->second.name
	   << " as " << (bus_interface_->host_flag? "host" : "device")
	   << " " << bus_interface_->ident << "." << endl;
}

void client_state_t::process_client_ready_(int fd, int argc, char*argv[])
{
	// The first argument arger the READY keyword in the client
	// time. Parse that as mantissa/scale.
      assert(argc >= 2);
      char*ep = 0;
      bus_interface_->ready_time = strtoul(argv[1], &ep, 10);

      assert(ep[0] == 'e' && ep[1] != 0);
      ep += 1;
      bus_interface_->ready_scale = strtol(ep, &ep, 10);
      assert(*ep == 0);

	// The remaining arguments are <name>=<value> tokens.
      for (int idx = 2 ; idx < argc ; idx += 1) {

	      // Parse the <name> from the token
	    ep = strchr(argv[idx], '=');
	    assert(ep && *ep=='=');
	    *ep++ = 0;

	      // Build the array of bit values from the <value>
	    std::valarray<bit_state_t> tmp (strlen(ep));
	    for (int bit = 0 ; ep[bit] != 0 ;  bit += 1) {
		    // Note that the string is MSB first, but we want
		    // to write the LSB into tmp[0].
		  int array_idx = tmp.size() - bit - 1;
		  switch (ep[bit]) {
		      case '0':
			tmp[array_idx] = BIT_0;
			break;
		      case '1':
			tmp[array_idx] = BIT_1;
			break;
		      case 'z':
			tmp[array_idx] = BIT_Z;
			break;
		      case 'x':
			tmp[array_idx] = BIT_X;
			break;
		      default:
			assert(0);
			tmp[array_idx] = BIT_X;
			break;
		  }
	    }

	    bus_interface_->client_signals[argv[idx]].resize(tmp.size());
	    bus_interface_->client_signals[argv[idx]] = tmp;
      }

	// This client is now ready and waiting for the server.
      bus_interface_->ready_flag = true;
}

void client_state_t::process_client_finish_(int fd, int argc, char*argv[])
{
	// Locate the bus that I'm part of.
      bus_map_idx_t bus_info = bus_map.find(bus_);
      assert(bus_info != bus_map.end());

      cerr << "Device " << dev_name_
	   << " detached from bus " << bus_info->second.name
	   << " as " << (bus_interface_->host_flag? "host" : "device")
	   << " " << bus_interface_->ident
	   << " with FINISH command." << endl;
      bus_interface_->ready_flag  = true;
      bus_interface_->finish_flag = true;
}
