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

# include  <simbus_p2p.h>
# include  <stdio.h>
# include  <assert.h>

int main(int argc, char*argv[])
{
      simbus_p2p_t bus = simbus_p2p_connect(argv[1], "master", 16, 16);
      assert(bus);

	/* Get the simulation started. */
      uint32_t val = 0;
      simbus_p2p_out(bus, &val);
      simbus_p2p_clock_posedge(bus, 4);

      uint32_t res = 0;
      simbus_p2p_in(bus, &res);
      assert(res == 0xffff);

	/* Hold the clock to the slave and make sure that the slave
	   doesn't respond. */
      simbus_p2p_clock_mode(bus, SIMBUS_P2P_CLOCK_STOP1);
      val = 1;
      simbus_p2p_out(bus, &val);
      simbus_p2p_clock_posedge(bus, 4);

      simbus_p2p_in(bus, &res);
      assert(res == 0xffff);

	/* Resume the clock and make sure it catched up. */
      simbus_p2p_clock_mode(bus, SIMBUS_P2P_CLOCK_RUN);
      simbus_p2p_clock_posedge(bus, 2);
      simbus_p2p_in(bus, &res);
      if (res != 0xfffe) {
	    simbus_p2p_end_simulation(bus);
	    return 1;
      }
      assert(res == 0xfffe);

      uint32_t prev;
      for (prev = val, val = 0 ; val < 0x04000 ; prev=val, val += 1) {
	    simbus_p2p_clock_negedge(bus, 1);
	    simbus_p2p_out(bus, &val);
	    simbus_p2p_clock_posedge(bus, 1);
	    simbus_p2p_in(bus, &res);

	    printf("Val=0x%04x, prev=0x%04x --> result=0x%04x\n", val, prev, res);
	    assert(res == (0xffff & (~prev)));
      }

      simbus_p2p_clock_posedge(bus, 4);

      simbus_p2p_end_simulation(bus);
      return 0;
}
