#ifndef __simbus_axi4_H
#define __simbus_axi4_H
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
# include  <inttypes.h>
# include  <stdio.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

typedef struct simbus_axi4_s*simbus_axi4_t;

/*
 * Open a connection to the bus server for an axi4 bus. The name is
 * the devname to use.
 */
EXTERN simbus_axi4_t simbus_axi4_connect(const char*server, const char*name,
					 size_t data_width,
					 size_t addr_width,
					 size_t wid_width,
					 size_t rid_width);

/*
 * Set a log file for the axi4 interface. This causes detailed library
 * debug messages to that file. Turn debugging off by setting the file
 * to NIL
 */
EXTERN void simbus_axi4_debug(simbus_axi4_t bus, FILE*fd);

/*
 * Wait some number of bus clocks.
 *
 * If the function returns because if an error, the function will
 * return an error code.
 *
 *   SIMBUS_AXI4_ERROR
 *     Some error occurred on the AXI4 bus.
 *
 *   SIMBUS_AIX4_FINISH
 *     The simulation terminated by a FINISH command by the host. The
 *     proper response to this is to simbus_pci_disconnect().
 */
EXTERN int simbus_axi4_wait(simbus_axi4_t bus, unsigned clks);
# define SIMBUS_AXI4_ERROR    (-1)
# define SIMBUS_AXI4_FINISHED (-2)
# define SIMBUS_AXI4_BREAK    (-3)

/*
 * Cause a ARESETn pulse to ben generated on the AXI4 bus. The pulse
 * width is given in clocks (ACLK) and if the settle time is given,
 * the simulation runs that many more ACLK clocks to let the target
 * device settle.
 */
EXTERN void simbus_axi4_reset(simbus_axi4_t bus, unsigned width, unsigned settle);

/*
 * These flaag bits are used for the protection flags of read/write
 * operations.
 */
# define SIMBUS_AXI4_PRIVILIGED  0x01
# define SIMBUS_AXI4_SECURE      0x02
# define SIMBUS_AXI4_INSTRUCTION 0x04

/*
 * The simbus_axi4_resp_t is the results/response from an AXI4
 * read/write transaction.
 */
typedef enum simbus_axi4_resp_e {
      SIMBUS_AXI4_RESP_OKAY   = 0x00,
      SIMBUS_AXI4_RESP_EXOKAY = 0x01,
      SIMBUS_AXI4_RESP_SLVERR = 0x02,
      SIMBUS_AXI4_RESP_DECERR = 0x03
}  simbus_axi4_resp_t;

/*
 * These functions write/read a single word. They take as many clocks
 * as necessary to complete the transaction, then return the AXI4
 * responce code that completed the transaction.
 */
EXTERN simbus_axi4_resp_t simbus_axi4_write32(simbus_axi4_t bus,
					      uint64_t addr,
					      int prot,
					      uint32_t data,
					      int strb);

EXTERN simbus_axi4_resp_t simbus_axi4_read32(simbus_axi4_t bus,
					     uint64_t addr,
					     int prot,
					     uint32_t*data);

EXTERN simbus_axi4_resp_t simbus_axi4_write8(simbus_axi4_t bus,
					     uint64_t addr,
					     int prot,
					     uint8_t data);

EXTERN simbus_axi4_resp_t simbus_axi4_read8(simbus_axi4_t bus,
					    uint64_t addr,
					    int prot,
					    uint8_t*data);


/*
 * Send an end-of-simulation message to the simulator, then disconnect
 * and close the bus object. Only the master devices should call the
 * simbus_axi4_end_simulation function. Other devices should call the
 * simbus_axi4_disconnect function, which skips the end-of-simulation
 * message.
 */
EXTERN void simbus_axi4_end_simulation(simbus_axi4_t bus);
EXTERN void simbus_axi4_disconnect(simbus_axi4_t bus);

#undef EXTERN
#endif
