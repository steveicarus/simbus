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

# include  "simbus_axi4_common.h"
# include  <stddef.h>
# include  <inttypes.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*
 * Wait some number of bus clocks, or until one of the interrupts in
 * the irq_mask is active. The irq_mask as passed in is a bit-mask of
 * interrupts to be enabled (1-bits enable the interrupt). The
 * irq_mask is replaced with the interrupts that are actually
 * active. If the function returns due to an interrupt, the reture
 * value is >0.
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
EXTERN int simbus_axi4_wait(simbus_axi4_t bus, unsigned clks, uint32_t*irq_mask);

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
 * These functions write/read a single word. They take as many clocks
 * as necessary to complete the transaction, then return the AXI4
 * response code that completed the transaction. In AXI4-Lite mode,
 * the bus width must be >= the width of the word, and the
 * implementation will use the strobes to control the writes, or will
 * mask the desired results out of the read.
 */
EXTERN simbus_axi4_resp_t simbus_axi4_write64(simbus_axi4_t bus,
					      uint64_t addr,
					      int prot,
					      uint64_t data);

EXTERN simbus_axi4_resp_t simbus_axi4_read64(simbus_axi4_t bus,
					     uint64_t addr,
					     int prot,
					     uint64_t*data);

EXTERN simbus_axi4_resp_t simbus_axi4_write32(simbus_axi4_t bus,
					      uint64_t addr,
					      int prot,
					      uint32_t data);

EXTERN simbus_axi4_resp_t simbus_axi4_read32(simbus_axi4_t bus,
					     uint64_t addr,
					     int prot,
					     uint32_t*data);

EXTERN simbus_axi4_resp_t simbus_axi4_write16(simbus_axi4_t bus,
					      uint64_t addr,
					      int prot,
					      uint16_t data);

EXTERN simbus_axi4_resp_t simbus_axi4_read16(simbus_axi4_t bus,
					     uint64_t addr,
					     int prot,
					     uint16_t*data);


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

#undef EXTERN
#endif
