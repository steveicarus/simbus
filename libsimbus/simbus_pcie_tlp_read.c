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

# include  "simbus_pcie_tlp.h"
# include  "simbus_pcie_tlp_priv.h"
# include  <stdlib.h>
# include  <string.h>
# include  <assert.h>

/*
 * Make a read TLP with these formats:
 *
 *        n...n is the data word count
 *        a...a is the address low 32 bits
 *        A...A is the address high 32 bits
 *        e...e is the first word byte enables
 *        E...E is the last word byte enables
 *        t...t is the transaction tag.
 *
 * 32bit address version:
 *    00000000 00000000 000000nn nnnnnnnn
 *    00000000 00000000 tttttttt EEEEeeee
 *    aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa  (32bit address)
 *
 * 64bit address version:
 *    00100000 00000000 000000nn nnnnnnnn
 *    00000000 00000000 tttttttt EEEEeeee
 *    AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA  (high bits of address)
 *    aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa  (low bits of address)
 */
void simbus_pcie_tlp_read(simbus_pcie_tlp_t bus, uint64_t addr,
			  uint32_t*data, size_t ndata,
			      int off, size_t len)
{
      uint32_t tlp[4];

      uint32_t addr_h = (addr >> 32) & 0xffffffffUL;
      uint32_t addr_l = (addr >>  0) & 0xffffffffUL;

	/* Memory Write Request Fmt/type */
      if (addr_h==0)
	    tlp[0] = 0x00000000; /* MRd w/ 32bit addr */
      else
	    tlp[0] = 0x20000000; /* MRd w/ 64bit addr */

	/* Write the data count into the length field. */
      assert(ndata <= 0x3ff && ndata >= 1);
      tlp[0] |= ndata;

      tlp[1] = 0;

	/* Calculate the byte enables for the first and last
	   words. Use the offset and length values passed in to do the
	   calculations. */
      assert(off <= 3);
      assert(off+len <= 4*ndata);

      assert(off==0 && len==ndata*4);
      if (ndata == 1)
	    tlp[1] |= 0x0f;
      else
	    tlp[1] |= 0xff;

	/* The write transaction does not require a completion, so
	   there is no need for a unique transaction id (TID) */
      uint8_t use_tag = __pcie_tlp_choose_tag(bus);
      tlp[1] |= use_tag << 8;

	/* Write the address into the header. If the address has high
	   bits, then write the high bits first. */
      size_t ntlp = 2;
      if (addr_h)
	    tlp[ntlp++] = addr_h;
      tlp[ntlp++] = addr_l;

	/* Send it! */
      __pcie_tlp_send_tlp(bus, tlp, ntlp);

	/* Wait for a response to the read. */
      while (bus->completions[use_tag] == 0) {
	    __pcie_tlp_next_posedge(bus);
      }

      uint32_t*ctlp = bus->completions[use_tag];
      bus->completions[use_tag] = 0;

	/*
	 * We are expecting a completion w/ data, with ndata words
	 * of data. Note that the data is big-endian, so needs to be
	 * swapped to native byte order.
	 *
	 *        n...n is the data word count, and should be 1.
	 *        d...d is the data
	 *        s...s is the completion status
	 *        t...t is the requestor tag, and should be use_tag
	 *
	 *    01001010 ........ ......nn nnnnnnnn
	 *    ........ ........ sss..... ........
	 *    ........ ........ tttttttt ........
	 *    dddddddd dddddddd dddddddd dddddddd
	 */
      size_t ndata2 = ctlp[0] & 0x03ff;
      int status = (ctlp[1] >> 13) & 0x07;

      switch (status) {
	  case 0: /* Successful Completion */
	    assert(ndata == ndata2);
	    for (size_t idx = 0 ; idx < ndata ; idx += 1) {
		  uint32_t tmp = ctlp[3+idx];
		  uint32_t val = 0;
		  for (size_t bdx = 0 ; bdx < 4 ; bdx += 1) {
			val = (val<<8) | tmp & 0xff;
			tmp >>= 8;
		  }
		  data[idx] = val;
	    }
	    break;
	  default: /* Unexpected completion type */
	    for (size_t idx = 0 ; idx < ndata ; idx += 1)
		  data[idx] = 0xffffffff;
	    break;
      }

      free(ctlp);
}
