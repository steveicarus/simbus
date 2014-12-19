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

# define WRITE_TID (0)

/*
 * Make a write TLP with these formats:
 *
 *        n...n is the data word count
 *        a...a is the address low 32 bits
 *        A...A is the address high 32 bits
 *        e...e is the first word byte enables
 *        E...E is the last word byte enables
 *        t...t is the transaction tag.
 *
 * 32bit address version:
 *    01000000 00000000 000000nn nnnnnnnn
 *    00000000 00000000 tttttttt EEEEeeee
 *    aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa  (32bit address)
 *    dddddddd dddddddd dddddddd dddddddd  (data[0])
 *    [... More data words ...]
 *
 * 32bit address version:
 *    01100000 00000000 000000nn nnnnnnnn
 *    00000000 00000000 tttttttt EEEEeeee
 *    AAAAAAAA AAAAAAAA AAAAAAAA AAAAAAAA
 *    aaaaaaaa aaaaaaaa aaaaaaaa aaaaaaaa  (32bit address)
 *    dddddddd dddddddd dddddddd dddddddd  (data[0])
 *    [... More data words ...]
 */
void simbus_xilinx_pcie_write(simbus_xilinx_pcie_t bus, uint64_t addr,
			      const uint32_t*data, size_t ndata,
			      int off, size_t len)
{
      uint32_t*tlp = calloc(ndata+4, sizeof(uint32_t));
      assert(tlp);

      uint32_t addr_h = (addr >> 32) & 0xffffffffUL;
      uint32_t addr_l = (addr >>  0) & 0xffffffffUL;

	/* Memory Write Request Fmt/type */
      if (addr_h==0)
	    tlp[0] = 0x40000000; /* MWr w/ 32bit addr */
      else
	    tlp[0] = 0x60000000; /* MWr w/ 64bit addr */

	/* Write the data count into the length field. */
      assert(ndata <= 0x3ff && ndata >= 1);
      tlp[0] |= ndata;

      tlp[1] = 0;

	/* Calculate the byte enables for the first and last
	   words. Use the offset and length values passed in to do the
	   calculations. */
      assert(off <= 3);
      assert(off+len <= 4*ndata);
      assert(off+4 > 4*(ndata-1));

      if (ndata == 1) {
	    uint32_t lmask = 0x0f & ~(0xf << len);
	    uint32_t omask = 0x0f & (0xf<<off);
	    uint32_t mask = lmask & omask;

	    tlp[1] |= mask;
      } else {
	    uint32_t omask = 0xf & (0xf << off);
	    size_t olen = (off + len) % 4;
	    if (olen == 0) olen = 4;

	    uint32_t lmask = 0xf0 & ~(0xf0 << olen);

	    tlp[1] |= lmask;
	    tlp[1] |= omask;
      }

	/* The write transaction does not require a completion, so
	   there is no need for a unique transaction id (TID) */
      tlp[1] |= WRITE_TID << 8;

	/* Write the address into the header. If the address has high
	   bits, then write the high bits first. */
      size_t ntlp = 2;
      if (addr_h)
	    tlp[ntlp++] = addr_h;
      tlp[ntlp++] = addr_l;

	/* Finally, load the data into the TLP. */
      for (size_t idx = 0 ; idx < ndata ; idx += 1)
	    tlp[ntlp++] = *data++;

	/* Send it! */
      __xilinx_pcie_send_tlp(bus, tlp, ntlp);

      free(tlp);
}
