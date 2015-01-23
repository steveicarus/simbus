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
 * this function transmits a TPL over the RX stream to the target
 * endpoint. The caller formats TLPs as described by the PCIe base
 * specification, with 32bit words. This function matches up the words
 * with the AXIS stream that connects to the Xilinx PCIe core.
 */
static void do_send_tlp_raw(simbus_pcie_tlp_t bus,
			    const uint32_t*data, size_t ndata)
{
      while (ndata >= 1) {
	      /* Put the first word into the lew 32bits of the stream
		 word, and set up the enables. */
	    for (size_t idx = 0 ; idx < 32 ; idx += 1) {
		  uint32_t mask = 1 << idx;
		  bus->m_axis_rx_tdata[idx] = data[0] & mask? BIT_1 : BIT_0;
	    }
	    for (size_t idx = 0 ; idx < 4 ; idx += 1)
		  bus->m_axis_rx_tkeep[idx] = BIT_1;

	      /* If there are 2 or more words, then put the second
		 word in the high bits of the stream word, and set the
		 corresponsing enables. */
	    if (ndata >= 2) {
		  for (size_t idx = 0 ; idx < 32 ; idx += 1) {
			uint32_t mask = 1 << idx;
			bus->m_axis_rx_tdata[32+idx] = data[1] & mask? BIT_1 : BIT_0;
		  }
		  for (size_t idx = 0 ; idx < 4 ; idx += 1)
			bus->m_axis_rx_tkeep[4+idx] = BIT_1;
	    } else {
		  for (size_t idx = 0 ; idx < 32 ; idx += 1) {
			bus->m_axis_rx_tdata[32+idx] = BIT_0;
		  }
		  for (size_t idx = 4 ; idx < 8 ; idx += 1)
			bus->m_axis_rx_tkeep[idx] = BIT_0;
	    }

	      /* Last stream word? */
	    bus->m_axis_rx_tlast = ndata<=2? BIT_1 : BIT_0;
	      /* Certainly, this is a valid word. */
	    bus->m_axis_rx_tvalid = BIT_1;

	      /* Clock it out. */
	    __pcie_tlp_next_posedge(bus);

	      /* If the receiver was ready, then step to the next word. */
	    if (bus->m_axis_rx_tready == BIT_1) {
		  if (ndata >= 2) {
			data += 2;
			ndata -= 2;
		  } else {
			ndata -= 1;
			data += 1;
		  }

		  if (ndata == 0) {
			bus->m_axis_rx_tvalid = BIT_0;
			bus->m_axis_rx_tlast  = BIT_0;
			for (size_t idx = 0 ; idx < 8 ; idx += 1)
			      bus->m_axis_rx_tkeep[idx] = BIT_X;
			for (size_t idx = 0 ; idx < 64 ; idx += 1)
			      bus->m_axis_rx_tdata[idx] = BIT_X;
		  }
	    }
      }
}

void __pcie_tlp_send_tlp(simbus_pcie_tlp_t bus,
			 const uint32_t*data, size_t ndata)
{
      size_t idx;

      struct tlp_cell*cell = calloc(1, sizeof(struct tlp_cell));
      cell->ndata = ndata;
      cell->data = calloc(ndata, sizeof(uint32_t));
      for (idx = 0 ; idx < ndata ; idx += 1)
	    cell->data[idx] = data[idx];


      if (bus->tlp_out_list == 0) {
	      /* No current tlp out pending (meaning not recursed) so
		 start the list and start processing. Note that the
		 processing may generate new TLPs on the output queue,
		 so be prepared for that. */
	    bus->tlp_out_list = cell;
	    cell->next = cell;

	    while (bus->tlp_out_list) {
		  struct tlp_cell*cur = bus->tlp_out_list->next;
		  do_send_tlp_raw(bus, cur->data, cur->ndata);
		  if (bus->tlp_out_list == cur) {
			bus->tlp_out_list = 0;
		  } else {
			assert(bus->tlp_out_list->next == cur);
			bus->tlp_out_list->next = cur->next;
		  }
		  free(cur->data);
		  free(cur);
	    }

      } else {
	      /* If we are recursing, then just push the TLP onto the
		 end of a work queue. The root call will notice it and
		 process it when it is done. */
	    cell->next = bus->tlp_out_list->next;
	    bus->tlp_out_list->next = cell;
	    bus->tlp_out_list = cell;
      }
}

static void crank_recv_tlp(simbus_pcie_tlp_t bus, uint32_t val)
{
      if (bus->debug) {
	    fprintf(bus->debug, "TLP word %d: 0x%08" PRIx32 "\n",
		   bus->s_tlp_cnt, val);
      }

      assert(bus->s_tlp_cnt < MAX_TLP);
      bus->s_tlp_buf[bus->s_tlp_cnt++] = val;
}

static void copy_tlp_to_completion(simbus_pcie_tlp_t bus,
				   int tag, size_t words)
{
      assert(bus->completions[tag] == 0);
      assert(words >= 3 && words <= bus->s_tlp_cnt);

      bus->completions[tag] = calloc(words, sizeof(uint32_t));

      for (size_t idx = 0 ; idx < words ; idx += 1)
	    bus->completions[tag][idx] = bus->s_tlp_buf[idx];
}

static void process_Assert_INTx(simbus_pcie_tlp_t bus);
static void process_Deassert_INTx(simbus_pcie_tlp_t bus);

static void complete_recv_tlp(simbus_pcie_tlp_t bus)
{
      if (bus->debug) {
	    fprintf(bus->debug, "Received TLP:\n");
	    for (size_t idx = 0 ; idx < bus->s_tlp_cnt ; idx += 1)
		  fprintf(bus->debug, "    0x%08" PRIx32 "\n",
			  bus->s_tlp_buf[idx]);
	    fflush(bus->debug);
      }

      uint32_t*tlp = 0;
      size_t ntlp = 0;

	/* Dispatch the TLP based on the type. */
      assert(bus->s_tlp_cnt >= 1);
      uint32_t tmp = bus->s_tlp_buf[0];
      if ((tmp&0xff000000) == 0x0a000000) { /* Completion w/o data */

	      /* Route completions based on the tag */
	    int tag = (bus->s_tlp_buf[2] >> 8) & 0xff;
	    copy_tlp_to_completion(bus, tag, 3);
	    
      } else if ((tmp&0xff000000) == 0x4a000000) { /* Completion w/ data */

	      /* Route completions based on the tag */
	    int tag = (bus->s_tlp_buf[2] >> 8) & 0xff;
	    size_t words = (bus->s_tlp_buf[0] >> 0) & 0x03ff;
	    copy_tlp_to_completion(bus, tag, 3+words);

      } else if ((tmp&0xff000000) == 0x40000000) { /* Write w/ 32bit address */

	    size_t ndata = bus->s_tlp_buf[0] & 0x03ff;
	    uint64_t addr = bus->s_tlp_buf[2];
	    int be0 = bus->s_tlp_buf[1] & 0x0f;
	    int beN = (bus->s_tlp_buf[1] & 0xf0) >> 4;

	    if (bus->write_fun)
		  bus->write_fun (bus, bus->write_cookie, addr, bus->s_tlp_buf+3, ndata, be0, beN);
	      /* No need for completions to writes */

      } else if ((tmp&0xff000000) == 0x60000000) { /* Write w/ 64bit address */

	    size_t ndata = bus->s_tlp_buf[0] & 0x03ff;
	    uint64_t addr = bus->s_tlp_buf[2];
	    int be0 = bus->s_tlp_buf[1] & 0x0f;
	    int beN = (bus->s_tlp_buf[1] & 0xf0) >> 4;
	    addr <<= 32;
	    addr |= bus->s_tlp_buf[3];

	    if (bus->write_fun)
		  bus->write_fun (bus, bus->write_cookie, addr, bus->s_tlp_buf+4, ndata, be0, beN);
	      /* No need for completions to writes */

      } else if ((tmp&0xff000000) == 0x00000000) { /* Read w/ 32bit address */

	    size_t ndata = bus->s_tlp_buf[0] & 0x03ff;
	    uint64_t addr = bus->s_tlp_buf[2];
	    int be0 = bus->s_tlp_buf[1] & 0x0f;
	    int beN = (bus->s_tlp_buf[1] & 0xf0) >> 4;
	    uint32_t byte_count = (ndata * 4) & 0x0fff;

	    tlp = calloc(3+ndata, sizeof(uint32_t));
	    ntlp = ndata+3;

	    tlp[0] = 0x4a000000 | ndata; /* CplD */
	    tlp[1] = (bus->request_id<<16) | byte_count;
	    tlp[2] = bus->s_tlp_buf[1] & 0xffffff00; /* Copy RID & tag */
	    tlp[2] |= addr&0x0000007f;

	    if (bus->read_fun)
		  bus->read_fun (bus, bus->read_cookie, addr, tlp+3, ndata, be0, beN);

      } else if ((tmp&0xff000000) == 0x40000000) { /* Read w/ 64bit address */

	    size_t ndata = bus->s_tlp_buf[0] & 0x03ff;
	    uint64_t addr = bus->s_tlp_buf[2];
	    int be0 = bus->s_tlp_buf[1] & 0x0f;
	    int beN = (bus->s_tlp_buf[1] & 0xf0) >> 4;
	    uint32_t byte_count = (ndata * 4) & 0x0fff;
	    addr <<= 32;
	    addr |= bus->s_tlp_buf[3];

	    tlp = calloc(3+ndata, sizeof(uint32_t));
	    ntlp = ndata+3;

	    tlp[0] = 0x4a000000 | ndata; /* CplD */
	    tlp[1] = (bus->request_id<<16) | byte_count;
	    tlp[2] = bus->s_tlp_buf[1] & 0xffffff00; /* Copy RID & tag */
	    tlp[2] |= addr&0x0000007f;

	    if (bus->read_fun)
		  bus->read_fun (bus, bus->read_cookie, addr, tlp+3, ndata, be0, beN);

      } else if ((tmp&0xff000000) == 0x30000000) { /* Msg routed to root complex */

	    if ((bus->s_tlp_buf[1]&0x000000fc) == 0x20)
		  process_Assert_INTx(bus);
	    else if ((bus->s_tlp_buf[1]&0x000000fc) == 0x24)
		  process_Deassert_INTx(bus);

      } else {
	    fprintf(stderr, "%s:%d: Received unhandled TLP:\n", __FILE__, __LINE__);
	    for (size_t idx = 0 ; idx < bus->s_tlp_cnt ; idx += 1)
		  fprintf(stderr, "    0x%08" PRIx32 "\n",
			  bus->s_tlp_buf[idx]);
	    fflush(stderr);
      }

	/* Now erase the buffer and make ready to receive more TLPs. */
      bus->s_tlp_cnt = 0;

      if (tlp) {
	    __pcie_tlp_send_tlp(bus, tlp, ntlp);
	    free(tlp);
      }
}


/*
 * Message Request TLPs (routed to the root) look like this:
 *
 *        c...c is the message code
 *               0x20 -- Assert_INTA
 *               0x21 -- Assert_INTB
 *               0x22 -- Assert_INTC
 *               0x23 -- Assert_INTD
 *               0x24 -- Deassert_INTA
 *               0x25 -- Deassert_INTB
 *               0x26 -- Deassert_INTC
 *               0x27 -- Deassert_INTD
 *
 *    00110000 00000000 00000000 00000000
 *    00000000 00000000 00000000 cccccccc
 *    xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx  (reserved)
 *    xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
 */

static void process_Assert_INTx(simbus_pcie_tlp_t bus)
{
      int intx = bus->s_tlp_buf[1]&3;
      bus->intx_mask |= 1 << intx;
}

static void process_Deassert_INTx(simbus_pcie_tlp_t bus)
{
      int intx = bus->s_tlp_buf[1]&3;
      bus->intx_mask &= ~ (1<<intx);
}

void __pcie_tlp_recv_tlp(simbus_pcie_tlp_t bus)
{
	/* If the data from the slave is not tvalid, or if we are not
	   tready to receive it, then there is nothing to do here. */
      if (bus->s_axis_tx_tvalid != BIT_1)
	    return;
      if (bus->s_axis_tx_tready != BIT_1)
	    return;

      uint32_t val = 0;
      size_t bytes = 0;
      for (int idx = 0 ; idx < 8 ; idx += 1) {
	    int keep_bit = (idx&4) | (3-(idx&3));
	    if (bus->s_axis_tx_tkeep[keep_bit] != BIT_1)
		  continue;

	    int byte_base = 8 * keep_bit;
	    for (int bit = 0 ; bit < 8 ; bit += 1) {
		  val <<= 1;
		  if (bus->s_axis_tx_tdata[byte_base + 7-bit] == BIT_1)
			val |= 1;
	    }

	    bytes += 1;

	    if (bytes==4) {
		  crank_recv_tlp(bus, val);
		  val = 0;
		  bytes = 0;
	    }
      }

	/* Given the nature of the remote, we expect that the remote
	   will send an even number of 32bit words in a clock
	   iteration. If that is not the case, we have work to do. */
      assert(bytes==0);

	/* If this is the last clock of the stream, then call a
	   function to complete the TLP and send it on. */
      if (bus->s_axis_tx_tlast==BIT_1)
	    complete_recv_tlp(bus);
}
