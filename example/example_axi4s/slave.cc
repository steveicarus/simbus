/*
 * Copyright (c) Stephen Williams (steve@icarus.com)
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

/*
 * This is an example of a simple slave device. Implement a
 * simple memory.
 */
# include  <simbus_axi4s.h>
# include  <cassert>

static uint32_t memory_space[8];

static simbus_axi4_resp_t slave_write32(simbus_axi4_t bus, uint64_t addr, int prot, uint32_t data)
{
      printf("WRITE32: @0x%02" PRIx64 " = 0x%08" PRIx32 "\n", addr, data);
      assert(addr%4 == 0);
      assert(addr/4  < 8);
      memory_space[addr/4] = data;
      return SIMBUS_AXI4_RESP_OKAY;
}

static simbus_axi4_resp_t slave_write8(simbus_axi4_t bus, uint64_t addr, int prot, uint8_t data)
{
      printf("WRITE8: @0x%02" PRIx64 " = 0x%02" PRIx8 "\n", addr, data);

      size_t shift = 8 * (addr%4);
      uint32_t mask = 0xff << shift;

      memory_space[addr/4] = (data<<shift) | (memory_space[addr/4] & ~mask);

      return SIMBUS_AXI4_RESP_OKAY;
}

static simbus_axi4_resp_t slave_read32(simbus_axi4_t bus, uint64_t addr, int prot, uint32_t*data)
{
      assert(addr%4 == 0);
      assert(addr/4  < 8);
      *data = memory_space[addr/4];
      printf("READ32: @0x%02" PRIx64 " = 0x%08" PRIx32 "\n", addr, *data);
      return SIMBUS_AXI4_RESP_OKAY;
}

static simbus_axi4_resp_t slave_read8(simbus_axi4_t bus, uint64_t addr, int prot, uint8_t*data)
{
      assert(addr/4  < 8);
      size_t shift = 8 * (addr%4);
      *data = (memory_space[addr/4] >> shift) & 0xff;
      printf("READ8: @0x%02" PRIx64 " = 0x%08" PRIx8 "\n", addr, *data);
      return SIMBUS_AXI4_RESP_OKAY;
}

static const struct simbus_axi4s_slave_s slave_dev = {
 write64: 0,
 write32: &slave_write32,
 write16: 0,
 write8 : &slave_write8,
 read64 : 0,
 read32 : &slave_read32,
 read16 : 0,
 read8  : &slave_read8
};

int main(int argc, char*argv[])
{
	// data_width = 32 bits (4 bytes)
	// addr_width = 5 bits  (32 bytes, 8 words)
	// wid_width (write transaction id) = 4 bits
	// rid_width (read transaction id) = 4 bits
      simbus_axi4_t bus = simbus_axi4_connect(argv[1], "slave",
					      32, 5, 4, 4, 0);
      assert(bus);

      FILE*debug = fopen("slave.log", "wt");
      if (debug) simbus_axi4_debug(bus, debug);

	// This is the main for the device.
      int rc = simbus_axi4_slave(bus, &slave_dev);

      if (debug) fprintf(debug, "slave.cc: simbus_axu4_slave returns rc=%d\n", rc);

	// After the bus is stopped, disconnect and we are done.
      simbus_axi4_disconnect(bus);

      if (debug) fclose(debug);

      return 0;
}
