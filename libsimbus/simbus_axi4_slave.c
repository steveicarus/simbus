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

# include  "simbus_axi4s.h"
# include  "simbus_axi4_priv.h"
# include  <string.h>
# include  <assert.h>

/*
 * This is a slave device, so send a READY message with the signals
 * that the slave drives, then wait for the values that the master
 * drives in the UNTIL response.
 */
static int __axi4s_ready_command(simbus_axi4_t bus)
{
      int idx;
      char buf[4096];
      snprintf(buf, sizeof(buf), "READY %" PRIu64 "e%d", bus->time_mant, bus->time_exp);

      char*cp = buf + strlen(buf);

      strcpy(cp, " AWREADY=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awready);

      strcpy(cp, " WREADY=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->wready);

      strcpy(cp, " BVALID=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->bvalid);

      strcpy(cp, " BRESP=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->bresp[1]);
      *cp++ = __bitval_to_char(bus->bresp[0]);

      strcpy(cp, " BID=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->wid_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->bid[bus->wid_width-1-idx]);

      strcpy(cp, " ARREADY=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arready);

      strcpy(cp, " RVALID=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->rvalid);

      strcpy(cp, " RDATA=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->data_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->rdata[bus->data_width-1-idx]);
 
      strcpy(cp, " RRESP=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->rresp[1]);
      *cp++ = __bitval_to_char(bus->rresp[0]);

      strcpy(cp, " RID=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->rid_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->bid[bus->rid_width-1-idx]);

      if (bus->irq_width > 0) {
	    strcpy(cp, " IRQ=");
	    cp += strlen(cp);
	    for (idx = 0 ; idx < bus->irq_width ; idx += 1)
		  *cp++ = __bitval_to_char(bus->irq[bus->irq_width-1-idx]);
      }

      if (bus->debug) {
	    *cp = 0;
	    fprintf(bus->debug, "SEND %s\n", buf);
	    fflush(bus->debug);
      }

      *cp++ = '\n';
      *cp = 0;

      char*argv[2048];
      int argc = __simbus_server_send_recv(bus->fd, buf, sizeof(buf),
					   2048, argv, bus->debug);
      if (argc == 0) {
	    return SIMBUS_AXI4_FINISHED;
      }

      if (strcmp(argv[0],"FINISH") == 0) {
	    return SIMBUS_AXI4_FINISHED;
      }

      assert(strcmp(argv[0],"UNTIL")==0);

      assert(argc >= 1);
      __parse_time_token(argv[1], &bus->time_mant, &bus->time_exp);

      for (idx = 2 ; idx < argc ; idx += 1) {
	    cp = strchr(argv[idx],'=');
	    assert(cp && *cp=='=');

	    *cp++ = 0;

	    if (strcmp(argv[idx],"ACLK") == 0) {
		  bus->aclk = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"ARESETn") == 0) {
		  bus->areset_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"AWVALID") == 0) {
		  bus->awvalid = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"AWADDR") == 0) {
		  assert(strlen(cp) == bus->addr_width);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->addr_width ; bdx += 1)
			bus->awaddr[bus->addr_width-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"AWLEN") == 0) {
		  assert(strlen(cp) == 8);
		  int bdx;
		  for (bdx = 0 ; bdx < 8 ; bdx += 1)
			bus->awlen[8-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"AWSIZE") == 0) {
		  assert(strlen(cp) == 3);
		  bus->awsize[2] = __char_to_bitval(cp[0]);
		  bus->awsize[1] = __char_to_bitval(cp[1]);
		  bus->awsize[0] = __char_to_bitval(cp[2]);

	    } else if (strcmp(argv[idx],"AWBURST") == 0) {
		  assert(strlen(cp) == 2);
		  bus->awburst[1] = __char_to_bitval(cp[0]);
		  bus->awburst[0] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"AWLOCK") == 0) {
		  assert(strlen(cp) == 2);
		  bus->awlock[1] = __char_to_bitval(cp[0]);
		  bus->awlock[0] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"AWCACHE") == 0) {
		  assert(strlen(cp) == 4);
		  bus->awcache[3] = __char_to_bitval(cp[0]);
		  bus->awcache[2] = __char_to_bitval(cp[1]);
		  bus->awcache[1] = __char_to_bitval(cp[2]);
		  bus->awcache[0] = __char_to_bitval(cp[3]);

	    } else if (strcmp(argv[idx],"AWPROT") == 0) {
		  assert(strlen(cp) == 3);
		  bus->awprot[2] = __char_to_bitval(cp[0]);
		  bus->awprot[1] = __char_to_bitval(cp[1]);
		  bus->awprot[0] = __char_to_bitval(cp[2]);

	    } else if (strcmp(argv[idx],"AWQOS") == 0) {
		  assert(strlen(cp) == 4);
		  bus->awqos[3] = __char_to_bitval(cp[0]);
		  bus->awqos[2] = __char_to_bitval(cp[1]);
		  bus->awqos[1] = __char_to_bitval(cp[2]);
		  bus->awqos[0] = __char_to_bitval(cp[3]);

	    } else if (strcmp(argv[idx],"AWID") == 0) {
		  assert(strlen(cp) == bus->wid_width);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->wid_width ; bdx += 1)
			bus->awid[bus->wid_width-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"WVALID") == 0) {
		  bus->wvalid = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"WDATA") == 0) {
		  assert(strlen(cp) == bus->data_width);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->data_width ; bdx += 1)
			bus->wdata[bus->data_width-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"WSTRB") == 0) {
		  assert(strlen(cp) == bus->data_width/8);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->data_width/8 ; bdx += 1)
			bus->wstrb[bus->data_width/8-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"BREADY") == 0) {
		  bus->bready = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"ARVALID") == 0) {
		  bus->arvalid = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"ARADDR") == 0) {
		  assert(strlen(cp) == bus->addr_width);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->addr_width ; bdx += 1)
			bus->araddr[bus->addr_width-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"ARLEN") == 0) {
		  assert(strlen(cp) == 8);
		  int bdx;
		  for (bdx = 0 ; bdx < 8 ; bdx += 1)
			bus->arlen[8-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"ARSIZE") == 0) {
		  assert(strlen(cp) == 3);
		  bus->arsize[2] = __char_to_bitval(cp[0]);
		  bus->arsize[1] = __char_to_bitval(cp[1]);
		  bus->arsize[0] = __char_to_bitval(cp[2]);

	    } else if (strcmp(argv[idx],"ARBURST") == 0) {
		  assert(strlen(cp) == 2);
		  bus->arburst[1] = __char_to_bitval(cp[0]);
		  bus->arburst[0] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"ARLOCK") == 0) {
		  assert(strlen(cp) == 2);
		  bus->arlock[1] = __char_to_bitval(cp[0]);
		  bus->arlock[0] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"ARCACHE") == 0) {
		  assert(strlen(cp) == 4);
		  bus->arcache[3] = __char_to_bitval(cp[0]);
		  bus->arcache[2] = __char_to_bitval(cp[1]);
		  bus->arcache[1] = __char_to_bitval(cp[2]);
		  bus->arcache[0] = __char_to_bitval(cp[3]);

	    } else if (strcmp(argv[idx],"ARPROT") == 0) {
		  assert(strlen(cp) == 3);
		  bus->arprot[2] = __char_to_bitval(cp[0]);
		  bus->arprot[1] = __char_to_bitval(cp[1]);
		  bus->arprot[0] = __char_to_bitval(cp[2]);

	    } else if (strcmp(argv[idx],"ARQOS") == 0) {
		  assert(strlen(cp) == 4);
		  bus->arqos[3] = __char_to_bitval(cp[0]);
		  bus->arqos[2] = __char_to_bitval(cp[1]);
		  bus->arqos[1] = __char_to_bitval(cp[2]);
		  bus->arqos[0] = __char_to_bitval(cp[3]);

	    } else if (strcmp(argv[idx],"ARID") == 0) {
		  assert(strlen(cp) == bus->rid_width);
		  int bdx;
		  for (bdx = 0 ; bdx < bus->rid_width ; bdx += 1)
			bus->arid[bus->rid_width-1-bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"RREADY") == 0) {
		  bus->rready = __char_to_bitval(*cp);

	    } else {
		  ; /* Skip signals not of interest to me. */
	    }
      }

      return 0;
}

static uint64_t bits_to_addr(simbus_axi4_t bus, bus_bitval_t bits[AXI4_MAX_ADDR])
{
      int idx;
      uint64_t res = 0;
      
      for (idx = 0 ; idx < bus->addr_width ; idx += 1) {
	    res *= 2;
	    if (bits[bus->addr_width-1-idx] == BIT_1) res |= 1;
      }

      return res;
}

static uint32_t bits_to_uint32(bus_bitval_t*bits, size_t nbits)
{
      int idx;
      uint32_t res = 0;
      
      for (idx = 0 ; idx < nbits ; idx += 1) {
	    res *= 2;
	    if (bits[nbits-1-idx] == BIT_1) res |= 1;
      }

      return res;
}

static uint8_t bits_to_uint8(bus_bitval_t*bits, size_t nbits)
{
      int idx;
      uint8_t res = 0;
      
      for (idx = 0 ; idx < nbits ; idx += 1) {
	    res *= 2;
	    if (bits[nbits-1-idx] == BIT_1) res |= 1;
      }

      return res;
}

static void do_reset(simbus_axi4_t bus)
{
      int idx;

      if (bus->debug)
	    fprintf(bus->debug, "do_reset: RESET\n");

      bus->awready = BIT_1;
      bus->wready  = BIT_0;
      bus->bvalid  = BIT_0;
      bus->bresp[0]= BIT_0;
      bus->bresp[1]= BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->bid[idx] = BIT_0;
      bus->arready = BIT_1;
      bus->rvalid  = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_DATA ; idx += 1)
	    bus->rdata[idx] = BIT_0;
      bus->rresp[0] = BIT_0;
      bus->rresp[1] = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->rid[idx] = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_IRQ ; idx  += 1)
	    bus->irq[idx] = BIT_0;
}

static simbus_axi4_resp_t write_data(simbus_axi4_t bus)
{
      simbus_axi4_resp_t resp;

      size_t use_wid = 8 * (1 << bus->slave.wsize);

	/* Each beat can transmit no more then the bus width. */
      assert(use_wid <= bus->data_width);
	/* Each beat must be naturally aligned. */
      assert(bus->slave.waddr % (use_wid/8) == 0);

	/* Where in the word is the write to happen? */
      int bit_offset = bus->slave.waddr % (bus->data_width/8);
      bit_offset *= 8;

	/* Use the target device function to write the word. */
      switch (use_wid) {
	  case 32:
	    assert(bus->device);
	    assert(bus->device->write32);
	    resp = bus->device->write32 (bus, bus->slave.waddr, bus->slave.wprot,
					 bits_to_uint32(bus->wdata+bit_offset, 32));
	    break;
	  case 8:
	    assert(bus->device);
	    assert(bus->device->write8);
	    resp = bus->device->write8 (bus, bus->slave.waddr, bus->slave.wprot,
					bits_to_uint8(bus->wdata+bit_offset, 8));
	    break;
	  default:
	    resp = SIMBUS_AXI4_RESP_SLVERR;
	    break;
      }

      return resp;
}

static simbus_axi4_resp_t read_data(simbus_axi4_t bus)
{
      simbus_axi4_resp_t resp;
      uint32_t data32 = 0;
      uint16_t data16 = 0;
      uint8_t  data8  = 0;
      int idx;

      size_t use_wid = 8 * (1 << bus->slave.rsize);

      assert(use_wid <= bus->data_width);
      int bit_offset = bus->slave.raddr % (bus->data_width/8);
      bit_offset *= 8;

      for (idx = 0 ; idx < bus->data_width ; idx += 1)
	    bus->rdata[idx] = BIT_X;

      switch (use_wid) {
	  case 32:
	    assert(bus->device);
	    assert(bus->device->read32);
	    resp = bus->device->read32(bus, bus->slave.raddr, bus->slave.rprot, &data32);
	    for (idx = 0 ; idx < 32 ; idx += 1) {
		  bus->rdata[idx+bit_offset] = data32&1;
		  data32 /= 2;
	    }
	    break;
	  case 16:
	    assert(bus->device);
	    assert(bus->device->read16);
	    resp = bus->device->read16(bus, bus->slave.raddr, bus->slave.rprot, &data16);
	    for (idx = 0 ; idx < 16 ; idx += 1) {
		  bus->rdata[idx+bit_offset] = data16&1;
		  data16 /= 2;
	    }
	    break;
	  case 8:
	    assert(bus->device);
	    assert(bus->device->read8);
	    resp = bus->device->read8(bus, bus->slave.raddr, bus->slave.rprot, &data8);
	    for (idx = 0 ; idx < 8 ; idx += 1) {
		  bus->rdata[idx+bit_offset] = data8&1;
		  data8 /= 2;
	    }
	    break;
	  default:
	    resp = SIMBUS_AXI4_RESP_SLVERR;
	    break;
      }

      return resp;
}

static void do_slave_machine(simbus_axi4_t bus)
{
      bus_bitval_t next_awready= bus->awready;
      bus_bitval_t next_wready = bus->wready;
      bus_bitval_t next_bvalid = bus->bvalid;
      bus_bitval_t next_bresp0 = bus->bresp[0];
      bus_bitval_t next_bresp1 = bus->bresp[1];

      bus_bitval_t next_arready = bus->arready;
      bus_bitval_t next_rvalid = bus->rvalid;
      bus_bitval_t next_rresp0 = bus->rresp[0];
      bus_bitval_t next_rresp1 = bus->rresp[1];

	/* Detect a write address */
      if (bus->awvalid==BIT_1 && bus->awready==BIT_1) {
	      /* Address is valid and ready, stash it. */
	    bus->slave.waddr = bits_to_addr (bus, bus->awaddr);
	    bus->slave.wsize = bits_to_uint8(bus->awsize, 3);
	    bus->slave.wlen  = bits_to_uint8(bus->awlen,  8);
	    next_awready = BIT_0;
	    next_wready  = BIT_1;

      } else if (bus->awready==BIT_0 && bus->wready==BIT_0) {
	    next_awready = BIT_1;

      }

	/* Detect write data */
      if (bus->wvalid==BIT_1 && bus->wready==BIT_1) {
	    simbus_axi4_resp_t resp = write_data(bus);
	    next_wready = BIT_0;

	    next_bvalid = BIT_1;
	    next_bresp0 = resp&1;
	    next_bresp1 = resp&2;
      }

	/* Detect that the BRESP has been transmitted */
      if (bus->bvalid==BIT_1 && bus->bready==BIT_1) {
	    next_awready = BIT_1;
	    next_bvalid = BIT_0;
	    next_bresp0 = BIT_X;
	    next_bresp1 = BIT_X;
      }

	/* Detect a read request */
      if (bus->arvalid==BIT_1 && bus->arready) {
	    bus->slave.raddr = bits_to_addr (bus, bus->araddr);
	    bus->slave.rsize = bits_to_uint8(bus->arsize, 3);
	    bus->slave.rlen  = bits_to_uint8(bus->arlen,  8);
	    next_arready = BIT_0;

	    simbus_axi4_resp_t resp = read_data(bus);

	    next_arready = BIT_0;
	    next_rvalid = BIT_1;
	    next_rresp0 = resp&1;
	    next_rresp1 = resp&2;
      }

	/* Dest read data has been transmitted */
      if (bus->rvalid==BIT_1 && bus->rready==BIT_1) {
	    next_rvalid = BIT_0;
	    next_arready = BIT_1;
      }

      bus->awready= next_awready;
      bus->wready = next_wready;
      bus->bvalid = next_bvalid;
      bus->bresp[0] = next_bresp0;
      bus->bresp[1] = next_bresp1;
      bus->arready = next_arready;
      bus->rvalid  = next_rvalid;
      bus->rresp[0] = next_rresp0;
      bus->rresp[1] = next_rresp1;
}

int simbus_axi4_slave(simbus_axi4_t bus, const struct simbus_axi4s_slave_s*dev)
{
      assert(dev && !bus->device);
      bus->device = dev;

      for (;;) {
	      /* Wait for the clock to fall... */
	    while (bus->aclk != BIT_0) { /* ACLK==1/X/Z */
		  __axi4s_ready_command(bus);
	    }

	    if (bus->areset_n == BIT_0)
		  do_reset(bus);

	      /* and wait for it to go high again. */
	    while (bus->aclk != BIT_1) { /* ACLK==0/X/Z */
		  __axi4s_ready_command(bus);
	    }

	    if (bus->areset_n == BIT_0) {
		  do_reset(bus);
	    } else {
		  do_slave_machine(bus);
	    }
      }

      return 0;
}
