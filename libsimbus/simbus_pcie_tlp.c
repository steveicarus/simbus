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
# include  <unistd.h>
# include  <assert.h>

/*
 * The BUS/dev/fun for the root device (me) and the endpoint device
 * (the remote) are fixed by the design of the simbus simulation.
 * These are the values:
 *
 *  Root id: 1/31/0
 *  endpoint id: passed by the caller
 */
# define DEFAULT_ROOT_ID 0x01f8


static void init_simbus_pcie_tlp(simbus_pcie_tlp_t bus)
{
      bus->debug = 0;
      bus->time_mant = 0;
      bus->time_exp = 0;

      bus->user_clk = BIT_1;
      bus->user_reset_out = BIT_0;
      bus->user_lnk_up = BIT_1;

      bus->request_id = DEFAULT_ROOT_ID;

      bus->tx_buf_av[5] = BIT_0;
      bus->tx_buf_av[4] = BIT_1;
      bus->tx_buf_av[3] = BIT_0;
      bus->tx_buf_av[2] = BIT_0;
      bus->tx_buf_av[1] = BIT_0;
      bus->tx_buf_av[0] = BIT_0;

      for (size_t idx = 0 ; idx < 64 ; idx += 1)
	    bus->m_axis_rx_tdata[idx] = BIT_X;
      for (size_t idx = 0 ; idx < 8 ; idx += 1)
	    bus->m_axis_rx_tkeep[idx] = BIT_X;
      bus->m_axis_rx_tlast = BIT_0;
      bus->m_axis_rx_tvalid = BIT_0;
      bus->m_axis_rx_tready = BIT_0;

      bus->s_axis_tx_tready = BIT_1;
}

simbus_pcie_tlp_t simbus_pcie_tlp_connect(const char*server, const char*name)
{
      int server_fd = __simbus_server_socket(server);
      if (server_fd < 0)
	    return 0;

      unsigned ident = 0;
      int rc = __simbus_server_hello(server_fd, name, &ident, 0, 0);

      if (rc < 0)
	    return 0;

      struct simbus_pcie_tlp_s*bus = calloc(1, sizeof(struct simbus_pcie_tlp_s));
      assert(bus);
      init_simbus_pcie_tlp(bus);
      bus->name = strdup(name);
      bus->fd = server_fd;
      bus->ident = ident;

      bus->s_tlp_cnt = 0;
      bus->tlp_next_tag = 0;

      for (int idx = 0 ; idx < 256 ; idx += 1)
	    bus->completions[idx] = 0;

      return bus;
}

void simbus_pcie_tlp_debug(simbus_pcie_tlp_t bus, FILE*debug)
{
      bus->debug = debug;
}

void simbus_pcie_tlp_disconnect(simbus_pcie_tlp_t bus)
{
      close(bus->fd);
      free(bus->name);
      free(bus);
}

void simbus_pcie_tlp_end_simulation(simbus_pcie_tlp_t bus)
{
	/* Send the FINISH command */
      __simbus_server_finish(bus->fd);
	/* Clean up connection. */
      simbus_pcie_tlp_disconnect(bus);
}

/*
 * This function sends to the server all the output signal values in a
 * READY command, then waits for an UNTIL command where I get back the
 * resolved values.
 */
static int send_ready_command(simbus_pcie_tlp_t bus)
{
      int rc;
      char buf[4096];
      snprintf(buf, sizeof(buf), "READY %" PRIu64 "e%d", bus->time_mant, bus->time_exp);

      char*cp = buf + strlen(buf);

      cp += __ready_signal(cp, "user_reset",  &bus->user_reset_out, 1);
      cp += __ready_signal(cp, "user_lnk_up", &bus->user_lnk_up,    1);
      cp += __ready_signal(cp, "tx_buf_av",   bus->tx_buf_av,       6);

      cp += __ready_signal(cp, "m_axis_rx_tdata", bus->m_axis_rx_tdata, 64);
      cp += __ready_signal(cp, "m_axis_rx_tkeep", bus->m_axis_rx_tkeep,  8);
      cp += __ready_signal(cp, "m_axis_rx_tlast", &bus->m_axis_rx_tlast, 1);
      cp += __ready_signal(cp, "m_axis_rx_tvalid",&bus->m_axis_rx_tvalid,1);

      cp += __ready_signal(cp, "s_axis_tx_tready",&bus->s_axis_tx_tready,1);

      if (bus->debug) {
	    *cp = 0;
	    fprintf(bus->debug, "%s: SEND %s\n", bus->name, buf);
      }

      *cp++ = '\n';
      *cp = 0;

      char*argv[2048];
      int argc = __simbus_server_send_recv(bus->fd, buf, sizeof(buf),
					   2048, argv, bus->debug);

      if (argc == 0) {
	    if (bus->debug) {
		  fprintf(bus->debug, "%s: Abort by error on stream\n", bus->name);
		  fflush(bus->debug);
	    }
	    return SIMBUS_PCIE_TLP_FINISHED;
      }

      if (strcmp(argv[0],"FINISH") == 0) {
	    if (bus->debug) {
		  fprintf(bus->debug, "%s: Abort by FINISH command\n", bus->name);
		  fflush(bus->debug);
	    }
	    return SIMBUS_PCIE_TLP_FINISHED;
      }

      assert(strcmp(argv[0],"UNTIL") == 0);

	/* Parse the time token */
      assert(argc >= 1);
      __parse_time_token(argv[1], &bus->time_mant, &bus->time_exp);


      int idx;
      for (idx = 2 ; idx < argc ; idx += 1) {
	    cp = strchr(argv[idx],'=');
	    assert(cp && *cp=='=');

	    *cp++ = 0;

	    if (strcmp(argv[idx],"user_clk") == 0) {
		  __until_signal(cp, &bus->user_clk, 1);

	    } else if (strcmp(argv[idx],"m_axis_rx_tready") == 0) {
		  __until_signal(cp, &bus->m_axis_rx_tready, 1);

	    } else if (strcmp(argv[idx],"s_axis_tx_tdata") == 0) {
		  __until_signal(cp, bus->s_axis_tx_tdata, 64);

	    } else if (strcmp(argv[idx],"s_axis_tx_tkeep") == 0) {
		  __until_signal(cp, bus->s_axis_tx_tkeep, 8);

	    } else if (strcmp(argv[idx],"s_axis_tx_tlast") == 0) {
		  __until_signal(cp, &bus->s_axis_tx_tlast, 1);

	    } else if (strcmp(argv[idx],"s_axis_tx_tvalid") == 0) {
		  __until_signal(cp, &bus->s_axis_tx_tvalid, 1);

	    } else if (strcmp(argv[idx],"s_axis_tx_tuser") == 0) {
		  __until_signal(cp, bus->s_axis_tx_tuser, 4);

	    } else {
		    /* Skip signals not of interest to me */
	    }
      }

      return 0;
}

void __pcie_tlp_next_posedge(simbus_pcie_tlp_t bus)
{
      int idx;

	/* If the clock is already high, wait for it to go low. */
      while (bus->user_clk == BIT_1) {
	    send_ready_command(bus);
      }

	/* Once the clock is low, wait for it to go high again. */
      while (bus->user_clk == BIT_0) {
	    send_ready_command(bus);
      }

	/* See about any TLP data coming in. */
      __pcie_tlp_recv_tlp(bus);
}

int simbus_pcie_tlp_wait(simbus_pcie_tlp_t bus, unsigned clks, int*irq_mask)
{
      int return_mask = 0;
      int enable_mask = irq_mask? *irq_mask : 0;

      if (clks == 0) {
	    if (irq_mask) *irq_mask = enable_mask & bus->intx_mask;
	    return 0;
      }

      while (clks > 0 && return_mask==0) {
	    __pcie_tlp_next_posedge(bus);
	    clks -= 1;

	    return_mask = enable_mask & bus->intx_mask;
      }

      if (irq_mask) *irq_mask = return_mask;
      return clks;
}

uint8_t __pcie_tlp_choose_tag(simbus_pcie_tlp_t bus)
{
      uint8_t res = bus->tlp_next_tag;

      bus->tlp_next_tag = (bus->tlp_next_tag + 1) % 32;

      return res;
}

void simbus_pcie_tlp_write_handle(simbus_pcie_tlp_t bus,
				  simbus_pcie_tlp_write_t fun,
				  simbus_pcie_tlp_cookie_t cookie)
{
      bus->write_fun = fun;
      bus->write_cookie = cookie;
}

void simbus_pcie_tlp_read_handle(simbus_pcie_tlp_t bus,
				 simbus_pcie_tlp_read_t fun,
				 simbus_pcie_tlp_cookie_t cookie)
{
      bus->read_fun = fun;
      bus->read_cookie = cookie;
}

int simbus_pcie_tlp_buf_avail(simbus_pcie_tlp_t bus, int nbuf)
{
      assert(nbuf >= 0);

	/* Saturate at the max possible value. */
      if (nbuf > 0x3f) nbuf = 0x3f;

      for (int idx = 0 ; idx < 6 ; idx += 1)
	    bus->tx_buf_av[idx] =  (nbuf & (1 << idx))? BIT_1 : BIT_0;

      return nbuf;
}
