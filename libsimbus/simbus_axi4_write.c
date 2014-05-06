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

# include  "simbus_axi4.h"
# include  "simbus_axi4_priv.h"
# include  <stdlib.h>
# include  <assert.h>

static void waddr_setup(simbus_axi4_t bus, uint64_t addr, int write_size, int prot)
{
      int idx;
      uint64_t mask64;

      assert(bus->addr_width <= AXI4_MAX_ADDR);
      for (idx = 0, mask64=1 ; idx < bus->addr_width ; idx += 1, mask64 <<= 1)
	    bus->awaddr[idx] = (addr&mask64)? BIT_1 : BIT_0;
      for (idx = 0 ; idx < 8 ; idx += 1)
	    bus->awlen[idx] = BIT_0;
      bus->awsize[0] = write_size&1? BIT_1 : BIT_0;
      bus->awsize[1] = write_size&2? BIT_1 : BIT_0;
      bus->awsize[2] = write_size&4? BIT_1 : BIT_0;
      bus->awburst[0] = BIT_1;
      bus->awburst[1] = BIT_0;
      bus->awlock[0] = BIT_0;
      bus->awlock[1] = BIT_0;
      bus->awcache[0] = BIT_0;
      bus->awcache[1] = BIT_0;
      bus->awcache[2] = BIT_0;
      bus->awcache[3] = BIT_0;
      bus->awprot[0] = (prot&1)? BIT_1 : BIT_0;
      bus->awprot[1] = (prot&2)? BIT_1 : BIT_0;
      bus->awprot[2] = (prot&4)? BIT_1 : BIT_0;
      bus->awqos[0] = BIT_0;
      bus->awqos[1] = BIT_0;
      bus->awqos[2] = BIT_0;
      bus->awqos[3] = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->awid[idx] = BIT_0;

}

static void waddr_clear(simbus_axi4_t bus)
{
      int idx;

      for (idx = 0 ; idx < AXI4_MAX_ADDR ; idx += 1)
	    bus->awaddr[idx] = BIT_X;
      for (idx = 0 ; idx < 8 ; idx += 1)
	    bus->awlen[idx] = BIT_X;
      bus->awsize[0] = BIT_X;
      bus->awsize[1] = BIT_X;
      bus->awsize[2] = BIT_X;
      bus->awburst[0] = BIT_X;
      bus->awburst[1] = BIT_X;
      bus->awlock[0] = BIT_X;
      bus->awlock[1] = BIT_X;
      bus->awcache[0] = BIT_X;
      bus->awcache[1] = BIT_X;
      bus->awcache[2] = BIT_X;
      bus->awcache[3] = BIT_X;
      bus->awprot[0] = BIT_X;
      bus->awprot[1] = BIT_X;
      bus->awprot[2] = BIT_X;
      bus->awqos[0] = BIT_X;
      bus->awqos[1] = BIT_X;
      bus->awqos[2] = BIT_X;
      bus->awqos[3] = BIT_X;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->awid[idx] = BIT_X;
}

static simbus_axi4_resp_t wait_for_resp(simbus_axi4_t bus)
{
      int idx;
      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;
      int response_timer = AXI4_RESP_TIMELIMIT;

	/* Wait for all three channels related to the transfer to
	   complete. The address must be transfered, the data
	   delivered, and the response returned. */
      while (bus->awvalid==BIT_1 || bus->wvalid==BIT_1 || bus->bready==BIT_1) {

	    __axi4_next_posedge(bus);

	      /* If the address is transferred, then stop driving it. */
	    if (bus->awvalid==BIT_1 && bus->awready==BIT_1) {
		  bus->awvalid = BIT_0;
		  waddr_clear(bus);
	    }

	      /* If the data is transferred, then stop driving it. */
	    if (bus->wvalid==BIT_1 && bus->wready==BIT_1) {
		  bus->wvalid = BIT_0;
		  for (idx = 0 ; idx < AXI4_MAX_DATA ; idx += 1)
			bus->wdata[idx] = BIT_X;
		  for (idx = 0 ; idx < AXI4_MAX_DATA/8 ; idx += 1)
			bus->wstrb[idx] = BIT_X;
	    }

	      /* If the response is received, then capture it. */
	    if (bus->bready==BIT_1 && bus->bvalid==BIT_1) {
		  int resp_tmp = 0;
		  bus->bready = BIT_0;
		  resp_tmp |= bus->bresp[0]==BIT_1? 1 : 0;
		  resp_tmp |= bus->bresp[1]==BIT_1? 2 : 0;
		  switch (resp_tmp) {
		      case 0:
			resp_code = SIMBUS_AXI4_RESP_OKAY;
			break;
		      case 1:
			resp_code = SIMBUS_AXI4_RESP_EXOKAY;
			break;
		      case 2:
			resp_code = SIMBUS_AXI4_RESP_SLVERR;
			break;
		      case 3:
			resp_code = SIMBUS_AXI4_RESP_DECERR;
			break;
		  }
	    } else if (bus->bready==BIT_1) {
		  response_timer -= 1;
		  if (response_timer < 0) {
			if (bus->debug) {
			      fprintf(bus->debug, "AXI4 write: NO RESPONSE, GIVING UP\n");
			}
			resp_code = SIMBUS_AXI4_RESP_SLVERR;
			bus->bready==BIT_0;
		  }
	    }
      }

      return resp_code;
}

simbus_axi4_resp_t simbus_axi4_write32(simbus_axi4_t bus,
				       uint64_t addr, int prot,
				       uint32_t data, int strb)
{
      int idx;
      uint32_t mask32;

	/* This function (at this point) only supports 32bit writes to
	   32bit data busses. */
      assert(bus->data_width == 32);
	/* Only support aligned addresses. */
      assert(addr%4 == 0);

	/* Drive the write address to the write address channel. */
      waddr_setup(bus, addr, bus->axsize_word, prot);

      bus->awvalid = BIT_1;

	/* Drive the write data to the write data channel */
      bus->wvalid = BIT_1;
      for (idx=0, mask32=1 ; idx < bus->data_width ; idx += 1, mask32 <<= 1)
	    bus->wdata[idx] = (data&mask32)? BIT_1 : BIT_0;
      for (idx = 0 ; idx < bus->data_width/8 ; idx += 1)
	    bus->wstrb[idx] = BIT_1;
      for (idx = bus->data_width/8 ; idx < 8 ; idx += 1)
	    bus->wstrb[idx] = BIT_0;

	/* Immediately ready to receive write response. */
      bus->bready = BIT_1;

	/* Wait for the write transaction to complete. */
      return wait_for_resp(bus);
}

/*
 * Write an 8bit byte to a wide bus by writing to the aligned address
 * a full word with a single byte lane configured and the right bits
 * filled in.
 */
simbus_axi4_resp_t simbus_axi4_write8(simbus_axi4_t bus,
				      uint64_t addr, int prot,
				      uint8_t  data)
{
      int idx;
      uint8_t mask8;

	/* Offset into the word of the target byte. */
      int data_pref = addr % (bus->data_width / 8);

	/* Setup the write address to the write address channel. */
      waddr_setup(bus, addr, bus->axsize_word, prot);
      bus->awvalid = BIT_1;

	/* Drive the write data to the write data channel. */
      bus->wvalid = BIT_1;

      for (idx=0 ; idx < data_pref*8 ; idx += 1)
	    bus->wdata[idx] = BIT_X;
      for (idx=0, mask8=1 ; idx < 8 ; idx += 1, mask8 <<= 1)
	    bus->wdata[idx+data_pref*8] = (data&mask8)? BIT_1 : BIT_0;
      for (idx=data_pref*8+8 ; idx < bus->data_width ; idx += 1)
	    bus->wdata[idx] = BIT_X;

      for (idx=0 ; idx < bus->data_width/8 ; idx += 1)
	    bus->wstrb[idx] = (idx==data_pref)? BIT_1 : BIT_0;

	/* Immediately ready to receive write response. */
      bus->bready = BIT_1;

	/* Wait for the write transaction to complete. */
      return wait_for_resp(bus);
}
