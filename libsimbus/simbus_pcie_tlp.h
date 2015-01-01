#ifndef __simbus_pcie_tlp_H
#define __simbus_pcie_tlp_H
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

# include  <inttypes.h>
# include  <stdio.h>

#ifdef __cplusplus
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif

typedef struct simbus_pcie_tlp_s* simbus_pcie_tlp_t;

/*
 * Open the connection to the bus server for a Xilinx PCIe core bus. The
 * name is the devname to use, which is conventionally "root".
 */
EXTERN simbus_pcie_tlp_t simbus_pcie_tlp_connect(const char*server, const char*name);

/*
 * Set a log file for the pci interface. This causes detailed library
 * debug messages to that file. Turn debugging off by setting the file
 * to NIL
 */
EXTERN void simbus_pcie_tlp_debug(simbus_pcie_tlp_t bus, FILE*fd);

/*
 * Wait for the number of clocks, or until the enabled interupt
 * trips. The irq_mask points to a mask of enabled interrupts. If any
 * of the enabled interrupts in the mask are asserted, or become
 * asserted, then the function sets the irq_mask value with the value
 * of the enabled interrupts that are asserted.
 */
EXTERN int simbus_pcie_tlp_wait(simbus_pcie_tlp_t bus, unsigned clks, int*irq_mask);
# define SIMBUS_PCIE_TLP_ERROR    (-1)
# define SIMBUS_PCIE_TLP_FINISHED (-2)

/*
 * Cause a "user_reset_out" pulse to be generated on the PCI bus of the
 * simulation. The pulse width is given in clocks, and if the settle
 * time is specified, the simulation runs that many more clocks to let
 * PCI devices settle.
 */
EXTERN void simbus_pcie_tlp_reset(simbus_pcie_tlp_t bus, unsigned width, unsigned settle);

EXTERN void simbus_pcie_tlp_cfg_write32(simbus_pcie_tlp_t bus,
					   uint16_t bus_devfn, uint16_t addr,
					   uint32_t val);
EXTERN void simbus_pcie_tlp_cfg_write16(simbus_pcie_tlp_t bus,
					   uint16_t bus_devfn, uint16_t addr,
					   uint16_t val);
EXTERN void simbus_pcie_tlp_cfg_write8(simbus_pcie_tlp_t bus,
					   uint16_t bus_devfn, uint16_t addr,
					   uint8_t val);

EXTERN void simbus_pcie_tlp_cfg_read32(simbus_pcie_tlp_t bus,
					  uint16_t bus_devfn, uint16_t addr,
					  uint32_t*val);

/*
 * Generate and transmit a Memory Write Request TLP for the specified
 * number of 32bit words. The TLP will be assembled for a non-posted
 * write, and transmitted in the minimum number of clocks.
 *
 * The addr is a 64bit address. The function will notice if the high
 * address bits are set, and if not will generate a 32bit address
 * cycle.
 *
 * The data is a pointer to an array of 32bit words. The ndata will be
 * used to set the TPL data payload length.
 *
 * The off/len will be used to configure the first and last byte
 * enables. The "off" is the number of initial bytes to skip (can be
 * 0, 1, 2, or 3) and the "len" in the byte out of all the valid
 * data. The value len+off must be <= 4*ndata and > 4*(ndata-1).
 */
EXTERN void simbus_pcie_tlp_write(simbus_pcie_tlp_t bus, uint64_t addr,
				     const uint32_t*data, size_t ndata,
				     int off, size_t len);

EXTERN void simbus_pcie_tlp_read(simbus_pcie_tlp_t bus, uint64_t addr,
				 uint32_t*data, size_t ndata,
				 int off, size_t len);

/*
 * When Write/read TLPs are received from the remote, the function
 * calls a callback to handle the data. The *_read_t and *_write_t
 * types describe pointers to functions that the bus calls to respond
 * to the types of TLPs.
 *
 * The simbus_pcie_tlp_cookie_t type is a pointer to a structure that
 * the user can define to point to user defined details. Private data,
 * in other words. The simbus_pcie_tlp library doesn't provide a
 * definition for the struct, the user can supply whatever definition
 * is desired.
 *
 * In any case, these functions can only be called by some other
 * function that waits on PCIe clocks.
 */
typedef struct simbus_pcie_tlp_cookie_s*simbus_pcie_tlp_cookie_t;

typedef void (*simbus_pcie_tlp_write_t) (simbus_pcie_tlp_t bus,
					 simbus_pcie_tlp_cookie_t cookie,
					 uint64_t addr,
					 const uint32_t*data, size_t ndata,
					 int be0, int beN);
typedef void (*simbus_pcie_tlp_read_t) (simbus_pcie_tlp_t bus,
					simbus_pcie_tlp_cookie_t cookie,
					uint64_t addr,
					uint32_t*data, size_t ndata,
					int be0, int beN);

EXTERN void simbus_pcie_tlp_write_handle(simbus_pcie_tlp_t bus,
					 simbus_pcie_tlp_write_t fun,
					 simbus_pcie_tlp_cookie_t cookie);
EXTERN void simbus_pcie_tlp_read_handle(simbus_pcie_tlp_t bus,
					simbus_pcie_tlp_read_t fun,
					simbus_pcie_tlp_cookie_t cookie);

/*
 * Send an end-of-simulation message to the simulator, then dosconnect
 * and close the bus object. Only HOST devices should call the
 * simbus_pcie_tlp_end_simulation function.
 */
EXTERN void simbus_pcie_tlp_end_simulation(simbus_pcie_tlp_t bus);

#endif
