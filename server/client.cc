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

client_state_t::client_state_t()
{
      bus = 0;
      dev_ident = 0;
      buffer_fill_ = 0;
      ready_flag = false;
}

const char white_space[] = " \r";

int client_state_t::read_from_socket(int fd)
{
      size_t trans = sizeof buffer_ - buffer_fill_ - 1;
      assert(trans > 0);

      int rc = read(fd, buffer_ + buffer_fill_, trans);
      assert(rc >= 0);
      if (rc == 0)
	    return rc;

      buffer_fill_ += rc;
      *(buffer_+buffer_fill_) = 0;

      if (char*eol = strchr(buffer_, '\n')) {
	      // Remote the new-line.
	    *eol++ = 0;
	    std::cerr << "XXXX message: " << buffer_ << std::endl;
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
      char outbuf[4096];

      if (dev_name == "") {
	    if (strcmp(argv[0], "HELLO") != 0) {
		  fprintf(stderr, "Expecting HELLO from client, got %s\n", argv[0]);
		  return;
	    }

	    assert(argc > 1);
	    string use_name = argv[1];

	    bus_map_idx_t bus_info = bus_map.find(bus);
	    assert(bus_info != bus_map.end());

	    bus_device_map_t::iterator cur = bus_info->second.device_map.find(use_name);
	    if (cur == bus_info->second.device_map.end()) {
		  write(fd, "NAK\n", 4);
		  cerr << "Device " << use_name
		       << " not found in bus " << bus_info->second.name
		       << endl;
		  return;
	    }

	    dev_name = use_name;
	    dev_ident = cur->second.ident;

	    snprintf(outbuf, sizeof outbuf, "YOU-ARE %u\n", dev_ident);
	    int rc = write(fd, outbuf, strlen(outbuf));
	    assert(rc == strlen(outbuf));

	    cerr << "Device " << use_name
		 << " is attached to bus " << bus_info->second.name
		 << "." << endl;
	    return;
      }

      if (strcmp(argv[0],"HELLO") == 0) {
	    cerr << "Extra HELLO from " << dev_name << endl;
      } else if (strcmp(argv[0],"READY") == 0) {

      } else {
	    cerr << "Unknown command " << argv[0]
		 << " from client " << dev_name << endl;
      }
}

