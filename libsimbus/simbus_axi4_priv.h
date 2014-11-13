#ifndef __simbus_axi4_priv_H
#define __simbus_axi4_priv_H
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

# include  "simbus_priv.h"

/*
 * For debug purposes, limit the time (in ACLK) that we wait for a
 * response to certain tranactions. The standard 
 */
# define AXI4_RESP_TIMELIMIT 1024

/*
 * For the purposes of sizing structures in the library, define the
 * maximum width that these variable vectors may be.
 */
# define AXI4_MAX_ADDR 64
# define AXI4_MAX_DATA 64
# define AXI4_MAX_ID 64
# define AXI4_MAX_IRQ 32

struct simbus_axi4_s {
	/* The name given in the simbus_pci_connect function. This is
	   also the name sent to the server in order to get my id. */
      char*name;
	/* POSIX fd for the socket used to connect with the server. */
      int fd;
	/* Identifier returned by the server during connect. */
      unsigned ident;

	/* If this is a slave, then this pointer points to the
	   callback table for slave operations. */
      const struct simbus_axi4s_slave_s*device;

	/* Debug log */
      FILE*debug;

	/* Configured widths */
      size_t data_width;
      size_t addr_width;
      size_t wid_width;
      size_t rid_width;
      size_t irq_width;

	/* This is the AxSIZE for a bus word. This is the value that
	   is used if the bus is AXI-Lite or otherwise full-width
	   words. */
      uint8_t axsize_word;

      struct {
	    uint64_t waddr;
	    uint8_t  wsize;
	    uint8_t  wlen;
	    uint8_t  wprot;
	    uint8_t  wburst;

	    uint64_t raddr;
	    uint8_t  rsize;
	    uint8_t  rlen;
	    uint8_t  rprot;
	    uint8_t  rburst;
      } slave;

	/* Current simulation time. */
      uint64_t time_mant;
      int time_exp;

	/* Values that I writes to the server */
      bus_bitval_t areset_n;
	/* .. write address channel */
      bus_bitval_t awvalid;
      bus_bitval_t awaddr[AXI4_MAX_ADDR];
      bus_bitval_t awlen[8];
      bus_bitval_t awsize[3];
      bus_bitval_t awburst[2];
      bus_bitval_t awlock[2];
      bus_bitval_t awcache[4];
      bus_bitval_t awprot[3];
      bus_bitval_t awqos[4];
      bus_bitval_t awid[AXI4_MAX_ID];
	/* .. write data channel */
      bus_bitval_t wvalid;
      bus_bitval_t wdata[AXI4_MAX_DATA];
      bus_bitval_t wstrb[AXI4_MAX_DATA/8];
	/* .. write response channel */
      bus_bitval_t bready;
	/* .. read address channel */
      bus_bitval_t arvalid;
      bus_bitval_t araddr[AXI4_MAX_ADDR];
      bus_bitval_t arlen[8];
      bus_bitval_t arsize[3];
      bus_bitval_t arburst[2];
      bus_bitval_t arlock[2];
      bus_bitval_t arcache[4];
      bus_bitval_t arprot[3];
      bus_bitval_t arqos[4];
      bus_bitval_t arid[AXI4_MAX_ID];
	/* .. read data channel */
      bus_bitval_t rready;

	/* Values that I get back from the server */
      bus_bitval_t aclk;
	/* .. write address channel */
      bus_bitval_t awready;
	/* .. write data channel */
      bus_bitval_t wready;
	/* .. write response channel */
      bus_bitval_t bvalid;
      bus_bitval_t bresp[2];
      bus_bitval_t bid[AXI4_MAX_ID];
	/* .. read address channel */
      bus_bitval_t arready;
	/* .. read data channel */
      bus_bitval_t rvalid;
      bus_bitval_t rdata[AXI4_MAX_DATA];
      bus_bitval_t rresp[2];
      bus_bitval_t rid[AXI4_MAX_ID];
	/* .. interrupts */
      bus_bitval_t irq[AXI4_MAX_IRQ];
};

extern int __axi4_ready_command(simbus_axi4_t bus);
extern void __axi4_next_posedge(simbus_axi4_t bus);

#endif
