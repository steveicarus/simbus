#ifndef __client_H
#define __client_H
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

# include  <string>
# include  <stddef.h>
# include  "priv.h"

/*
 * A client is mapped using its file descriptor as the key. The client
 * contains the "bus", which is the number of the bus that it belongs
 * to, and can be used to look up the port.
 */
class client_state_t {

    public:
      client_state_t();

	// Read data from the socket, and process any commands that
	// might appear.
      int read_from_socket(int fd);

	// Bind me to my bus.
      void set_bus(const std::string&bus_key);

	// Return true if the FINISH command has been send for this
	// client. In this case, the FD will not be valid anymore.
      bool is_exited(void) const;

    private:
      void process_client_command_(int fd, int argv, char*argv[]);
      void process_client_hello_(int fd, int argv, char*argv[]);
      void process_client_ready_(int fd, int argv, char*argv[]);
      void process_client_finish_(int fd, int argv, char*argv[]);

    private:
	// Key of the bus that I belong to.
      std::string bus_;

	// Device name and ident.
      std::string dev_name_;

	// State information
      struct bus_device_plug*bus_interface_;

	// Keep an input buffer of data read from the connection.
      char buffer_[4096];
      size_t buffer_fill_;
};

#endif
