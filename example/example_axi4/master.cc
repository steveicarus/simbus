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

# include  <simbus_axi4.h>
# include  <cstdio>
# include  <cassert>

int main(int argc, char*argv[])
{
      simbus_axi4_resp_t axi4_rc;
      simbus_axi4_t bus = simbus_axi4_connect(argv[1], "master", 32, 5, 4, 4);
      assert(bus);

      FILE*debug = fopen("master.log", "wt");
      if (debug) simbus_axi4_debug(bus, debug);

      fprintf(debug, "Wait 4 clocks...\n");
      fflush(debug);
      simbus_axi4_wait(bus, 4);

      fprintf(debug, "Reset bus...\n");
      fflush(debug);
      simbus_axi4_reset(bus, 8, 8);

      fprintf(debug, "Write some values...\n");
      fflush(debug);

	// Write some values...
      axi4_rc = simbus_axi4_write32(bus, 0x00, 0x00, 0x11111111, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x01, 0x00, 0x22222222, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x02, 0x00, 0x33333333, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x03, 0x00, 0x44444444, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x04, 0x00, 0x55555555, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x05, 0x00, 0x66666666, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x06, 0x00, 0x77777777, 0xf);
      axi4_rc = simbus_axi4_write32(bus, 0x07, 0x00, 0x88888888, 0xf);

      for (int addr = 0 ; addr < 8 ; addr += 1) {
	    uint32_t data;
	    axi4_rc = simbus_axi4_read32(bus, addr, 00, &data);
	    printf("Read back addr=0x%02x: data=0x%08x\n", addr, data);
      }

      simbus_axi4_end_simulation(bus);
      return 0;
}
