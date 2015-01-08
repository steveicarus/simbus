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
 * Configuration messages all expect immediate completions, so build a
 * wait for the response completion into these commands.
 */

/*
 * Make a write TLP with these formats:
 *
 *        n...n is the data word count
 *        a...a is the address low 32 bits
 *        e...e is the first word byte enables
 *        f...f is the bus/device/function address
 *        r...r is the requester id.
 *        t...t is the transaction tag.
 *
 *    01000100 00000000 00000000 00000001  (always transfer 1 word)
 *    rrrrrrrr rrrrrrrr tttttttt 0000eeee
 *    ffffffff ffffffff 0000aaaa aaaaaaaa  (12bit address)
 *    dddddddd dddddddd dddddddd dddddddd  (data)
 *
 */
void simbus_pcie_tlp_cfg_write32(simbus_pcie_tlp_t bus,
				    uint16_t bus_devfn, uint16_t addr,
				    uint32_t val)
{
      uint32_t tlp[4];
      uint8_t use_tag = bus->tlp_next_tag;

      tlp[0] = 0x44000001;
      tlp[1] = 0x0000000f | (use_tag << 8) | (bus->request_id << 16);
      tlp[2] = (bus_devfn << 16) | (addr & 0x0ffc);
      tlp[3] = val;

      bus->tlp_next_tag = (bus->tlp_next_tag + 1) % 32;

      __pcie_tlp_send_tlp(bus, tlp, 4);

	/* Wait for the completion to come back. */
      while (bus->completions[use_tag] == 0) {
	    __pcie_tlp_next_posedge(bus);
      }

      if (bus->debug) {
	    fprintf(bus->debug, "Write32 completion:\n");
      }

      free(bus->completions[use_tag]);
      bus->completions[use_tag] = 0;
}

void simbus_pcie_tlp_cfg_write16(simbus_pcie_tlp_t bus,
				    uint16_t bus_devfn, uint16_t addr,
				    uint16_t val)
{
      uint32_t tlp[4];
      size_t shift = addr%4;
      uint8_t use_tag = bus->tlp_next_tag;

      tlp[0] = 0x44000001;
      tlp[1] = 0x00000000 | (use_tag << 8) | (3 << shift) | (bus->request_id << 16);
      tlp[2] = (bus_devfn << 16) | (addr & 0x0ffc);
      tlp[3] = val << 8*shift;

      bus->tlp_next_tag = (bus->tlp_next_tag + 1) % 32;

      __pcie_tlp_send_tlp(bus, tlp, 4);

      while (bus->completions[use_tag] == 0) {
	    __pcie_tlp_next_posedge(bus);
      }

      free(bus->completions[use_tag]);
      bus->completions[use_tag] = 0;

}

void simbus_pcie_tlp_cfg_write8(simbus_pcie_tlp_t bus,
				    uint16_t bus_devfn, uint16_t addr,
				    uint8_t val)
{
      uint32_t tlp[4];
      size_t shift = addr%4;
      uint8_t use_tag = bus->tlp_next_tag;

      tlp[0] = 0x44000001;
      tlp[1] = 0x00000000 | (use_tag << 8) | (1 << shift) | (bus->request_id << 16);
      tlp[2] = (bus_devfn << 16) | (addr & 0x0ffc);
      tlp[3] = val << 8*shift;

      bus->tlp_next_tag = (bus->tlp_next_tag + 1) % 32;

      __pcie_tlp_send_tlp(bus, tlp, 4);

      while (bus->completions[use_tag] == 0) {
	    __pcie_tlp_next_posedge(bus);
      }

      free(bus->completions[use_tag]);
      bus->completions[use_tag] = 0;
}

/*
 * Make a read TLP with these formats:
 *
 *        n...n is the data word count
 *        a...a is the address low 32 bits
 *        e...e is the first word byte enables
 *        f...f is the bus/device/function address
 *        t...t is the transaction tag.
 *
 *    00000100 00000000 00000000 00000001  (always transfer 1 word)
 *    00000000 00000000 tttttttt 0000eeee
 *    ffffffff ffffffff 0000aaaa aaaaaaaa  (12bit address)
 *
 */
void simbus_pcie_tlp_cfg_read32(simbus_pcie_tlp_t bus,
				   uint16_t bus_devfn, uint16_t addr,
				   uint32_t*val)
{
      uint32_t tlp[4];
      uint8_t use_tag = __pcie_tlp_choose_tag(bus);

      tlp[0] = 0x04000001;
      tlp[1] = 0x00000000 | (use_tag << 8) | 0xf | (bus->request_id << 16);
      tlp[2] = (bus_devfn << 16) | (addr & 0x0ffc);

      __pcie_tlp_send_tlp(bus, tlp, 3);

      while (bus->completions[use_tag] == 0) {
	    __pcie_tlp_next_posedge(bus);
      }

      uint32_t*ctlp = bus->completions[use_tag];
      bus->completions[use_tag] = 0;

	/*
	 * We are expecting a completion w/ data, with a single word
	 * of data.
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
      size_t ndata = ctlp[0] & 0x03ff;
      int status = (ctlp[1] >> 13) & 0x07;

      switch (status) {
	  case 0: /* Successful Completion */
	    assert(ndata == 1);
	    val[0] = ctlp[3];
	    break;
	  default: /* Unexpected completion type */
	    val[0] = 0xffffffff;
	    break;
      }

      free(ctlp);
}
