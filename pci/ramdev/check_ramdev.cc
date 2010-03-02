/*
 * Copyright (c) 2010 Stephen Williams (steve@picturel.com)
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

# define __STDC_FORMAT_MACROS 1
# include  <simbus_pci.h>
# include  <stdio.h>
# include  <inttypes.h>
# include  <assert.h>

const uint32_t EXP_DEVICE_ID = 0xffc112c5;
const uint64_t BASE_BAR0_32 = 0x00000000ff000000;
const uint64_t BASE_BAR2_32 = 0x00000000fefff000;

/*
 * This is a test bench for the ramdev memory device. This bench
 * assumes that the device id 0xffc112c5 and a bar0 mask of
 * 0xffff0000.
 */
int main(int argc, char*argv[])
{
      simbus_pci_t pci = simbus_pci_connect(argv[1], "host");
      assert(pci);

      FILE*debug = fopen("host-debug.log", "w");
      simbus_pci_debug(pci, debug);

      simbus_pci_wait(pci, 4, 0);
      simbus_pci_reset(pci, 8, 8);

	// Read the device ID from dev0. This is our DUT. Make sure
	// the device id is correct.
      uint32_t value = simbus_pci_config_read(pci, 0x10000);
      printf("Read from config 0x00010000: %lx...", value);
      if (value == EXP_DEVICE_ID) printf("OK\n");
      else printf("BAD! Expecting 0x%lx\n", EXP_DEVICE_ID);

	// Read the device ID from dev1. This device should not be
	// present, so I should read 0xffffffff.
      value = simbus_pci_config_read(pci, 0x20000);
      printf("Read from config 0x00020000: %lx...", value);
      if (value == 0xffffffff) printf("OK\n");
      else printf("BAD! Expecting 0xffffffff\n");

	// Let a few clock cycles run.
      simbus_pci_wait(pci, 4, 0);

	// Memory test of BAR0
      simbus_pci_config_write(pci, 0x10010, 0xaa55aa55, 0);
      value = simbus_pci_config_read(pci, 0x10010);
      printf("Read from config 0x00010010: %lx...", value);
      if (value == 0xaa55000c) printf("OK\n");
      else printf("BAD! Expecting 0xaa55000c\n");

      simbus_pci_config_write(pci, 0x10010, 0x55aa55aa, 0);
      value = simbus_pci_config_read(pci, 0x10010);
      printf("Read from config 0x00010010: %lx...", value);
      if (value == 0x55aa000c) printf("OK\n");
      else printf("BAD! Expecting 0x55aa000c\n");

	// Memory test of BAR1
      simbus_pci_config_write(pci, 0x10014, 0xaa55aa55, 0);
      value = simbus_pci_config_read(pci, 0x10014);
      printf("Read from config 0x00010014: %lx...", value);
      if (value == 0xaa55aa55) printf("OK\n");
      else printf("BAD! Expecting 0xaa55aa55\n");

      simbus_pci_config_write(pci, 0x10014, 0x55aa55aa, 0);
      value = simbus_pci_config_read(pci, 0x10014);
      printf("Read from config 0x00010014: %lx...", value);
      if (value == 0x55aa55aa) printf("OK\n");
      else printf("BAD! Expecting 0x55aa55aa\n");

	// Now map the device to 0x00000000:ff000000
	// and set up for 32bit addressing tests.
      simbus_pci_config_write(pci, 0x10014, 0x00000000, 0);
      simbus_pci_wait(pci, 2, 0);
      simbus_pci_config_write(pci, 0x10010, 0xff000000, 0);
      simbus_pci_wait(pci, 1, 0);

      simbus_pci_config_write(pci, 0x1001c, 0x00000000, 0);
      simbus_pci_wait(pci, 2, 0);
      simbus_pci_config_write(pci, 0x10018, 0xfefff000, 0);
      simbus_pci_wait(pci, 1, 0);

	// Clear all the status bits,
	// Enable Memory Space access and bus mastering
      simbus_pci_config_write(pci, 0x10004, 0xffff0006, 0);

	// Now check that empty, uninitialized memory reads as zeros.
      printf("Should read zeros from uninitialized ramdev...\n");
      uint64_t base1 = BASE_BAR0_32;
      for (unsigned idx = 0 ;  idx < 512 ;  idx += 8) {
	    uint32_t value1 = simbus_pci_read32(pci, base1 + idx, 0);
	    uint32_t value2 = simbus_pci_read32(pci, base1 + idx + 4, 0);
	    printf("0x%08x: %08lx %08lx\n", base1+idx, value1, value2);
	    if (value1 != 0 || value2 != 0) printf("FAIL\n");
      }

	// Try write/read of a few memory locations.
      simbus_pci_write32(pci, base1+8, 0xaaaa5555, 0);
      simbus_pci_wait(pci, 4, 0);

      value = simbus_pci_read32(pci, base1 + 8, 0);
      printf("0x%08x: %lx (should be 0xaaaa5555)\n", base1+8, value);
      if (value != 0xaaaa5555) printf("FAIL\n");

      simbus_pci_wait(pci, 4, 0);

      simbus_pci_write32(pci, base1+8, 0x0055aa00, 0x09);
      simbus_pci_wait(pci, 4, 0);

      value = simbus_pci_read32(pci, base1+8, 0);
      printf("0x%08x: %lx (should be 0xaa55aa55)\n", base1+8, value);
      if (value != 0xaa55aa55) printf("FAIL\n");

      simbus_pci_wait(pci, 4, 0);
      simbus_pci_end_simulation(pci);

      printf("Done\n");
      return 0;
}
