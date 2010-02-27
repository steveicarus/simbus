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
# include  <stdio.h>

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
 * Set a log file for the pci interface. This causes detailed library
 * debug messages to that file. Turn debugging off by setting the file
 * to NIL
 */
EXTERN void simbus_pci_debug(simbus_pci_t pci, FILE*fd);

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
EXTERN uint32_t simbus_pci_read32(simbus_pci_t bus, uint64_t addr, int BEn);
EXTERN uint64_t simbus_pci_read64(simbus_pci_t bus, uint64_t addr, int BEn);

/*
 * Set handlers for target cycles. These handlers are invoked when
 * the device represented by the simbus_pci_t object is a target to a
 * bus cycle. The needXX_fun_t functions are called by the library
 * when the bus is reading from this device, and the recvXX_fun_t
 * functions are called by the library when the bus is writing to this
 * device.
 */
typedef uint32_t (*need32_fun_t) (simbus_pci_t bus, uint64_t addr, int BEn);
typedef void (*recv32_fun_t) (simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn);

typedef uint64_t (*need64_fun_t) (simbus_pci_t bus, uint64_t addr, int BEn);
typedef void (*recv64_fun_t) (simbus_pci_t bus, uint64_t addr, uint64_t val, int BEn);

EXTERN void simbus_pci_config_need32(simbus_pci_t bus, need32_fun_t fun);
EXTERN void simbus_pci_config_recv32(simbus_pci_t bus, recv32_fun_t fun);

/*
 * Memory regions are claimed by the target device by calling the
 * simbus_pci_mem_xlate function with the region number and a pointer
 * to the translation descriptor. The target API supports up to 8
 * translation regions, each with their own base address and mask. An
 * address matches the region if (addr&mask) == (base&mask).
 */
struct simbus_translation {
      int flags;
      uint64_t base;
      uint64_t mask;
      need32_fun_t need32;
      need64_fun_t need64;
      recv32_fun_t recv32;
      recv64_fun_t recv64;
};

EXTERN void simbus_pci_mem_xlate(simbus_pci_t bus, unsigned idx,
				 const struct simbus_translation*drv);

/*
 * Send an end-of-simulation message to the simulator.
 */
EXTERN void simbus_pci_end_simulation(simbus_pci_t bus);

#undef EXTERN

#endif
