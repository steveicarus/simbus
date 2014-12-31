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

# define __STDC_FORMAT_MACROS
# include  <simbus_pcie_tlp.h>
# include  <inttypes.h>
# include  <cstdio>
# include  <cassert>

uint32_t address_space[16*1024/4];

static uint32_t mask_from_be(int be)
{
      uint32_t mask = 0;
      if (be&8) mask |= 0xff000000;
      if (be&4) mask |= 0x00ff0000;
      if (be&2) mask |= 0x0000ff00;
      if (be&1) mask |= 0x000000ff;
      return mask;
}

static inline uint32_t& masked_assign(uint32_t&l, uint32_t val, uint32_t mask)
{
      l = (l&~mask) | (val&mask);
      return l;
}

static void write_fun (simbus_pcie_tlp_t bus,
		       simbus_pcie_tlp_cookie_t,
		       uint64_t addr,
		       const uint32_t*data, size_t ndata,
		       int be0, int beN)
{
      assert(addr%4 == 0);
      addr /= 4;

      masked_assign(address_space[addr+0], data[0], mask_from_be(be0));

      for (size_t idx = 1 ; idx+1 < ndata ; idx += 1)
	    address_space[addr+idx] = data[idx];

      if (ndata > 1)
	    masked_assign(address_space[addr+ndata-1], data[ndata-1], mask_from_be(beN));
}

static void read_fun (simbus_pcie_tlp_t bus,
		      simbus_pcie_tlp_cookie_t,
		      uint64_t addr,
		      uint32_t*data, size_t ndata,
		      int, int)
{
      assert(addr%4 == 0);
      addr /= 4;

      for (size_t idx = 0 ; idx < ndata ; idx += 1)
	    data[idx] = address_space[addr+idx];

}


int main(int argc, char*argv[])
{
      simbus_pcie_tlp_t bus = simbus_pcie_tlp_connect(argv[1], "root");

      simbus_pcie_tlp_write_handle(bus, &write_fun, 0);
      simbus_pcie_tlp_read_handle(bus, &read_fun, 0);

      FILE*debug = fopen("root.log", "wt");
      if (debug) simbus_pcie_tlp_debug(bus, debug);

      printf("Wait 4 clocks...\n");
      fflush(stdout);
      simbus_pcie_tlp_wait(bus, 4);

      printf("Reset bus...");
      fflush(stdout);
      simbus_pcie_tlp_reset(bus, 1, 8);

      printf("Wait 4 more clocks...\n");
      fflush(stdout);
      simbus_pcie_tlp_wait(bus, 4);

      printf("Read device id from PCIe config space...\n");
      uint32_t id;
      simbus_pcie_tlp_cfg_read32(bus, 0x0000, 0x0000, &id);
      printf("  Id: 0x%08" PRIx32 "\n", id);
      fflush(stdout);

      simbus_pcie_tlp_cfg_write32(bus, 0x0000, 0x0010, 0xaaaaaaaa);
      uint32_t val;
      simbus_pcie_tlp_cfg_read32(bus, 0x0000, 0x0010, &val);
      printf("  BAR0: 0x%08" PRIx32 "\n", val);
      fflush(stdout);

      printf("Write some sample data...\n");

      uint32_t buf[4];
      buf[0] = 0xaaaaaaaa;
      buf[1] = 0x55555555;
      buf[2] = 0x12345678;
      buf[3] = 0x87654321;

      simbus_pcie_tlp_write(bus, 0x00000010, buf, 4, 0, 16);
      simbus_pcie_tlp_wait(bus, 4);

      printf("Read sample data back...\n");
      fflush(stdout);
      uint32_t dst[4];
      simbus_pcie_tlp_read(bus, 0x00000010, dst, 4, 0, 16);

      for (size_t idx = 0 ; idx < 4 ; idx += 1) {
	    printf("    0x%04x:  0x%08" PRIx32 "  (s.b. 0x%08" PRIx32 ")\n",
		   0x10+4*idx, dst[idx], buf[idx]);
      }

      printf("Wait 4 more clocks...\n");
      fflush(stdout);
      simbus_pcie_tlp_wait(bus, 4);

      printf("Write a DMA command. This should cause a DMA write.\n");
      buf[0] = 0x00000001;
      buf[1] = 0x00000020;
      simbus_pcie_tlp_write(bus, 0x00000000, buf, 2, 0, 8);

      printf("Wait 256 more clocks...\n");
      fflush(stdout);
      simbus_pcie_tlp_wait(bus, 256);

      printf("Check memory as 0x20:\n");
      for (size_t idx = 8 ; idx < 12 ; idx += 1) {
	    printf("    0x%04x:  0x%08" PRIx32 "\n",
		   idx+4, address_space[idx]);
      }

      simbus_pcie_tlp_end_simulation(bus);
      if (debug) fclose(debug);
}
