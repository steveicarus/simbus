/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
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

# include  "simbus_p2p.h"
# include  "simbus_p2p_priv.h"
# include  <unistd.h>
# include  <stdlib.h>
# include  <string.h>
# include  <assert.h>

simbus_p2p_t simbus_p2p_connect(const char*server, const char*name,
				unsigned width_i, unsigned width_o)
{
      assert(width_i > 0 || width_o > 0);

      int server_fd = __simbus_server_socket(server);
      assert(server_fd >= 0);

      unsigned ident = 0;
      int rc = __simbus_server_hello(server_fd, name, &ident);

      if (rc < 0)
	    return 0;

      struct simbus_p2p_s*bus = calloc(1, sizeof(struct simbus_p2p_s));
      assert(bus);

      bus->name = strdup(name);
      bus->fd = server_fd;
      bus->ident = ident;

      bus->time_mant = 0;
      bus->time_exp = 0;

      bus->clock = BIT_X;
      bus->clock_mode[0] = BIT_0;
      bus->clock_mode[1] = BIT_0;
      bus->width_i = width_i;
      bus->width_o = width_o;

      if (width_i > 0) {
	    unsigned idx;
	    bus->data_i = calloc(width_i, sizeof(bus->data_i[0]));
	    for (idx = 0 ; idx < width_i ; idx += 1)
		  bus->data_i[idx] = BIT_X;
      } else {
	    bus->data_i = 0;
      }
      if (width_o > 0) {
	    unsigned idx;
	    bus->data_o = calloc(width_o, sizeof(bus->data_o[0]));
	    for (idx = 0 ; idx < width_o ; idx += 1)
		  bus->data_o[idx] = BIT_X;
      } else {
	    bus->data_o = 0;
      }

      return bus;
}

int simbus_p2p_is_host(simbus_p2p_t bus)
{
      return bus->ident == 0;
}

void simbus_p2p_end_simulation(simbus_p2p_t bus)
{
	/* Send the FINISH command */
      __simbus_server_finish(bus->fd);
	/* Clean up connection. */
      simbus_p2p_disconnect(bus);
}

void simbus_p2p_disconnect(simbus_p2p_t bus)
{
      close(bus->fd);
      free(bus->name);
      free(bus->data_i);
      free(bus->data_o);
      free(bus);
}

void simbus_p2p_clock_mode(simbus_p2p_t bus, int mode)
{
      assert(bus->ident == 0);
      bus->clock_mode[0] = (mode&1)? BIT_1 : BIT_0;
      bus->clock_mode[1] = (mode&2)? BIT_1 : BIT_0;
}

int simbus_p2p_in(simbus_p2p_t bus, uint32_t*data)
{
      int rc = 0;
      unsigned idx;
      for (idx = 0 ; idx < bus->width_i ; idx += 1) {
	    int word = idx/32;
	    int wbit = idx%32;
	    if (wbit == 0)
		  data[word] = 0;

	    switch (bus->data_i[idx]) {
		case BIT_0:
		  break;
		case BIT_1:
		  data[word] |= 1 << wbit;
		  break;
		case BIT_X:
		case BIT_Z:
		  rc -= 1;
		  break;
	    }
      }

      return rc;
}

int simbus_p2p_out_peek(simbus_p2p_t bus, uint32_t*data)
{
      int rc = 0;
      unsigned idx;
      for (idx = 0 ; idx < bus->width_o ; idx += 1) {
	    int word = idx/32;
	    int wbit = idx%32;
	    if (wbit == 0)
		  data[word] = 0;

	    switch (bus->data_o[idx]) {
		case BIT_0:
		  break;
		case BIT_1:
		  data[word] |= 1 << wbit;
		  break;
		case BIT_X:
		case BIT_Z:
		  rc -= 1;
		  break;
	    }
      }

      return rc;
}

void simbus_p2p_out(simbus_p2p_t bus, const uint32_t*data)
{
      unsigned idx;

      for (idx = 0 ; idx < bus->width_o ; idx += 1) {
	    int word = idx/32;
	    int wbit = idx%32;
	    uint32_t mask = 1;
	    mask <<= wbit;

	    if (data[word] & mask)
		  bus->data_o[idx] = BIT_1;
	    else
		  bus->data_o[idx] = BIT_0;
      }

}

void simbus_p2p_in_poke(simbus_p2p_t bus, const uint32_t*data)
{
      unsigned idx;

      for (idx = 0 ; idx < bus->width_i ; idx += 1) {
	    int word = idx/32;
	    int wbit = idx%32;
	    uint32_t mask = 1;
	    mask <<= wbit;

	    if (data[word] & mask)
		  bus->data_i[idx] = BIT_1;
	    else
		  bus->data_i[idx] = BIT_0;
      }

}

static int send_ready_p2p(simbus_p2p_t bus)
{
      char buf[4096];

      snprintf(buf, sizeof(buf), "READY %" PRIu64 "e%d", bus->time_mant, bus->time_exp);

      char*cp = buf + strlen(buf);

      if (bus->ident == 0) { /* Only the host can send this */
	    strcpy(cp, " CLOCK_MODE=");
	    cp += strlen(cp);
	    *cp++ = __bitval_to_char(bus->clock_mode[1]);
	    *cp++ = __bitval_to_char(bus->clock_mode[0]);
      }

      if (bus->ident == 0 && bus->width_o > 0) {
	    unsigned idx;
	    strcpy(cp, " DATA_O=");
	    cp += strlen(cp);

	      /* Note that the protocol vector is MSB order. */
	    for (idx = 0 ; idx < bus->width_o ; idx += 1) {
		  *cp++ = __bitval_to_char(bus->data_o[bus->width_o-idx-1]);
	    }
      }

      if (bus->ident != 0 && bus->width_i > 0) {
	    unsigned idx;
	    strcpy(cp, " DATA_I=");
	    cp += strlen(cp);

	      /* Note that the protocol vector is MSB order. */
	    for (idx = 0 ; idx < bus->width_i ; idx += 1) {
		  *cp++ = __bitval_to_char(bus->data_i[bus->width_i-idx-1]);
	    }
      }

      *cp++ = '\n';
      *cp = 0;

      char*argv[2048];
      int argc = __simbus_server_send_recv(bus->fd, buf, sizeof(buf), 2048, argv, 0);

      if (strcmp(argv[0],"FINISH") == 0) {
	    return -1;
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

	    if (strcmp(argv[idx],"CLOCK") == 0) {
		  bus->clock = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx], "DATA_I") == 0) {
		  int top = strlen(cp);
		  assert(top > 0);

		    /* If vector is short, pad with X */
		  int pad;
		  for (pad = top ; pad < bus->width_i ; pad += 1)
			bus->data_i[pad] = BIT_X;

		  while (top > 0) {
			bus_bitval_t bit = __char_to_bitval(cp[top-1]);
			if (top <= bus->width_i)
			      bus->data_i[bus->width_i-top] = bit;
			top -= 1;
		  }

	    } else if (strcmp(argv[idx], "DATA_O") == 0) {
		  int top = strlen(cp);
		  assert(top > 0);

		    /* If vector is short, pad with X */
		  int pad;
		  for (pad = top ; pad < bus->width_o ; pad += 1)
			bus->data_o[pad] = BIT_X;

		  while (top > 0) {
			bus_bitval_t bit = __char_to_bitval(cp[top-1]);
			if (top <= bus->width_o)
			      bus->data_o[bus->width_o-top] = bit;
			top -= 1;
		  }

	    } else {
		    /* Skip uninteresting signals */
	    }
      }

      return 0;
}

int simbus_p2p_clock_posedge(simbus_p2p_t bus, unsigned cycles)
{
      int rc = 0;
      while (cycles > 0 && rc >= 0) {
	      /* Look for low CLOCK */
	    while (bus->clock != BIT_0 && rc >= 0)
		  rc = send_ready_p2p(bus);

	      /* Look for high CLOCK */
	    while (bus->clock != BIT_1 && rc >= 0)
		  rc = send_ready_p2p(bus);

	    cycles -= 1;
      }

      return rc;
}

int simbus_p2p_clock_negedge(simbus_p2p_t bus, unsigned cycles)
{
      int rc = 0;
      while (cycles > 0 && rc >= 0) {
	      /* Look for high CLOCK */
	    while (bus->clock != BIT_1 && rc >= 0)
		  rc = send_ready_p2p(bus);

	      /* Look for low CLOCK */
	    while (bus->clock != BIT_0 && rc >= 0)
		  rc = send_ready_p2p(bus);

	    cycles -= 1;
      }

      return rc;
}
