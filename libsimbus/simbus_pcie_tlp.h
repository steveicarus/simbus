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


EXTERN int simbus_pcie_tlp_wait(simbus_pcie_tlp_t bus, unsigned clks);
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
 * Send an end-of-simulation message to the simulator, then dosconnect
 * and close the bus object. Only HOST devices should call the
 * simbus_pcie_tlp_end_simulation function.
 */
EXTERN void simbus_pcie_tlp_end_simulation(simbus_pcie_tlp_t bus);

#endif
