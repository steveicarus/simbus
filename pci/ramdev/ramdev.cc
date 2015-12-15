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

/*
 * Usage: ramdev <flags>...
 * Flags:
 *
 *   -s <server-path>
 *      This is the address (pipe or tcp) to connect to the bus server
 *
 *   -n <name>
 *      Use this device name when connecting to the bus. The default
 *      name is "ramdev".
 *
 *   -i <ident>
 *      Use this value as the PCI identifier. The default is 0xffc112c5.
 *
 *   -d <path>
 *      Path to a debug log file. simbus_pci_debug messages are
 *      written to this log file.
 */

/*
 * After the command region, The BAR2 region has registers of a DMA
 * controller. The DMA controller supports a single linear read.
 *
 *    0x1000  : Cmd/Status
 *       [0] GO
 *       [1] FIFO Mode (0 - linear external addresses, 1 - fixed address)
 *       [2] Enable Count Interrupt
 *       [3] DAC64 addresses
 *       [4] Write flag (0 - DMA read, 1 - DMA write)
 *
 *    0x1008  : External Address (low 32 bits)
 *    0x100c  : External Address (high 32 bits, defaults to 0)
 *
 *    0x1010  : Memory Offset (bytes)
 *
 *    0x1004  : Transfer count (bytes)
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
static size_t memory_size = 0; // Size of memory in BYTES

const int BAR2_WORDS = 0x2000/4;
static uint32_t bar2_mem[BAR2_WORDS] = { 0 };

static uint32_t config_read32(simbus_pci_t, uint64_t, int);
static void config_recv32(simbus_pci_t, uint64_t, uint32_t, int);

static void do_bus_master_operations(simbus_pci_t bus);

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
      for (;;) {
	    int rc = simbus_pci_wait(bus, 0xffffffff, 0);
	    if (rc == SIMBUS_PCI_FINISHED) {
		  printf("Simulation finished.\n");
		  break;
	    }
	    if (rc == SIMBUS_PCI_BREAK) {
		  do_bus_master_operations(bus);
		  continue;
	    }
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

/*
 * The bar0_needXX functions implement reads from BAR0, the memory of
 * the device. The reads are free of side effects, so it is harmless
 * to ignore the BEn bits.
 */
static uint32_t bar0_need32(simbus_pci_t bus, uint64_t addr, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~bar0_mask;
      assert(use_addr < memory_size);
      use_addr /= 4;

      uint32_t val = memory_space[use_addr];
#if 0
      printf("Memory read32 from 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);
#endif
      return val;
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
#if 0
      printf("Memory write32 to 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);
#endif
      memory_space[use_addr] = (val&be_mask) | (memory_space[use_addr]&~be_mask);
}

/*
 * Reading BAR2 is idempotent.
 */
static uint32_t bar2_need32(simbus_pci_t bus, uint64_t addr, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~BAR2_MASK;
      use_addr /= 4;

      uint32_t val = bar2_mem[use_addr];

      printf("BAR2 read32 from 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);

      return val;
}

/*
 * Writing BAR2 may trigger actions. Perform the write, then perform
 * any actions that are triggered by the command.
 */
static void bar2_recv32(simbus_pci_t bus, uint64_t addr, uint32_t val, int BEn)
{
      uint32_t use_addr = addr;
      use_addr &= ~BAR2_MASK;
      use_addr /= 4;

      uint32_t be_mask = 0xffffffff;
      if (BEn & 1) be_mask &= 0xffffff00;
      if (BEn & 2) be_mask &= 0xffff00ff;
      if (BEn & 4) be_mask &= 0xff00ffff;
      if (BEn & 8) be_mask &= 0x00ffffff;

      printf("BAR2 write32 to 0x%04" PRIx64 ": 0x%08" PRIx32 " (BE#=0x%x)\n",
	     addr, val, BEn);
      fflush(stdout);

      bar2_mem[use_addr] = (val&be_mask) | (memory_space[use_addr]&~be_mask);

      switch (use_addr) {
	  case 0x1000/4: // Cmd/Status
	    simbus_pci_wait_break(bus);
	    break;
	  case 0x1004/4: // Transfer count
	    bar2_mem[use_addr] %= ~3; // Mask to multiple of 4
	    break;
	  case 0x1010/4: // Memory Offset
	    bar2_mem[use_addr] %= ~3; // Mask to multiple of 4
	    break;
	  default:
	    break;
      }
}


struct simbus_translation bar0_map;
struct simbus_translation bar2_map;

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
	    bar0_map.recv32 = &bar0_recv32;
	    bar0_map.base = config_mem[5];
	    bar0_map.base <<= 32;
	    bar0_map.base |= config_mem[4];
	    bar0_map.mask = 0xffffffff00000000ULL | bar0_mask;
	    simbus_pci_mem_xlate(bus, 0, &bar0_map);

	    bar2_map.need32 = &bar2_need32;
	    bar2_map.recv32 = &bar2_recv32;
	    bar2_map.base = config_mem[7];
	    bar2_map.base <<= 32;
	    bar2_map.base |= config_mem[6];
	    bar2_map.mask = 0xffffffff00000000ULL | BAR2_MASK;
	    simbus_pci_mem_xlate(bus, 2, &bar2_map);
      } else {
	    simbus_pci_mem_xlate(bus, 0, &bar0_map);
	    simbus_pci_mem_xlate(bus, 2, &bar2_map);
      }
}

static void do_bus_master_operations(simbus_pci_t bus)
{
#if 0
      printf("Break and do bus mastering...\n");
      fflush(stdout);
#endif
      if (bar2_mem[0x1000/4] & 0x01) { // DMA in progress
	      // While the transfer count > 0...
	    while (bar2_mem[0x1004/4] > 0) {
		  uint64_t addr = bar2_mem[0x100c/4];
		  addr <<= 32;
		  addr |= bar2_mem[0x1008/4];
		  uint32_t off = bar2_mem[0x1010/4];

		  bar2_mem[0x1010/4] += 4;
		  bar2_mem[0x1008/4] += 4;
		  bar2_mem[0x1004/4] -= 4;

		  if (bar2_mem[0x1000/4] & 0x10) { // DMA Write
			uint32_t val = 0xffffffff;
			if (off < memory_size) {
			      off /= 4;
			      val = memory_space[off];
			}
			simbus_pci_write32(bus, addr, val, 0);
		  } else { // DMA Read
			uint32_t val = simbus_pci_read32(bus, addr, 0);
			if (off < memory_size) {
			      off /= 4;
#if 0
			      printf("Write 0x%08lx to off=0x%x remain=%lu\n",
				     val, off, bar2_mem[0x1004/4]);
#endif
			      memory_space[off] = val;
			}
		  }
	    }

	      // Clear the GO bit
	    bar2_mem[0x1000/4] &= ~0x01;
      }
}
