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

# include  "simbus_pci.h"
# include  "simbus_pci_priv.h"
# include  "simbus_priv.h"
# include  <unistd.h>
# include  <limits.h>
# include  <stdlib.h>
# include  <string.h>
# include  <stdio.h>
# include  <assert.h>

static int get_command(simbus_pci_t pci)
{
      int rc = 0;
      if (pci->pci_c_be[3] == BIT_1)
	    rc |= 0x08;
      if (pci->pci_c_be[2] == BIT_1)
	    rc |= 0x04;
      if (pci->pci_c_be[1] == BIT_1)
	    rc |= 0x02;
      if (pci->pci_c_be[0] == BIT_1)
	    rc |= 0x01;

      return rc;
}

static uint64_t get_addr32(simbus_pci_t pci)
{
      uint64_t rc = 0;
      uint64_t mask = 1;
      int idx;
      for (idx = 0, mask=1 ; idx < 32 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] == BIT_1) rc |= mask;
      }

      return rc;
}

static int get_idsel(simbus_pci_t pci)
{
      assert(pci->ident < 16);
      if (pci->pci_ad[16 + pci->ident] == BIT_1)
	    return 1;
      else
	    return 0;
}

/*
 * An attempt to read from the config space leads me to complete the
 * config cycle.
 */
static void do_target_config_read(simbus_pci_t pci)
{
      uint64_t addr = get_addr32(pci) & 0x0000ffff;
      uint32_t val = 0xffffffff;

      if (pci->config_need32)
	    val = pci->config_need32(pci, addr, 0);

	/* Emit DEVSEL# but drive TRDY# high to insert a turnaround
	   cycle for the AD bus. */
      pci->out_devsel_n = BIT_0;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

      __pci_half_clock(pci);
      __pci_half_clock(pci);

	/* Drive TRDY# and the AD. */
      pci->out_trdy_n = BIT_0;

      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1, val >>= 1)
	    pci->out_ad[idx] = (val&1) ? BIT_1 : BIT_0;
      for (idx = 32 ; idx < 64; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

	/* Wait for the master to read the data. */
      do {
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
      } while (pci->pci_irdy_n == BIT_1);

      pci->out_devsel_n = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;
	/* This is turnaround time for AD. */
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

      __pci_half_clock(pci);
      __pci_half_clock(pci);

	/* Release the bus and settle. */
      __undrive_bus(pci);
      __pci_half_clock(pci);
      __pci_half_clock(pci);
}

static void do_target_config_write(simbus_pci_t pci)
{
      assert(pci->pci_clk == BIT_1);

      uint64_t addr = get_addr32(pci) & 0x0000ffff;
	/* Emit DEVSEL# and TRDY# . */
      pci->out_devsel_n = BIT_0;
      pci->out_trdy_n = BIT_0;
      pci->out_stop_n = BIT_1;

	/* Wait for the master to be ready with the data. */
      do {
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
      } while (pci->pci_irdy_n == BIT_1);

      uint32_t val = get_addr32(pci);
      int BEn = 0;
      if (pci->pci_c_be[0] == BIT_1) BEn |= 1;
      if (pci->pci_c_be[1] == BIT_1) BEn |= 2;
      if (pci->pci_c_be[2] == BIT_1) BEn |= 4;
      if (pci->pci_c_be[3] == BIT_1) BEn |= 8;

      if (pci->config_recv32)
	    pci->config_recv32(pci, addr, val, BEn);

      pci->out_devsel_n = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

      __pci_half_clock(pci);
      __pci_half_clock(pci);

      pci->out_devsel_n = BIT_Z;
      pci->out_trdy_n   = BIT_Z;
      pci->out_stop_n   = BIT_Z;
}

/*
 * This function is called on the rising edge of any PCI clock. If
 * this device supports target operations, then handle them here.
 */
void __pci_target_state_machine(simbus_pci_t pci)
{
      switch (pci->target_state) {

	  case TARG_IDLE:
	    if (pci->pci_frame_n == BIT_0) {
		  int command = get_command(pci);

		  if (command == 0x0a && get_idsel(pci)) {
			do_target_config_read(pci);

		  } else if (command == 0x0b && get_idsel(pci)) {
			do_target_config_write(pci);

		  } else {
			pci->target_state = TARG_BUS_BUSY;
		  }
	    }
	    break;

	  case TARG_BUS_BUSY:
	    if (pci->pci_frame_n == BIT_1) {
		  pci->target_state = TARG_IDLE;
	    }
	    break;
      }
}
