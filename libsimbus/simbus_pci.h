#ifndef __simbus_pci_H
#define __simbus_pci_H
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

# include  <inttypes.h>

#ifdef __cplusplus
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif

typedef struct simbus_pci_s* simbus_pci_t;

/*
 * Open the connection to the bus server for a PCI bus. The name is
 * the devname to use.
 */
EXTERN simbus_pci_t simbus_pci_connect(const char*server, const char*name);

/*
 * Advance the Verilog simulation some number of PCI clocks, or until
 * one of the interrupts in the irq_enable is active. The irq_enable
 * is a bit-mask of interrupts to be enabled (high bits enable the
 * interrupt) and the return value is the mask of interrupts that
 * interrupted the wait, or 0 if the wait timed out.
 */
EXTERN unsigned simbus_pci_wait(simbus_pci_t bus, unsigned clks, unsigned irq_enable);

/*
 * Cause a RESET# pulse to be generated on the PCI bus of the
 * simulation. The pulse width is given in clocks, and if the settle
 * time is specified, the simulation runs that many more clocks to let
 * PCI devices settle.
 */
EXTERN void simbus_pci_reset(simbus_pci_t bus, unsigned width, unsigned settle);


/*
 * Execute a PCI configuration read cycle and return the 32bit value
 * that was read. The address is the type-0 or type-1 address
 * according to PCI config space conventions.
 */
EXTERN uint32_t simbus_pci_config_read(simbus_pci_t bus, uint64_t addr);

/*
 * Execute a PCI configuration WRITE cycle to write the 32bit
 * value. The BEn is a bit-mask that enables byte lanes of the 32bit
 * value. This mask follows the PCI conventions, so BEn==0 writes all
 * for bytes, and BEn==0xe enables only the least significant byte.
 */
EXTERN void simbus_pci_config_write(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn);

/*
 * Write 32/64 bit values to the bus, enabled by the BEn, using PCI
 * BE# conventions.
 *
 * The write64 uses 64bit data and address, even if the BEn is 0xf0
 * and the address is 32bits.
 *
 * The write32 uses 32bit PCI only. If the address is more then
 * 32bits, then use a DAC to write the 64bit address.
 */
EXTERN void simbus_pci_write32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn);
EXTERN void simbus_pci_write64(simbus_pci_t bus, uint64_t addr, uint64_t val, int BEn);

/*
 * Read 32/64 bit values from the bus.
 *
 * The read64 uses 64bit data and address, even if the address is less
 * then 32 bits.
 *
 * The read32 uses 32bit PCI only. If the address is more then 32its,
 * then use a DAC to write the 64bit address.
 */
EXTERN uint32_t simbus_pci_read32(simbus_pci_t bus, uint64_t addr);
EXTERN uint64_t simbus_pci_read64(simbus_pci_t bus, uint64_t addr);

/*
 * Send an end-of-simulation message to the simulator.
 */
EXTERN void simbus_pci_end_simulation(simbus_pci_t bus);

#undef EXTERN

#endif
