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
# include  <simbus_xilinx_pcie.h>
# include  <inttypes.h>
# include  <cstdio>
# include  <cassert>


int main(int argc, char*argv[])
{
      simbus_xilinx_pcie_t bus = simbus_xilinx_pcie_connect(argv[1], "root");

      FILE*debug = fopen("root.log", "wt");
      if (debug) simbus_xilinx_pcie_debug(bus, debug);

      printf("Wait 4 clocks...\n");
      fflush(stdout);
      simbus_xilinx_pcie_wait(bus, 4);

      printf("Reset bus...");
      fflush(stdout);
      simbus_xilinx_pcie_reset(bus, 1, 8);

      printf("Wait 4 more clocks...\n");
      fflush(stdout);
      simbus_xilinx_pcie_wait(bus, 4);

      printf("Read device id from PCIe config space...\n");
      uint32_t id;
      simbus_xilinx_pcie_cfg_read32(bus, 0x0000, 0x0000, &id);
      printf("  Id: 0x%08" PRIx32 "\n", id);
      fflush(stdout);

      simbus_xilinx_pcie_end_simulation(bus);
      if (debug) fclose(debug);
}
