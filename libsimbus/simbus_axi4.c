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
# include  <string.h>
# include  <unistd.h>
# include  <assert.h>

static void init_simbus_axi4(struct simbus_axi4_s*bus)
{
      int idx;

      bus->debug = 0;

      bus->fd = -1;
      bus->time_mant = 0;
      bus->time_exp = 0;

	/* Signals that I drive... */
      bus->areset_n = BIT_1;
	/* .. write address channel */
      bus->awvalid = BIT_0;
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
	/* .. write data channel */
      bus->wvalid = BIT_0;
      for (idx = 0 ; idx < AXI4_MAX_DATA ; idx += 1)
	    bus->wdata[idx] = BIT_X;
      for (idx = 0 ; idx < AXI4_MAX_DATA/8 ; idx += 1)
	    bus->wstrb[idx] = BIT_X;
	/* .. write response channel */
      bus->bready = BIT_0;
	/* .. read address channel */
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
	/* .. read data channel */
      bus->rready = BIT_0;

	/* Signals from the server... */
      bus->aclk = BIT_X;
	/* .. write address channel */
      bus->awready = BIT_Z;
	/* .. write data channel */
      bus->wready = BIT_Z;
	/* .. write response channel */
      bus->bvalid = BIT_Z;
      bus->bresp[0] = BIT_Z;
      bus->bresp[1] = BIT_Z;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->bid[idx] = BIT_Z;
	/* .. read address channel */
      bus->arready = BIT_Z;
	/* .. read data channel */
      bus->rvalid = BIT_Z;
      for (idx = 0 ; idx < AXI4_MAX_DATA ; idx += 1)
	    bus->rdata[idx] = BIT_Z;
      bus->rresp[0] = BIT_Z;
      bus->rresp[1] = BIT_Z;
      bus->rresp[2] = BIT_Z;
      for (idx = 0 ; idx < AXI4_MAX_ID ; idx += 1)
	    bus->arid[idx] = BIT_Z;
}

int __axi4_ready_command(struct simbus_axi4_s*bus)
{
      int idx;
      char buf[4096];
      snprintf(buf, sizeof(buf), "READY %" PRIu64 "e%d", bus->time_mant, bus->time_exp);

      char*cp = buf + strlen(buf);

      strcpy(cp, " ARESETn=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->areset_n);

      strcpy(cp, " AWVALID=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awvalid);

      strcpy(cp, " AWADDR=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->addr_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->awaddr[idx]);

      strcpy(cp, " AWLEN=");
      cp += strlen(cp);
      for (idx = 0 ; idx < 8 ; idx += 1)
	    *cp++ = __bitval_to_char(bus->awlen[idx]);

      strcpy(cp, " AWSIZE=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awsize[0]);
      *cp++ = __bitval_to_char(bus->awsize[1]);
      *cp++ = __bitval_to_char(bus->awsize[2]);

      strcpy(cp, " AWBURST=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awburst[0]);
      *cp++ = __bitval_to_char(bus->awburst[1]);

      strcpy(cp, " AWLOCK=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awlock[0]);
      *cp++ = __bitval_to_char(bus->awlock[1]);

      strcpy(cp, " AWCACHE=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awcache[0]);
      *cp++ = __bitval_to_char(bus->awcache[1]);
      *cp++ = __bitval_to_char(bus->awcache[2]);
      *cp++ = __bitval_to_char(bus->awcache[3]);

      strcpy(cp, " AWPROT=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awprot[0]);
      *cp++ = __bitval_to_char(bus->awprot[1]);
      *cp++ = __bitval_to_char(bus->awprot[2]);

      strcpy(cp, " AWQOS=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->awqos[0]);
      *cp++ = __bitval_to_char(bus->awqos[1]);
      *cp++ = __bitval_to_char(bus->awqos[2]);
      *cp++ = __bitval_to_char(bus->awqos[3]);

      strcpy(cp, " AWID=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->wid_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->awid[idx]);

      strcpy(cp, " WVALID=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->wvalid);

      strcpy(cp, " WDATA=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->data_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->wdata[idx]);

      strcpy(cp, " WSTRB=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->data_width/8 ; idx += 1)
	    *cp++ = __bitval_to_char(bus->wstrb[idx]);

      strcpy(cp, " BREADY=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->bready);

      strcpy(cp, " ARVALID=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arvalid);

      strcpy(cp, " ARADDR=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->addr_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->araddr[idx]);

      strcpy(cp, " ARLEN=");
      cp += strlen(cp);
      for (idx = 0 ; idx < 8 ; idx += 1)
	    *cp++ = __bitval_to_char(bus->arlen[idx]);

      strcpy(cp, " ARSIZE=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arsize[0]);
      *cp++ = __bitval_to_char(bus->arsize[1]);
      *cp++ = __bitval_to_char(bus->arsize[2]);

      strcpy(cp, " ARBURST=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arburst[0]);
      *cp++ = __bitval_to_char(bus->arburst[1]);

      strcpy(cp, " ARLOCK=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arlock[0]);
      *cp++ = __bitval_to_char(bus->arlock[1]);

      strcpy(cp, " ARCACHE=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arcache[0]);
      *cp++ = __bitval_to_char(bus->arcache[1]);
      *cp++ = __bitval_to_char(bus->arcache[2]);
      *cp++ = __bitval_to_char(bus->arcache[3]);

      strcpy(cp, " ARPROT=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arprot[0]);
      *cp++ = __bitval_to_char(bus->arprot[1]);
      *cp++ = __bitval_to_char(bus->arprot[2]);

      strcpy(cp, " ARQOS=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->arqos[0]);
      *cp++ = __bitval_to_char(bus->arqos[1]);
      *cp++ = __bitval_to_char(bus->arprot[2]);
      *cp++ = __bitval_to_char(bus->arprot[3]);

      strcpy(cp, " ARID=");
      cp += strlen(cp);
      for (idx = 0 ; idx < bus->rid_width ; idx += 1)
	    *cp++ = __bitval_to_char(bus->arid[idx]);

      strcpy(cp, " RREADY=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(bus->rready);

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

      assert(strcmp(argv[0],"UNTIL") == 0);

	/* Parse the time token */
      assert(argc >= 1);
      __parse_time_token(argv[1], &bus->time_mant, &bus->time_exp);


      for (idx = 2 ; idx < argc ; idx += 1) {
	    cp = strchr(argv[idx],'=');
	    assert(cp && *cp=='=');

	    *cp++ = 0;

	    if (strcmp(argv[idx],"ACLK") == 0) {
		  bus->aclk = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"AWREADY") == 0) {
		  bus->awready = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"WREADY") == 0) {
		  bus->wready = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"BVALID") == 0) {
		  bus->bvalid = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"BRESP") == 0) {
		  assert(strlen(cp) >= 2);
		  bus->bresp[0] = __char_to_bitval(cp[0]);
		  bus->bresp[1] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"BID") == 0) {
		  int bdx;
		  for (bdx = 0 ; bdx<AXI4_MAX_ID && cp[bdx] ; bdx += 1)
			bus->bid[idx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"ARREADY") == 0) {
		  bus->arready = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"RVALID") == 0) {
		  bus->rvalid = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"RDATA") == 0) {
		  int bdx;
		  for (bdx = 0 ; bdx<AXI4_MAX_DATA && cp[bdx] ; bdx += 1)
			bus->rdata[bdx] = __char_to_bitval(cp[bdx]);

	    } else if (strcmp(argv[idx],"RRESP") == 0) {
		  assert(strlen(cp) >= 2);
		  bus->rresp[0] = __char_to_bitval(cp[0]);
		  bus->rresp[1] = __char_to_bitval(cp[1]);

	    } else if (strcmp(argv[idx],"RID") == 0) {
		  int bdx;
		  for (bdx = 0 ; bdx<AXI4_MAX_ID && cp[bdx] ; bdx += 1)
			bus->rid[idx] = __char_to_bitval(cp[bdx]);

	    } else {
		  ; /* Skip signals not of interest to me. */
	    }
      }

      return 0;
}

void __axi4_next_posedge(simbus_axi4_t bus)
{
	/* Wait for the clock to fall... */
      while (bus->aclk != BIT_0) { /* ACLK==1/X/Z */
	    __axi4_ready_command(bus);
      }

	/* And wait for it to go high again. */
      while (bus->aclk != BIT_1) { /* ACLK==0/X/Z */
	    __axi4_ready_command(bus);
      }
}

simbus_axi4_t simbus_axi4_connect(const char*server, const char*name,
				  size_t data_width, size_t addr_width,
				  size_t wid_width, size_t rid_width)
{
      int server_fd = __simbus_server_socket(server);
      assert(server_fd >= 0);

      assert(data_width <= AXI4_MAX_DATA);
      assert(addr_width <= AXI4_MAX_ADDR);

      char arg_dw[64];
      char arg_aw[64];
      char arg_ww[64];
      char arg_rw[64];
      snprintf(arg_dw, sizeof arg_dw, "data_width=%zd", data_width);
      snprintf(arg_aw, sizeof arg_aw, "addr_width=%zd", addr_width);
      snprintf(arg_ww, sizeof arg_aw, "wid_width=%zd",  wid_width);
      snprintf(arg_rw, sizeof arg_aw, "rid_width=%zd",  wid_width);
      char*args[4];
      args[0] = arg_dw;
      args[1] = arg_aw;
      args[2] = arg_ww;
      args[3] = arg_rw;

      unsigned ident = 0;
      int rc = __simbus_server_hello(server_fd, name, &ident, 4, args);

      if (rc < 0)
	    return 0;

      struct simbus_axi4_s*bus = calloc(1, sizeof(struct simbus_axi4_s));
      assert(bus);
      init_simbus_axi4(bus);
      bus->name = strdup(name);
      bus->fd = server_fd;
      bus->ident = ident;
      bus->data_width = data_width;
      bus->addr_width = addr_width;
      bus->wid_width  = wid_width;
      bus->rid_width  = rid_width;

      return bus;
}

void simbus_axi4_debug(simbus_axi4_t bus, FILE*fd)
{
      bus->debug = fd;
}

int simbus_axi4_wait(simbus_axi4_t bus, unsigned clks)
{
      if (clks == 0)
	    return 0;

      while (clks > 0) {
	    __axi4_next_posedge(bus);
	    clks -= 1;
      }

      return 0;
}

void simbus_axi4_reset(simbus_axi4_t bus, unsigned width, unsigned settle)
{
      assert(width > 0);
      assert(settle > 0);

      bus->areset_n = BIT_0;
      simbus_axi4_wait(bus, width);

      bus->areset_n = BIT_1;
      simbus_axi4_wait(bus, settle);
}

void simbus_axi4_end_simulation(simbus_axi4_t bus)
{
	/* Send the FINISH command */
      __simbus_server_finish(bus->fd);
	/* Clean up connection. */
      simbus_axi4_disconnect(bus);
}

void simbus_axi4_disconnect(simbus_axi4_t bus)
{
      close(bus->fd);
      free(bus->name);
      free(bus);
}
