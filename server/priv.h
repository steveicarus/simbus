#ifndef __priv_H
#define __priv_H
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

# include  <stdio.h>
# include  <map>
# include  <string>

/* Parse the config file. */
extern int config_file(FILE*cfg);

extern void service_init(void);
extern void service_run(void);

/*
 * A bus contains a configuration that is a set of bus devices. The
 * configuration is read from the input configuration file and
 * collected into a bus_device_map_t map, with the name of the device
 * as the key.
 */

struct service_bus_device_config {
	// Identifier number to use for the device.
      unsigned ident;
};
typedef std::map<std::string,struct service_bus_device_config> bus_device_map_t;

/*
 * The bus_state describes a bus. The fd is the posix file-descriptor
 * for the server port, and the name is the configured bus name.
 *
 * The bus_map maps the TCP port to the bus. This key is also given to
 * the client so that it can search this table to find its bus.
 */
struct bus_state {
	// Human-readable name for the bus.
      std::string name;
	// posix fd for the bus socker.
      int fd;
	// List of unbound devices
      bus_device_map_t device_map;
};

extern std::map <unsigned, bus_state> bus_map;
typedef std::map<unsigned, bus_state>::iterator bus_map_idx_t;


extern void service_add_bus(unsigned port, const std::string&name,
			    const bus_device_map_t&dev);

/*
 * $Log: $
 */
#endif
