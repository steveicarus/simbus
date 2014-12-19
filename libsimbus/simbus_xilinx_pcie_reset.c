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


# include  "simbus_xilinx_pcie.h"
# include  "simbus_xilinx_pcie_priv.h"
# include  <stdlib.h>
# include  <string.h>
# include  <assert.h>

void simbus_xilinx_pcie_reset(simbus_xilinx_pcie_t bus, unsigned width, unsigned settle)
{
      assert(width > 0);

      bus->user_reset_out = BIT_1;
      simbus_xilinx_pcie_wait(bus, width);

      bus->user_reset_out = BIT_0;
      if (settle > 0) simbus_xilinx_pcie_wait(bus, settle);
}
