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

static void raddr_setup(simbus_axi4_t bus, uint64_t addr, int size, int prot)
{
      int idx;
      uint64_t mask64;

      bus->arvalid = BIT_1;
      for (idx = 0, mask64=1 ; idx < bus->addr_width ; idx += 1, mask64 <<= 1)
	    bus->araddr[idx] = (addr&mask64)? BIT_1 : BIT_0;
      for (idx = 0 ; idx < 8 ; idx += 1)
	    bus->arlen[idx] = BIT_0;
      bus->arsize[0] = size&1? BIT_1 : BIT_0;
      bus->arsize[1] = size&2? BIT_1 : BIT_0;
      bus->arsize[2] = size&4? BIT_1 : BIT_0;
      bus->arburst[0] = BIT_1;
      bus->arburst[1] = BIT_0;
      bus->arlock[0] = BIT_0;
      bus->arlock[1] = BIT_0;
      bus->arcache[0] = BIT_0;
      bus->arcache[1] = BIT_0;
      bus->arcache[2] = BIT_0;
      bus->arcache[3] = BIT_0;
      bus->arprot[0] = (prot&1)? BIT_1 : BIT_0;
      bus->arprot[1] = (prot&2)? BIT_1 : BIT_0;
      bus->arprot[2] = (prot&4)? BIT_1 : BIT_0;
      bus->arqos[0] = BIT_0;
      bus->arqos[1] = BIT_0;
      bus->arqos[2] = BIT_0;
      bus->arqos[3] = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->arid[idx] = BIT_0;
}

static void raddr_check(simbus_axi4_t bus)
{
      if (bus->arvalid==BIT_1 && bus->arready==BIT_1) {
	    int idx;
	    bus->arvalid = BIT_0;
	    for (idx = 0 ; idx < AXI4_MAX_ADDR ; idx += 1)
		  bus->araddr[idx] = BIT_X;
	    for (idx = 0 ; idx < 8 ; idx += 1)
		  bus->arlen[idx] = BIT_X;
	    bus->arsize[0] = BIT_X;
	    bus->arsize[1] = BIT_X;
	    bus->arsize[2] = BIT_X;
	    bus->arburst[0] = BIT_X;
	    bus->arburst[1] = BIT_X;
	    bus->arlock[0] = BIT_X;
	    bus->arlock[1] = BIT_X;
	    bus->arcache[0] = BIT_X;
	    bus->arcache[1] = BIT_X;
	    bus->arcache[2] = BIT_X;
	    bus->arcache[3] = BIT_X;
	    bus->arprot[0] = BIT_X;
	    bus->arprot[1] = BIT_X;
	    bus->arprot[2] = BIT_X;
	    bus->arqos[0] = BIT_X;
	    bus->arqos[1] = BIT_X;
	    bus->arqos[2] = BIT_X;
	    bus->arqos[3] = BIT_X;
	    for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
		  bus->arid[idx] = BIT_X;
      }
}

static simbus_axi4_resp_t read_extract_resp(simbus_axi4_t bus)
{
      int resp_tmp = 0;
      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;

	/* Extract and interpret the response code. */
      resp_tmp |= bus->rresp[0]==BIT_1? 1 : 0;
      resp_tmp |= bus->rresp[1]==BIT_1? 2 : 0;
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

      return resp_code;
}

simbus_axi4_resp_t simbus_axi4_read64(simbus_axi4_t bus,
				      uint64_t addr, int prot,
				      uint64_t*data)
{
	/* Offset into the word of the target byte */
      int data_pref = addr % (bus->data_width / 8);

	/* For now, assume 32bit data bus width. */
      assert(bus->data_width >= 64);
      assert(bus->addr_width <= 64);
	/* For now, assume address is aligned. */
      assert(addr%8 == 0);

	/* Drive the read address to the read address channel. */
      raddr_setup(bus, addr, bus->axsize_word, prot);

	/* Ready for the read response. */
      bus->rready = BIT_1;

      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;

      while (bus->arvalid==BIT_1 || bus->rready==BIT_1) {

	    __axi4_next_posedge(bus);

	      /* If the address is transferred, then stop driving it. */
	    raddr_check(bus);

	      /* If the data response is received, then capture it. */
	    if (bus->rready==BIT_1 && bus->rvalid==BIT_1) {
		  int idx;
		  uint64_t mask64;
		  bus->rready = BIT_0;

		    /* Extract the data word */
		  data[0] = 0;
		  for (idx=0, mask64=1 ; idx < 64 ; idx += 1, mask64<<=1)
			data[0] |= bus->rdata[data_pref*8+idx]==BIT_1? mask64 : 0;

		  resp_code = read_extract_resp(bus);
	    }
      }

      return resp_code;
}

simbus_axi4_resp_t simbus_axi4_read32(simbus_axi4_t bus,
				      uint64_t addr, int prot,
				      uint32_t*data)
{
	/* Offset into the word of the target byte */
      int data_pref = addr % (bus->data_width / 8);

	/* For now, assume 32bit data bus width. */
      assert(bus->data_width >= 32);
      assert(bus->addr_width <= 64);
	/* For now, assume address is aligned. */
      assert(addr%4 == 0);

	/* Drive the read address to the read address channel. */
      raddr_setup(bus, addr, bus->axsize_word, prot);

	/* Ready for the read response. */
      bus->rready = BIT_1;

      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;

      while (bus->arvalid==BIT_1 || bus->rready==BIT_1) {

	    __axi4_next_posedge(bus);

	      /* If the address is transferred, then stop driving it. */
	    raddr_check(bus);

	      /* If the data response is received, then capture it. */
	    if (bus->rready==BIT_1 && bus->rvalid==BIT_1) {
		  int idx;
		  uint32_t mask32;
		  bus->rready = BIT_0;

		    /* Extract the data word */
		  data[0] = 0;
		  for (idx=0, mask32=1 ; idx < 32 ; idx += 1, mask32<<=1)
			data[0] |= bus->rdata[data_pref*8+idx]==BIT_1? mask32 : 0;

		  resp_code = read_extract_resp(bus);
	    }
      }

      return resp_code;
}

simbus_axi4_resp_t simbus_axi4_read16(simbus_axi4_t bus,
				      uint64_t addr, int prot,
				      uint16_t*data)
{
	/* Offset into the word of the target byte */
      int data_pref = addr % (bus->data_width / 8);

	/* For now, assume 32bit data bus width. */
      assert(bus->data_width >= 16);
      assert(bus->addr_width <= 64);
	/* For now, assume address is aligned. */
      assert(addr%2 == 0);

	/* Drive the read address to the read address channel. */
      raddr_setup(bus, addr, bus->axsize_word, prot);

	/* Ready for the read response. */
      bus->rready = BIT_1;

      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;

      while (bus->arvalid==BIT_1 || bus->rready==BIT_1) {

	    __axi4_next_posedge(bus);

	      /* If the address is transferred, then stop driving it. */
	    raddr_check(bus);

	      /* If the data response is received, then capture it. */
	    if (bus->rready==BIT_1 && bus->rvalid==BIT_1) {
		  int idx;
		  uint16_t mask16;
		  bus->rready = BIT_0;

		    /* Extract the data word */
		  data[0] = 0;
		  for (idx=0, mask16=1 ; idx < 16 ; idx += 1, mask16<<=1)
			data[0] |= bus->rdata[data_pref*8+idx]==BIT_1? mask16 : 0;

		  resp_code = read_extract_resp(bus);
	    }
      }

      return resp_code;
}

/*
 * Read a single byte from the AXI4 bus by reading a word and masking
 * the bits that we need.
 */
simbus_axi4_resp_t simbus_axi4_read8(simbus_axi4_t bus,
				     uint64_t addr, int prot,
				     uint8_t*data)
{
	/* Offset into the word of the target byte */
      int data_pref = addr % (bus->data_width / 8);

	/* Drive the read address to the read address channel. */
      raddr_setup(bus, addr, bus->axsize_word, prot);

	/* Ready for the read response. */
      bus->rready = BIT_1;

      simbus_axi4_resp_t resp_code = SIMBUS_AXI4_RESP_OKAY;

      while (bus->arvalid==BIT_1 || bus->rready==BIT_1) {

	    __axi4_next_posedge(bus);

	      /* If the address is transferred, then stop driving it. */
	    raddr_check(bus);

	      /* If the data is received, then capture it. */
	    if (bus->rready==BIT_1 && bus->rvalid==BIT_1) {
		  int idx;
		  int mask8;
		  bus->rready = BIT_0;

		    /* Extract the data byte */
		  data[0] = 0;
		  for (idx=0, mask8=1 ; idx < 8 ; idx += 1, mask8<<=1)
			data[0] |= bus->rdata[data_pref*8+idx]==BIT_1? mask8 : 0;

		  resp_code = read_extract_resp(bus);
	    }
      }

      return resp_code;
}
