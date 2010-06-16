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

# include  <stdio.h>
# include  <map>
# include  <string>
# include  <valarray>
# include  <list>
# include  <fstream>

class protocol_t;

/* Initial pre-configuration. */
extern void service_init(const char*trace_path);

/* Parse the config file. */
extern int config_file(FILE*cfg);

/* Run the processes, if any */
extern void process_run(void);

/* Run the server. */
extern int service_run(void);

/*
 * The signal_sate_map_t is a map if signals with their value. The
 * value is stored as an array of bit states, with <array>[0] store as
 * the LSB.
 */
typedef enum bit_state_e { BIT_0, BIT_1, BIT_Z, BIT_X } bit_state_t;
typedef std::map<std::string, std::valarray<bit_state_t> > signal_state_map_t;

/*
 * A bus contains a configuration that is a set of bus devices,
 * represented by a map of bus_device_plug objects. The configuration
 * is read from the input configuration file and collected into a
 * bus_device_map_t map, with the name of the device as the key. The
 * bus itself then contains this plug as a member.
 */

struct bus_device_plug {
      bus_device_plug() : host_flag(false), fd(-1), ready_flag(false), finish_flag(false), exited_flag(false) { }
	// True if this device is a "host" connection.
      bool host_flag;
	// Identifier number to use for the device.
      unsigned ident;
	// posix fd for the client socket. This fd can also be used to
	// look up the client in the client_map.
      int fd;
	// True when the device is ready for another step.
      bool ready_flag;
      bool finish_flag;
      bool exited_flag;
	// Time that the client last reported.
      uint64_t ready_time;
      int ready_scale;
	// Map of the signal values from the client.
      signal_state_map_t client_signals;
	// Map of this signal values to send to the client.
      signal_state_map_t send_signals;
};
typedef std::map<std::string,struct bus_device_plug> bus_device_map_t;

/*
 * The bus_state describes a bus. The fd is the posix file-descriptor
 * for the server port, and the name is the configured bus name.
 *
 * The bus_map maps the port string to the bus. This key is also given to
 * the client so that it can search this table to find its bus.
 */
struct bus_state {
	// Human-readable name for the bus.
      std::string name;
	// Pointer to the protocol implementation instance for this
	// bus.
      protocol_t*proto;
	// map of options set in the config file. These are
	// interpreted by the specific protocol.
      std::map<std::string,std::string> options;
	// posix fd for the bus socket. This is only used for
	// listening for new clients.
      int fd;
	// when initializing, unlink all the paths in this list.
      std::list<std::string>unlink_on_initialization;
	// True if the bus is finished (by finish command)
      bool finished;
	// List of configured devices. The key is the name of the
	// device, so that the client device can be located when it
	// binds and calls in its name.
      bus_device_map_t device_map;

      void assembly_complete();
};

/*
 * Keep a list of all the busses in the system, indexed by their port
 * string.
 */
extern std::map <std::string, bus_state*> bus_map;
typedef std::map<std::string, bus_state*>::iterator bus_map_idx_t;

/*
 * This function is used by the service config file to add a new bus
 * to the system.
 */
extern void service_add_bus(const std::string&port, const std::string&name,
			    const std::string&bus_protocol_name,
			    const bus_device_map_t&dev,
			    const std::map<std::string,std::string>&options);


/*
 * This function is used by the config file to add a new process to
 * the process executer.
 */
extern void process_add(const std::string&name,
			const std::string&use_exec,
			const std::string&use_stdin,
			const std::string&use_stdout,
			const std::string&use_stderr,
			const std::map<std::string,std::string>&use_env);

/*
 * Hook for the server LXT dumper. It is up to the protocols to figure
 * out what to dump here.
 */
extern struct lxt2_wr_trace*service_lxt;
# define SERVICE_TIME_PRECISION (-10)

/*
 * File for logging protocol interractions with the clients.
 */
extern std::ofstream protocol_log;

#endif
