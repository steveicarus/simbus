#ifndef __simbus_axi4_common_H
#define __simbus_axi4_common_H
/*
 * Copyright (c) 2014 Stephen Williams (steve@icarus.com)
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
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

# include  <stddef.h>
# include  <stdio.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

typedef struct simbus_axi4_s*simbus_axi4_t;

/*
 * The simbus_axi4_resp_t is the results/response from an AXI4
 * read/write transaction. Various read/write function use this.
 */
typedef enum simbus_axi4_resp_e {
      SIMBUS_AXI4_RESP_OKAY   = 0x00,
      SIMBUS_AXI4_RESP_EXOKAY = 0x01,
      SIMBUS_AXI4_RESP_SLVERR = 0x02,
      SIMBUS_AXI4_RESP_DECERR = 0x03
}  simbus_axi4_resp_t;

/*
 * Open a connection to the bus server for an axi4 bus. The name is
 * the devname to use, and the server is the name of the simbus server
 * to connect to.
 *
 * The simbus_axi4_disconnect() function is the opposite of the
 * connect (it is like "close") but is not needed if
 * simbus_axi4_end_simulation is called.
 */
EXTERN simbus_axi4_t simbus_axi4_connect(const char*server, const char*name,
					 size_t data_width,
					 size_t addr_width,
					 size_t wid_width,
					 size_t rid_width,
					 size_t irq_width);
EXTERN void simbus_axi4_disconnect(simbus_axi4_t bus);

/*
 * Set a log file for the axi4 interface. This causes detailed library
 * debug messages to that file. Turn debugging off by setting the file
 * to NIL
 */
EXTERN void simbus_axi4_debug(simbus_axi4_t bus, FILE*fd);

/*
 * These are the return values from the _wait functions.
 */
# define SIMBUS_AXI4_ERROR    (-1)
# define SIMBUS_AXI4_FINISHED (-2)
# define SIMBUS_AXI4_BREAK    (-3)

#undef EXTERN
#endif
