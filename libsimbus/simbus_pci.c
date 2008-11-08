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
# include  <sys/types.h>
# include  <sys/socket.h>
# include  <netdb.h>
# include  <unistd.h>
# include  <stdlib.h>
# include  <string.h>
# include  <stdio.h>
# include  <assert.h>

typedef enum { BIT_0, BIT_1, BIT_X, BIT_Z } bus_bitval_t;
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

struct simbus_pci_s {
	/* The name given in the simbus_pci_connect function. This is
	   also the name sent to the server in order to get my id. */
      char*name;
	/* POSIX fd for the socket used to connect with the server. */
      int fd;
	/* Identifier returned by the server during connect. */
      unsigned ident;
	/* Current simulation time. */
      uint64_t time_mant;
      int time_exp;

	/* Values that I write to the server */
      bus_bitval_t out_reset_n;
      bus_bitval_t out_req_n;
      bus_bitval_t out_ad[64];

	/* values that I get back from the server */
      bus_bitval_t pci_clk;
      bus_bitval_t pci_gnt_n;
      bus_bitval_t pci_inta_n[16];
      bus_bitval_t pci_ad[64];
};

static void init_simbus_pci(struct simbus_pci_s*pci)
{
      int idx;
      pci->time_mant = 0;
      pci->time_exp = 0;

      pci->out_reset_n = BIT_1;
      pci->out_req_n = BIT_1;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

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

	    } else if (strcmp(argv[idx],"AD") == 0) {
		  for (idx = 0 ; idx < 64 ; idx += 1) {
			assert(*cp);
			pci->pci_ad[63-idx] = char_to_bitval(*cp);
			cp += 1;
		  }

	    } else {
		    /* Skip signals not of interest to me. */
	    }
      }

      return 0;
}

simbus_pci_t simbus_pci_connect(const char*server, const char*name)
{
      fprintf(stderr, "simbus_pci_connect: STUB name=%s\n", name);

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

unsigned simbus_pci_wait(simbus_pci_t pci, unsigned clks, unsigned irq)
{
      if (irq != 0) {
	    fprintf(stderr, "simbus_pci_wait: STUB irq=0x%x\n", irq);
      }

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

uint32_t simbus_pci_config_read(simbus_pci_t pci, uint64_t addr)
{
      fprintf(stderr, "simbus_pci_config_read: STUB addr=0x%x\n", addr);
      return 0xffffffff;
}

void simbus_pci_config_write(simbus_pci_t pci, uint64_t addr, uint32_t val, int BEn)
{
      fprintf(stderr, "simbus_pci_config_write: STUB addr=0x%x, val=%x, BE#=0x%xn", addr, val, BEn);
}

uint32_t simbus_pci_read32(simbus_pci_t pci, uint64_t addr)
{
      fprintf(stderr, "simbus_pci_read32: STUB addr=0x%x\n", addr);
      return 0xffffffff;
}

void simbus_pci_write32(simbus_pci_t pci, uint64_t addr, uint32_t val, int BEn)
{
      fprintf(stderr, "simbus_pci_write32: STUB addr=0x%x, val=%x, BE#=%x\n", addr, val, BEn);
}

void simbus_pci_end_simulation(simbus_pci_t pci)
{
      fprintf(stderr, "simbus_pci_end_simulation: STUB\n");
      close(pci->fd);
      free(pci->name);
      free(pci);
}
