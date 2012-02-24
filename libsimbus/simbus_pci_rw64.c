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


uint64_t simbus_pci_read64(simbus_pci_t pci, uint64_t addr, int BEn)
{
      uint64_t val = UINT64_C(0xffffffffffffffff);
      uint64_t valx= UINT64_C(0xffffffffffffffff);
      int retry = 1;
      int idx;
      int rc;

      while (retry) {
	    __pci_request_bus(pci);

	    pci->out_req_n = BIT_1;

	    __address_command(pci, addr, 0xf6, 1, 0);

	      /* Collect the BE# bits. */
	    pci->out_c_be[0] = BEn&0x01? BIT_1 : BIT_0;
	    pci->out_c_be[1] = BEn&0x02? BIT_1 : BIT_0;
	    pci->out_c_be[2] = BEn&0x04? BIT_1 : BIT_0;
	    pci->out_c_be[3] = BEn&0x08? BIT_1 : BIT_0;
	    pci->out_c_be[4] = BEn&0x10? BIT_1 : BIT_0;
	    pci->out_c_be[5] = BEn&0x20? BIT_1 : BIT_0;
	    pci->out_c_be[6] = BEn&0x40? BIT_1 : BIT_0;
	    pci->out_c_be[7] = BEn&0x80? BIT_1 : BIT_0;
	      /* Make sure address lines are undriven */
	    for (idx = 0 ; idx < 64 ; idx += 1) {
		  pci->out_ad[idx] = BIT_Z;
	    }

	    if (__wait_for_devsel(pci) < 0) {
		    /* Master abort */
		  return UINT64_C(0xffffffffffff);
	    }

	    rc = __wait_for_read(pci, &val, &valx);
	    if (rc == GPCI_TARGET_RETRY) {
		    /* Release all the signals I've been driving. */
		  __undrive_bus(pci);

		    /* This clocks the drivers to the next state, and
		       clocks in the final parity from the target. */
		  __pci_next_posedge(pci);
		  continue;
	    }

	    assert(rc >= 0);

	    if (pci->out_frame_n != BIT_0) {
		  retry = 0;
		  break;
	    }

	    pci->out_frame_n = BIT_1;
	    pci->out_req64_n = BIT_1;
	    __pci_next_posedge(pci);

	      /* Target did not ACK64#, so we are reading a burst of 2
		 32bit words. Read the next word and terminate the
		 burst */
	    uint64_t hival, hivalx;
	    rc = __wait_for_read(pci, &hival, &hivalx);
	    assert(rc >= 0);

	    val |= hival << 32;
	    break;
      }

	/* Release all the signals I've been driving. */
      __undrive_bus(pci);
      __pci_next_posedge(pci);

      return val;
}

void simbus_pci_write64(simbus_pci_t pci, uint64_t addr, uint64_t val, int BEn)
{
      int rc;

      __pci_request_bus(pci);

      pci->out_req_n = BIT_1;

      __address_command(pci, addr, 0xf7, 1, 0);

      __setup_for_write(pci, val, BEn, 1);

      if ( (rc = __wait_for_devsel(pci)) < 0)
	    return;

	/* Wait for the target to TRDY# in order to clock the data out. */
      while (pci->pci_trdy_n != BIT_0) {
	    __pci_next_posedge(pci);
      }

	/* If the target accepted the 64bit transaction, then the
	   active ACK64# will have caused me to set FRAME#==1. So I
	   know here that I've clocked out the entire 64bit word and
	   I'm done. */
      if (pci->out_frame_n == BIT_1) {
	      /* Release the bus and settle. */
	    __undrive_bus(pci);
	    __pci_next_posedge(pci);
	    return;
      }

	/* The target only acknowledged a 32bit write, to complete the
	   64bit write with another 32bit word. */
      pci->out_frame_n = BIT_1;
      pci->out_req64_n = BIT_1;
      __setup_for_write(pci, val>>32, BEn>>4, 0);

	/* Wait for the target to TRDY# in order to clock the data out. */
      while (pci->pci_trdy_n != BIT_0) {
	    __pci_next_posedge(pci);
      }

	/* Release the bus and settle. */
      __undrive_bus(pci);
      __pci_next_posedge(pci);
}

int simbus_pci_write64b(simbus_pci_t pci, uint64_t addr,
			 const uint64_t*val, int words,
			 int BEFn, int BELn)
{
      assert(words > 0);
      assert(words > 1 || BEFn==BELn);

      int idx;
      for (idx = 0 ; idx < words ; idx += 1, addr += 4) {
	    int use_BEn = 0;
	    if (idx == 0) use_BEn = BEFn;
	    else if (idx+1 == words) use_BEn = BELn;

	    simbus_pci_write64(pci, addr, val[idx], use_BEn);
      }

      return words;
}
