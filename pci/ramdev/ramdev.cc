/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
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
# include  <unistd.h>
# include  <simbus_pci.h>
# include  <inttypes.h>
# include  <stdio.h>
# include  <stdlib.h>
# include  <assert.h>

const char*name = "ramdev";

const uint32_t DEVICE_ID = 0xffc112c5;

static uint32_t bar0_mask = 0xffff0000;
const uint32_t BAR0_FLAGS = 0x0000000c;
const uint32_t BAR2_MASK  = 0xffffe000;
const uint32_t BAR2_FLAGS = 0x0000000c;

# define CONFIG_WORDS 16
static uint32_t config_mem[CONFIG_WORDS];

static uint32_t*memory_space = 0;
static size_t memory_size = 0;

static uint32_t config_read32(simbus_pci_t, uint64_t, int);
static void config_recv32(simbus_pci_t, uint64_t, uint32_t, int);

int main(int argc, char*argv[])
{
      const char*server = 0;
      const char*debug_path = 0;

	// Prefill the config space with default values.
      config_mem[ 0] = DEVICE_ID;
      config_mem[ 1] = 0x00000000;
      config_mem[ 2] = 0x05000000; // RAM memory
      config_mem[ 3] = 0x00000000;
      config_mem[ 4] = 0x00000000 | BAR0_FLAGS; // BAR0 (prefetchable, 64bit)
      config_mem[ 5] = 0x00000000; // BAR1 (high bits of BAR0)
      config_mem[ 6] = 0x00000000 | BAR2_FLAGS; // BAR2 (64bit)
      config_mem[ 7] = 0x00000000; // BAR3 (high bits of BAR2)
      config_mem[ 8] = 0x00000000; // BAR4 not implemented
      config_mem[ 9] = 0x00000000; // BAR5 not implemented
      config_mem[10] = 0x00000000; // CIS not implemented
      config_mem[11] = 0x00000000;
      config_mem[12] = 0x00000000;
      config_mem[13] = 0x00000000;
      config_mem[14] = 0x00000000;
      config_mem[15] = 0x00000000;

      while (int arg = getopt(argc, argv, "d:i:n:s:")) {
	    if (arg == -1) break;

	    switch (arg) {
		case 'd':
		  debug_path = optarg;
		  break;

		case 'i':
		  config_mem[0] = strtoul(optarg, 0, 0);
		  break;

		case 'n':
		  name = optarg;
		  break;

		case 's':
		  server = optarg;
		  break;

		default:
		  assert(0);
		  break;
	    }
      }

      memory_size = 1 + ~bar0_mask;
      memory_space = new uint32_t[memory_size/4];
      printf("Memory is %zu words\n", memory_size/4);
      for (int idx = 0 ; idx < memory_size/4 ; idx += 1)
	    memory_space[idx] = 0;

      simbus_pci_t bus = simbus_pci_connect(server, name);
      if (bus == 0) {
	    fprintf(stderr, "Unable to connect to server=%s with name=%s\n",
		    server, name);
	    return -1;
      }

      FILE*debug_fd = debug_path? fopen(debug_path, "w") : 0;
      if (debug_fd) simbus_pci_debug(bus, debug_fd);

	// Set up callback functions to handle transactions from the
	// bus. All the behavior of this device exists there.
      simbus_pci_config_need32(bus, &config_read32);
      simbus_pci_config_recv32(bus, &config_recv32);

	// Wait forever. We act like a PCI device by responding to
	// PCI transactions on the PCI bus, implemented in our
	// callbacks. When the simulation finishes, the pci_wait will
	// return, and we clean up.
      int rc = simbus_pci_wait(bus, 0xffffffff, 0);
      switch (rc) {
	  case SIMBUS_PCI_FINISHED:
	    printf("Simulation finished.\n");
	    break;
	  default:
	    printf("simbus_pci_wait returned rc=%d\n", rc);
	    break;
      }

	// Close our connection to the simbus server/PCI bus with this
	// command. Do *not* use the simbus_pci_end_simulation because
	// we are not a host device.
      simbus_pci_disconnect(bus);

      if (debug_fd) fclose(debug_fd);

      return 0;
}

static uint32_t bar0_need32(simbus_pci_t bus, uint64_t addr, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~bar0_mask;
      assert(use_addr < memory_size);
      use_addr /= 4;

      uint32_t val = memory_space[use_addr];

      printf("Memory read32 from 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);

      return val;
}

static uint64_t bar0_need64(simbus_pci_t bus, uint64_t addr, int BEn)
{
      return 0;
}

static void bar0_recv32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~bar0_mask;
      assert(use_addr < memory_size);
      use_addr /= 4;

      uint32_t be_mask = 0xffffffff;
      if (BEn & 1) be_mask &= 0xffffff00;
      if (BEn & 2) be_mask &= 0xffff00ff;
      if (BEn & 4) be_mask &= 0xff00ffff;
      if (BEn & 8) be_mask &= 0x00ffffff;

      printf("Memory write32 to 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);

      memory_space[use_addr] = (val&be_mask) | (memory_space[use_addr]&~be_mask);
}

static void bar0_recv64(simbus_pci_t bus, uint64_t addr, uint64_t val, int BEn)
{
}

struct simbus_translation bar0_map;

static uint32_t config_read32(simbus_pci_t bus, uint64_t addr, int BEn)
{
      uint32_t val = 0xffffffff;
      uint64_t waddr = addr/4;

      if (waddr < CONFIG_WORDS) {
	    val = config_mem[waddr];
      } else {
	    val = 0xffffffff;
      }

      printf("Config read from 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      return val;
}

static void config_recv32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn)
{
      printf("Config write to 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);

      uint64_t waddr = addr/4;
      if (waddr >= CONFIG_WORDS)
	    return;

      uint32_t be_mask = 0xffffffff;
      if (BEn & 1) be_mask &= 0xffffff00;
      if (BEn & 2) be_mask &= 0xffff00ff;
      if (BEn & 4) be_mask &= 0xff00ffff;
      if (BEn & 8) be_mask &= 0x00ffffff;
      switch (waddr) {
	  case 1: // StatusCommand
	    config_mem[1] = (val&0x0006&be_mask) | (config_mem[1]&0xffff&~be_mask);
	    break;
	  case 4: // BAR0
	    config_mem[waddr] = (val&be_mask) | (config_mem[waddr]&~be_mask);
	    config_mem[waddr] &= bar0_mask;
	    config_mem[waddr] |= BAR0_FLAGS;
	    break;
	  case 6: // BAR2
	    config_mem[waddr] = (val&be_mask) | (config_mem[waddr]&~be_mask);
	    config_mem[waddr] &= BAR2_MASK;
	    config_mem[waddr] |= BAR2_FLAGS;
	    break;
	  case 5: // BAR1
	  case 7: // BAR3
	    config_mem[waddr] = (val&be_mask) | (config_mem[waddr]&~be_mask);
	    break;
	  default:
	    break;
      }

      if (config_mem[1] & 0x0002) {
	    bar0_map.need32 = &bar0_need32;
	    bar0_map.need64 = &bar0_need64;
	    bar0_map.recv32 = &bar0_recv32;
	    bar0_map.recv64 = &bar0_recv64;
	    bar0_map.base = config_mem[5];
	    bar0_map.base <<= 32;
	    bar0_map.base |= config_mem[4];
	    bar0_map.mask = 0xffffffff00000000ULL | bar0_mask;
	    simbus_pci_mem_xlate(bus, 0, &bar0_map);
      } else {
	    simbus_pci_mem_xlate(bus, 0, &bar0_map);
      }
}
