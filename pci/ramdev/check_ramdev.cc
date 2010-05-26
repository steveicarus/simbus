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
const uint64_t BASE_BAR2_32 = 0x00000000feffe000;

const uint64_t  BASE_MEM_SPACE = 0x00000000a0000000;
const size_t    MEM_WORDS      = 1024*1024;
const uint64_t  MASK_MEM_SPACE = ~((uint64_t)4 * MEM_WORDS - 1);
static uint32_t mem_space [MEM_WORDS];

static uint32_t mem_need32(simbus_pci_t bus, uint64_t addr, int BEn);
static void mem_recv32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn);

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

      uint64_t value64;

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

	// Set up host memory to be accessible via PCI.
      struct simbus_translation mem_map;
      mem_map.flags = 0;
      mem_map.need32 = &mem_need32;
      mem_map.need64 = 0;
      mem_map.recv32 = &mem_recv32;
      mem_map.recv64 = 0;
      mem_map.base = BASE_MEM_SPACE;
      mem_map.mask = MASK_MEM_SPACE;
      simbus_pci_mem_xlate(pci, 0, &mem_map);

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

	// Try 64bit write/read.
      value64 = 0x1122334455667788;
      simbus_pci_write64(pci, base1+16, value64, 0);
      simbus_pci_wait(pci, 1, 0);

      value64 = simbus_pci_read64(pci, base1+16, 0);
      printf("0x%08x: %016llx (should be 0x1122334455667788)\n", base1+16, value64);

	// Have the memory device bus-master read from me. Load a
	// stretch of 64 words with some known data, and configure the
	// DMA controller on the pcimem to read that data into its own
	// space.
      for (int idx = 0 ; idx < 64 ; idx += 1)
	    mem_space[idx] = idx;

      simbus_pci_write32(pci, BASE_BAR2_32+0x1008, BASE_MEM_SPACE, 0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x100c, 0,              0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1010, 0x1000,         0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1004, 64*4,           0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1000, 0x01,           0);

      int wait_time = 100;
      uint32_t status = simbus_pci_read32(pci, BASE_BAR2_32+0x1000, 0);
      while (wait_time && (status & 0x01)) {
	    int rc = simbus_pci_wait(pci, 32, 0xffff);
	    if (rc != 0) {
		  printf("XXXX irq mask = %d\n", rc);
	    }
	    wait_time -= 1;
	    status = simbus_pci_read32(pci, BASE_BAR2_32+0x1000, 0);
      }

      printf("Status: 0x%08x\n", status);
      if (status & 0x01) {
	    printf("ERROR: Timeout waiting for DMA to function.\n");
	    fflush(stdout);

	    simbus_pci_wait(pci, 4, 0);
	    simbus_pci_end_simulation(pci);
      }

      for (int idx = 0 ; idx < 64 ; idx += 1) {
	    uint64_t addr = base1 + 0x1000 + 4*idx;
	    uint32_t val = simbus_pci_read32(pci, addr, 0);
	    if (val != idx) {
		  printf("ERROR: Got %u, expecting %d\n", val, idx);
	    }
      }

	// OK, have the device write the memory back to me
      simbus_pci_write32(pci, BASE_BAR2_32+0x1008, BASE_MEM_SPACE+4*64, 0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x100c, 0,              0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1010, 0x1000,         0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1004, 64*4,           0);
      simbus_pci_write32(pci, BASE_BAR2_32+0x1000, 0x11,           0);

      wait_time = 100;
      status = simbus_pci_read32(pci, BASE_BAR2_32+0x1000, 0);
      while (wait_time && (status & 0x01)) {
	    int rc = simbus_pci_wait(pci, 32, 0xffff);
	    if (rc != 0) {
		  printf("XXXX irq mask = %d\n", rc);
	    }
	    wait_time -= 1;
	    status = simbus_pci_read32(pci, BASE_BAR2_32+0x1000, 0);
      }

      printf("Status: 0x%08x\n", status);
      if (status & 0x01) {
	    printf("ERROR: Timeout waiting for DMA to function.\n");
	    fflush(stdout);

	    simbus_pci_wait(pci, 4, 0);
	    simbus_pci_end_simulation(pci);
      }

      for (int idx = 0 ; idx < 64 ; idx += 1) {
	    if (mem_space[64+idx] != mem_space[idx]) {
		  printf("ERROR: Got 0x%08lx back, expecting 0x%08lx\n",
			 mem_space[64+idx], mem_space[idx]);
		  fflush(stdout);
	    }
      }

      simbus_pci_wait(pci, 4, 0);
      simbus_pci_end_simulation(pci);

      printf("Done\n");
      return 0;
}

static uint32_t mem_need32(simbus_pci_t bus, uint64_t addr, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~MASK_MEM_SPACE;
      use_addr /= 4;

      uint32_t val = mem_space[use_addr];
#if 0
      printf("Memory read32 from 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);
#endif
      return val;
}

static void mem_recv32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~MASK_MEM_SPACE;
      use_addr /= 4;

      uint32_t be_mask = 0xffffffff;
      if (BEn & 1) be_mask &= 0xffffff00;
      if (BEn & 2) be_mask &= 0xffff00ff;
      if (BEn & 4) be_mask &= 0xff00ffff;
      if (BEn & 8) be_mask &= 0x00ffffff;
#if 0
      printf("Memory write32 to 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);
#endif
      mem_space[use_addr] = (val&be_mask) | (mem_space[use_addr]&~be_mask);
}
