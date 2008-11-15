/*
 * Copyright (c) 2008 Stephen Williams (steve@icarus.com)
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

# include  "simbus_pci.h"
# include  "simbus_pci_priv.h"
# include  <sys/types.h>
# include  <sys/socket.h>
# include  <netdb.h>
# include  <unistd.h>
# include  <stdlib.h>
# include  <string.h>
# include  <stdio.h>
# include  <assert.h>


static const char bus_bitval_map[4] = { '0', '1', 'x', 'z' };
static bus_bitval_t char_to_bitval(char ch)
{
      switch (ch) {
	  case '0': return BIT_0;
	  case '1': return BIT_1;
	  case 'z': return BIT_Z;
	  default:  return BIT_X;
      }
}
static void init_simbus_pci(struct simbus_pci_s*pci)
{
      int idx;
      pci->debug = 0;
      pci->time_mant = 0;
      pci->time_exp = 0;

      pci->out_reset_n = BIT_1;
      pci->out_req_n = BIT_1;
      pci->out_req64_n = BIT_Z;
      pci->out_frame_n = BIT_Z;
      pci->out_irdy_n = BIT_Z;
      pci->out_trdy_n = BIT_Z;
      pci->out_stop_n = BIT_Z;
      pci->out_devsel_n = BIT_Z;
      pci->out_ack64_n = BIT_Z;
      for (idx = 0 ; idx < 8 ; idx += 1)
	    pci->out_c_be[idx] = BIT_Z;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;
      pci->out_par = BIT_Z;
      pci->out_par64 = BIT_Z;

      pci->pci_clk = BIT_X;
      pci->pci_gnt_n = BIT_X;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->pci_ad[idx] = BIT_X;

}

/*
 * This function sends to the server all the output signal values in a
 * READY command, then waits for an UNTIL command where I get back the
 * resolved values.
 */
static int send_ready_command(struct simbus_pci_s*pci)
{
      int rc;
      char buf[4096];
      snprintf(buf, sizeof(buf), "READY %lue%d", pci->time_mant, pci->time_exp);

      char*cp = buf + strlen(buf);

      strcpy(cp, " RESET#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_reset_n];

      strcpy(cp, " REQ#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_req_n];

      strcpy(cp, " REQ64#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_req64_n];

      strcpy(cp, " FRAME#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_frame_n];

      strcpy(cp, " IRDY#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_irdy_n];

      strcpy(cp, " TRDY#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_trdy_n];

      strcpy(cp, " STOP#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_stop_n];

      strcpy(cp, " DEVSEL#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_devsel_n];

      strcpy(cp, " ACK64#=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_ack64_n];

      strcpy(cp, " C/BE#=");
      cp += strlen(cp);
      for (rc = 0 ; rc < 8 ; rc += 1)
	    *cp++ = bus_bitval_map[pci->out_c_be[7-rc]];

      strcpy(cp, " AD=");
      cp += strlen(cp);
      for (rc = 0 ; rc < 64 ; rc += 1)
	    *cp++ = bus_bitval_map[pci->out_ad[63-rc]];

      strcpy(cp, " PAR=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_par];

      strcpy(cp, " PAR64=");
      cp += strlen(cp);
      *cp++ = bus_bitval_map[pci->out_par64];

      if (pci->debug) {
	    *cp = 0;
	    fprintf(pci->debug, "SEND %s\n", buf);
      }

	/* Terminate the message string. */
      *cp++ = '\n';
      *cp = 0;

      rc = write(pci->fd, buf, cp-buf);
      assert(rc == cp-buf);

	/* Now read the response, which should be an UNTIL command */
      rc = read(pci->fd, buf, sizeof(buf)-1);
      assert( rc >= 0 );

      buf[rc] = 0;

      cp = strchr(buf, '\n');
      assert(cp && *cp=='\n');

      *cp = 0;
      if (pci->debug) {
	    fprintf(pci->debug, "RECV %s\n", buf);
      }

      cp = buf;
      int argc = 0;
      char*argv[2048];

      cp = buf + strspn(buf, " ");
      while (*cp) {
	    argv[argc++] = cp;
	    cp += strcspn(cp, " ");
	    *cp++ = 0;
	    cp += strspn(cp, " ");
      }
      argv[argc] = 0;

      assert(strcmp(argv[0],"UNTIL") == 0);

	/* Parse the time token */
      assert(argc >= 1);
      pci->time_mant = strtoul(argv[1], &cp, 10);
      assert(*cp == 'e');
      cp += 1;
      pci->time_exp = strtol(cp, 0, 10);

      int idx;
      for (idx = 2 ; idx < argc ; idx += 1) {
	    cp = strchr(argv[idx],'=');
	    assert(cp && *cp=='=');

	    *cp++ = 0;

	    if (strcmp(argv[idx],"PCI_CLK") == 0) {
		  pci->pci_clk = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"GNT#") == 0) {
		  pci->pci_gnt_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"INTA#") == 0) {
		  assert(strlen(cp) == 16);
		  int bit;
		  for (bit = 0 ; bit < 16 ; bit += 1)
			pci->pci_inta_n[bit] = char_to_bitval(cp[15-bit]);

	    } else if (strcmp(argv[idx],"FRAME#") == 0) {
		  pci->pci_frame_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"REQ64#") == 0) {
		  pci->pci_req64_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"IRDY#") == 0) {
		  pci->pci_irdy_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"TRDY#") == 0) {
		  pci->pci_trdy_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"STOP#") == 0) {
		  pci->pci_stop_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"DEVSEL#") == 0) {
		  pci->pci_devsel_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"ACK64#") == 0) {
		  pci->pci_ack64_n = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"C/BE#") == 0) {
		  assert(strlen(cp) == 8);
		  int bit;
		  for (bit = 0 ; bit < 8 ; bit += 1) {
			assert(*cp);
			pci->pci_c_be[7-bit] = char_to_bitval(*cp);
			cp += 1;
		  }

	    } else if (strcmp(argv[idx],"AD") == 0) {
		  assert(strlen(cp) == 64);
		  int bit;
		  for (bit = 0 ; bit < 64 ; bit += 1) {
			assert(*cp);
			pci->pci_ad[63-bit] = char_to_bitval(*cp);
			cp += 1;
		  }

	    } else if (strcmp(argv[idx],"PAR") == 0) {
		  pci->pci_par = char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"PAR64") == 0) {
		  pci->pci_par64 = char_to_bitval(*cp);

	    } else {
		    /* Skip signals not of interest to me. */
	    }
      }

      return 0;
}

simbus_pci_t simbus_pci_connect(const char*server, const char*name)
{
      char*host_name = 0;
      char*host_port = 0;

	/* Parse the server string to a host name and port. If the
	   host name is missing, then assume the rest is the port and
	   use "localhost" as the server. */
      char*cp;
      if ( (cp = strchr(server, ':')) != 0 ) {
	    host_name = malloc(cp - server + 1);
	    strncpy(host_name, server, cp-server);
	    host_name[cp-server] - 0;

	    host_port = strdup(cp+1);
      } else {
	    host_name = strdup("localhost");
	    host_port = strdup(server);
      }

	/* Look up the host name and port to get the host/port
	   address. Try to connect to the service. */
      struct addrinfo hints, *res;
      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      int rc = getaddrinfo(host_name, host_port, &hints, &res);
      assert(rc == 0);

	/* Try to connect to the server. */
      int server_fd = -1;
      struct addrinfo *rp;
      for (rp = res ; rp != 0 ; rp = rp->ai_next) {
	      /* Make the right kind of socket. */
	    server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	    if (server_fd < 0)
		  continue;

	      /* Try to connect. */
	    if (connect(server_fd, rp->ai_addr, rp->ai_addrlen) < 0) {
		  close(server_fd);
		  server_fd = -1;
		  continue;
	    }

	    break;
      }

      freeaddrinfo(res);
      free(host_name);
      free(host_port);

      assert(server_fd >= 0);

      char buf[4096];

	/* Send HELLO message to the server. */
      snprintf(buf, sizeof buf, "HELLO %s\n", name);

      rc = write(server_fd, buf, strlen(buf));
      assert(rc == strlen(buf));

	/* Read response from server. */
      rc = read(server_fd, buf, sizeof buf);
      assert(rc > 0);
      assert(strchr(buf, '\n'));

	/* If the server NAKs me, then give up. */
      if (strcmp(buf, "NAK\n") == 0) {
	    close(server_fd);
	    return 0;
      }

      unsigned ident = 0;
      if (strncmp(buf, "YOU-ARE ", 8) == 0) {
	    sscanf(buf, "YOU-ARE %u", &ident);
      } else {
	    close(server_fd);
	    return 0;
      }

      struct simbus_pci_s*pci = calloc(1, sizeof(struct simbus_pci_s));
      assert(pci);
      init_simbus_pci(pci);
      pci->name = strdup(name);
      pci->fd = server_fd;
      pci->ident = ident;

      return pci;
}

void simbus_pci_debug(simbus_pci_t pci, FILE*debug)
{
      pci->debug = debug;
}

static bus_bitval_t bit_xor(bus_bitval_t a, bus_bitval_t b)
{
      if (a==BIT_X) return BIT_X;
      if (a==BIT_Z) return BIT_X;
      if (b==BIT_X) return BIT_X;
      if (b==BIT_Z) return BIT_X;

      if (a != b) return BIT_1;
      else return BIT_0;
}

void __pci_half_clock(simbus_pci_t pci)
{
      send_ready_command(pci);

      if (pci->pci_clk == BIT_1) {
	    int idx;

	      /* If the clock is 1, then we clocked out AD and C/BE#
		 values. Now we can calculate the PAR and PAR64 bits
		 that well me transmitted on the next clock. */

	    pci->out_par = BIT_Z;
	    for (idx = 0 ; idx < 32 && pci->out_par==BIT_Z ; idx += 1)
		  if (pci->out_ad[idx] != BIT_Z)
			pci->out_par = BIT_0;

	    if (pci->out_par != BIT_Z) {
		  for (idx = 0 ; idx < 32 ; idx += 1)
			pci->out_par = bit_xor(pci->out_par, pci->out_ad[idx]);
		  for (idx = 0 ; idx < 4 ; idx += 1)
			pci->out_par = bit_xor(pci->out_par, pci->out_c_be[idx]);
	    }

	      /* The 64bit bus signals work similarly. */
	    pci->out_par64 = BIT_Z;
	    for (idx = 32 ; idx < 64 && pci->out_par64==BIT_Z ; idx += 1)
		  if (pci->out_ad[idx] != BIT_Z)
			pci->out_par64 = BIT_0;

	    if (pci->out_par64 != BIT_Z) {
		  for (idx = 32 ; idx < 64 ; idx += 1)
			pci->out_par64 = bit_xor(pci->out_par64, pci->out_ad[idx]);
		  for (idx = 4 ; idx < 8 ; idx += 1)
			pci->out_par64 = bit_xor(pci->out_par64, pci->out_c_be[idx]);
	    }
      }
}

unsigned simbus_pci_wait(simbus_pci_t pci, unsigned clks, unsigned irq)
{

	/* Wait for the clock to go low, and to go high again. */
      assert(clks > 0);
      unsigned mask = 0;
      while (clks > 0 && ! (mask&irq)) {
	    while (pci->pci_clk != BIT_0)
		  send_ready_command(pci);

	    while (pci->pci_clk != BIT_1)
		  send_ready_command(pci);

	    clks -= 1;

	      /* Collect the interrupts that are now being drive. */
	    mask = 0;
	    int idx;
	    for (idx = 0 ; idx < 16 ; idx += 1) {
		  if (pci->pci_inta_n[idx] == BIT_0)
			mask |= 1<<idx;
	    }
      }

      return 0;
}

void simbus_pci_reset(simbus_pci_t pci, unsigned width, unsigned settle)
{
      assert(width > 0);
      assert(settle > 0);

      pci->out_reset_n = BIT_0;
      simbus_pci_wait(pci, width, 0);

      pci->out_reset_n = BIT_1;
      simbus_pci_wait(pci, settle, 0);
}

void __pci_request_bus(simbus_pci_t pci)
{
      int count = 32;
      pci->out_req_n = BIT_0;
      while (pci->pci_gnt_n != BIT_0) {
	    simbus_pci_wait(pci,1,0);
	    count -= 1;
	    assert(count > 0);
      }

      pci->out_req_n = BIT_1;
}

/*
 * Send the address and command...
 */
void __address_command32(simbus_pci_t pci, uint64_t addr, unsigned cmd)
{
      int idx;
      pci->out_req64_n = BIT_1;
      pci->out_frame_n = BIT_0;
      pci->out_irdy_n  = BIT_1;
      pci->out_trdy_n  = BIT_Z;
      pci->out_stop_n  = BIT_Z;
      pci->out_devsel_n= BIT_Z;

	/* The address. */
      uint64_t addr_tmp = addr;
      for (idx = 0 ; idx < 32 ; idx += 1) {
	    pci->out_ad[idx] = (addr_tmp&1)? BIT_1 : BIT_0;
	    addr_tmp >>= (uint64_t)1;
      }

	/* Ah, there is more address data. That means this is a 64bit
	   address and a DAC is necessary. Generate the DAC to clock
	   out the low bits, and let the remaining bits be taken care
	   of by the next clock. */
      if (addr_tmp != 0) {
	    pci->out_c_be[0] = BIT_1;
	    pci->out_c_be[1] = BIT_0;
	    pci->out_c_be[2] = BIT_1;
	    pci->out_c_be[3] = BIT_1;

	      /* Clock the DAC command and low address bits */
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);

	      /* Get the remaining address bits ready. */
	    for (idx = 0 ; idx < 32 ; idx += 1) {
		  pci->out_ad[idx] = (addr_tmp&1)? BIT_1 : BIT_0;
		  addr_tmp >>= (uint64_t)1;
	    }
       }

      for (idx = 0 ; idx < 4 ; idx += 1)
	    pci->out_c_be[idx] = (cmd & (1<<idx)) ? BIT_1 : BIT_0;

	/* Clock the Command and address */
      __pci_half_clock(pci);
      __pci_half_clock(pci);

	/* Stage the IRDY# */
      pci->out_frame_n = BIT_1;
      pci->out_irdy_n  = BIT_0;
}

int __wait_for_devsel(simbus_pci_t pci)
{
      if (pci->pci_devsel_n != BIT_0) { /* FAST decode... */
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
	    if (pci->pci_devsel_n != BIT_0) { /* SLOW decode... */
		  __pci_half_clock(pci);
		  __pci_half_clock(pci);
		  if (pci->pci_devsel_n != BIT_0) { /* Subtractive decode... */
			__pci_half_clock(pci);
			__pci_half_clock(pci);
			if (pci->pci_devsel_n != BIT_0) { /* give up. */
			      __undrive_bus(pci);
			      return -1;
			}
		  }
	    }
      }

      return 0;
}

int __wait_for_read32(simbus_pci_t pci, uint32_t*val)
{
      pci->out_frame_n = BIT_1;
      pci->out_req64_n = BIT_1;

	/* Wait for TRDY# */
      int count = 256;
      while (pci->pci_trdy_n != BIT_0) {
	    __pci_half_clock(pci);
	    __pci_half_clock(pci);
	    count -= 1;
	    assert(count > 0);
      }

	/* Collect the result read from the device. */
      uint32_t result = 0;
      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1) {
	    if (pci->pci_ad[idx] != BIT_0)
		  result |= 1 << idx;
      }

      *val = result;
      return 0;
}

void __undrive_bus(simbus_pci_t pci)
{
      int idx;

      pci->out_frame_n = BIT_Z;
      pci->out_req64_n = BIT_Z;
      pci->out_irdy_n  = BIT_Z;

      for (idx = 0 ; idx < 8 ; idx += 1)
	    pci->out_c_be[idx] = BIT_Z;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;
}

void simbus_pci_end_simulation(simbus_pci_t pci)
{
      fprintf(stderr, "simbus_pci_end_simulation: STUB\n");
      write(pci->fd, "FINISH\n", 7);
      close(pci->fd);
      free(pci->name);
      free(pci);
}
