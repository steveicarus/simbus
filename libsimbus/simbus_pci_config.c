/*
 * Copyright (c) 2008 Stephen Williams (steve@icarus.com)
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
# include  <stdio.h>
# include  <assert.h>

/*
 * Send the address and command...
 */
static void address_command(simbus_pci_t pci, uint64_t addr, unsigned cmd)
{
      int idx;
      pci->out_req64_n = BIT_1;
      pci->out_frame_n = BIT_0;
      pci->out_irdy_n  = BIT_1;
      pci->out_trdy_n  = BIT_Z;
      pci->out_stop_n  = BIT_Z;
      pci->out_devsel_n= BIT_Z;

      for (idx = 0 ; idx < 8 ; idx += 1)
	    pci->out_c_be[idx] = (cmd & (1<<idx)) ? BIT_1 : BIT_0;

	/* The address */
      uint64_t addr_tmp = addr;
      bus_bitval_t par = BIT_0;
      bus_bitval_t par64 = BIT_0;
      for (idx = 0 ; idx < 64 ; idx += 1) {
	    pci->out_ad[idx] = (addr_tmp&1)? BIT_1 : BIT_0;
	    addr_tmp >>= (uint64_t)1;
	    if (pci->out_ad[idx] == BIT_1) {
		  if (idx < 32)
			par = par==BIT_1? BIT_0 : BIT_1;
		  else
			par64 = par64==BIT_1? BIT_0 : BIT_1;
	    }
      }

	/* Clock the Command and address */
      __pci_half_clock(pci);
      __pci_half_clock(pci);

	/* Stage the IRDY# */
      pci->out_frame_n = BIT_1;
      pci->out_irdy_n  = BIT_0;
}

static void undrive_bus(simbus_pci_t pci)
{
      int idx;

      pci->out_frame_n = BIT_Z;
      pci->out_req64_n = BIT_Z;
      pci->out_irdy_n  = BIT_Z;

      for (idx = 0 ; idx < 8 ; idx += 1)
	    pci->out_c_be[idx] = BIT_Z;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;
}

uint32_t simbus_pci_config_read(simbus_pci_t pci, uint64_t addr)
{
      int idx;
	/* Arbitrate for the bus. This may return immediately if the
	   bus is parked here, or it may return after some clocks and
	   a REQ#/GNT# handshake. */
      __pci_request_bus(pci);

	/* Advance to the low phase of the PCI clock. We do this
	   because we want our outputs to change on the rising edges
	   of the PCI clock. */
      if (pci->pci_clk != BIT_1)
	    __pci_half_clock(pci);

      pci->out_req_n = BIT_1;

      address_command(pci, addr, 0xfa);

	/* Set up to read all 4 bytes of the word. (Config I/O is 32 bits.) */
      pci->out_c_be[0] = BIT_0;
      pci->out_c_be[1] = BIT_0;
      pci->out_c_be[2] = BIT_0;
      pci->out_c_be[3] = BIT_0;
      for (idx = 0 ; idx < 64 ; idx += 1) {
	    pci->out_ad[idx] = BIT_Z;
      }

	/* Clock the IRDY and BE#s (and PAR), and un-drive the AD bits. */
      __pci_half_clock(pci);
      assert(pci->pci_clk == BIT_0);

      if (pci->pci_devsel_n != BIT_0) { /* FAST decode... */
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
	    if (pci->pci_devsel_n != BIT_0) { /* SLOW decode... */
		  __pci_half_clock(pci);
		  __pci_half_clock(pci);
		  if (pci->pci_devsel_n != BIT_0) { /* Subtractive decode... */
			__pci_half_clock(pci);
			__pci_half_clock(pci);
			if (pci->pci_devsel_n != BIT_0) { /* give up. */
			      undrive_bus(pci);
			      fprintf(stderr, "simbus_pci_config_read: "
				      "No response to addr=0x%x\n", addr);
			      return 0xffffffff;
			}
		  }
	    }
      }

      pci->out_frame_n = BIT_1;
      pci->out_req64_n = BIT_1;

	/* Wait for TRDY# */
      int count = 60;
      while (pci->pci_trdy_n != BIT_0) {
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
	    count -= 60;
	    assert(count > 0);
      }

	/* Collect the result read from the device. */
      uint32_t result = 0;
      for (idx = 0 ; idx < 32 ; idx += 1) {
	    if (pci->pci_ad[idx] != BIT_0)
		  result |= 1 << idx;
      }

	/* Release all the signals I've been driving. */
      undrive_bus(pci);

	/* This clocks the drivers to the next state, and clocks in
	   the final parity from the target. */
      __pci_half_clock(pci);
      __pci_half_clock(pci);

	/* XXXX Here we should check the pci_par parity bit */

	/* Done. Return the result. */
      return result;
}

void simbus_pci_config_write(simbus_pci_t pci, uint64_t addr, uint32_t val, int BEn)
{
      int idx;
	/* Arbitrate for the bus. This may return immediately if the
	   bus is parked here, or it may return after some clocks and
	   a REQ#/GNT# handshake. */
      __pci_request_bus(pci);

	/* Advance to the low phase of the PCI clock. We do this
	   because we want our outputs to change on the rising edges
	   of the PCI clock. */
      if (pci->pci_clk != BIT_1)
	    __pci_half_clock(pci);

      pci->out_req_n = BIT_1;

      address_command(pci, addr, 0xfb);

      pci->out_c_be[0] = BEn&1 ? BIT_1 : BIT_0;
      pci->out_c_be[1] = BEn&2 ? BIT_1 : BIT_0;
      pci->out_c_be[2] = BEn&4 ? BIT_1 : BIT_0;
      pci->out_c_be[3] = BEn&5 ? BIT_1 : BIT_0;
      pci->out_c_be[4] = BIT_1;
      pci->out_c_be[5] = BIT_1;
      pci->out_c_be[7] = BIT_1;
      pci->out_c_be[8] = BIT_1;

      for (idx = 0 ; idx < 32 ; idx += 1, val >>= 1)
	    pci->out_ad[idx] = (val&1) ? BIT_1 : BIT_0;
      for (idx = 32 ; idx < 64; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

	/* Clock the IRDY and BE#s (and PAR). */
      __pci_half_clock(pci);
      assert(pci->pci_clk == BIT_0);

      if (pci->pci_devsel_n != BIT_0) { /* FAST */
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
	    if (pci->pci_devsel_n != BIT_0) { /* MED */
		  __pci_half_clock(pci);
		  __pci_half_clock(pci);
		  if (pci->pci_devsel_n != BIT_0) { /* SLOW */
			__pci_half_clock(pci);
			__pci_half_clock(pci);
			if (pci->pci_devsel_n != BIT_0) { /* subtract */
			      fprintf(stderr, "simbus_pci_config_write: "
				      "No response to addr=0x%x\n", addr);
			      undrive_bus(pci);
			      return;
			}
		  }
	    }
      }

      while (pci->pci_trdy_n != BIT_0) {
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
      }

      undrive_bus(pci);
}
