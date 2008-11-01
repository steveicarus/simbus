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

struct simbus_pci_s {
      char*name;
      int fd;
      unsigned ident;
};

simbus_pci_t simbus_pci_connect(const char*server, const char*name)
{
      fprintf(stderr, "simbus_pci_connect: STUB name=%s\n", name);

      char*host_name = 0;
      char*host_port = 0;

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

	/* Given the host string from the command line, look it up and
	   get the address and port numbers. */
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
      pci->name = strdup(name);
      pci->fd = server_fd;
      pci->ident = ident;

      return pci;
}

unsigned simbus_pci_wait(simbus_pci_t pci, unsigned clks, unsigned irq)
{
      fprintf(stderr, "simbus_pci_wait: STUB clks=%u, irq=0x%x\n", clks, irq);
      return 0;
}

void simbus_pci_reset(simbus_pci_t pci, unsigned width, unsigned settle)
{
      fprintf(stderr, "simbus_pci_reset: STUB width=%u, settle=%u\n", width, settle);
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
