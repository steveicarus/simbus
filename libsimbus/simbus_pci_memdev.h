#ifndef __simbus_pci_memdev_H
#define __simbus_pci_memdev_H
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


/*
 * The pci_memory module implements a PCI target memory region with
 * some extra smarts. Once initialized, the device can be used as a
 * memory target by other devices on the PCI bus.
 *
 *   Vendor ID: 0x12c5 (Picture Elements, Inc.)
 *   Device ID: 0xffc1 (pci63_memory device)
 */
typedef struct simbus_pci_memdev_s *simbus_pci_memdev_t;

#ifdef __cplusplus
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif


/*
 * Given a config space base address, open the pcisim_memdev
 * device. The config address depends on the system configuration,
 * which attaches idsel signals for the device to the right
 * places. The device should already have been mapped into the address
 * space by whatever BIOS is being used.
 */
EXTERN simbus_pci_memdev_t simbus_pci_memdev_init(simbus_pci_t bus, uint8_t config);

/*
 * Get the base address of the main memory. This is the address
 * programmed into BAR0, with the flags removed. This is the address
 * that can be passed to other devices as pointers.
 */
EXTERN uint64_t simbus_pci_memdev_start(simbus_pci_memdev_t xsp);

/*
 * This function causes a region of memory to be filled with a given
 * value. The "off" parameter is the offset into the memory area, and
 * the cnt is the number of DWORDS to fill. The fill parameter is the
 * value to write into the memory.
 *
 * The offset must be a multiple of 4, and cnt is the number of
 * *DWORDS*, not bytes.
 */
EXTERN void simbus_pci_memdev_fill(simbus_pci_memdev_t xsp,
				   uint64_t off, unsigned cnt,
				   uint32_t fill);

/*
 * This function does a BYTE compare of two regions within the
 * memory. The off0 and off1 arguments are the offsets within this
 * memory of the regions to compare, and the cnt is the number of
 * bytes to compare.
 *
 * This function returns 0 if the regions are identical.
 */
EXTERN int simbus_pci_memdev_compare(simbus_pci_memdev_t xsp,
				     uint64_t off0, uint64_t off1,
				     unsigned cnt);

/*
 * These commands tell the memory device to load/save data to/from a
 * host file. The return code is the number of bytes of I/O, or a
 * value <0 for an error.
 */
EXTERN int simbus_pci_memdev_load(simbus_pci_memdev_t xsp,
				  uint64_t offset, unsigned count,
				  const char*path);

EXTERN int simbus_pci_memdev_save(simbus_pci_memdev_t xsp,
				  uint64_t offset, unsigned count,
				  const char*path);

#endif
