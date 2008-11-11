#ifndef __simbus_pci_priv_H
#define __simbus_pci_priv_H
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

typedef enum { BIT_0, BIT_1, BIT_X, BIT_Z } bus_bitval_t;

struct simbus_pci_s {
	/* The name given in the simbus_pci_connect function. This is
	   also the name sent to the server in order to get my id. */
      char*name;
	/* POSIX fd for the socket used to connect with the server. */
      int fd;
	/* Identifier returned by the server during connect. */
      unsigned ident;

	/* Debug output file. */
      FILE*debug;

	/* Current simulation time. */
      uint64_t time_mant;
      int time_exp;

	/* Values that I write to the server */
      bus_bitval_t out_reset_n;
      bus_bitval_t out_req_n;
      bus_bitval_t out_req64_n;
      bus_bitval_t out_frame_n;
      bus_bitval_t out_irdy_n;
      bus_bitval_t out_trdy_n;
      bus_bitval_t out_stop_n;
      bus_bitval_t out_devsel_n;
      bus_bitval_t out_ack64_n;
      bus_bitval_t out_c_be[8];
      bus_bitval_t out_ad[64];
      bus_bitval_t out_par;
      bus_bitval_t out_par64;

	/* values that I get back from the server */
      bus_bitval_t pci_clk;
      bus_bitval_t pci_gnt_n;
      bus_bitval_t pci_inta_n[16];
      bus_bitval_t pci_frame_n;
      bus_bitval_t pci_req64_n;
      bus_bitval_t pci_irdy_n;
      bus_bitval_t pci_trdy_n;
      bus_bitval_t pci_stop_n;
      bus_bitval_t pci_devsel_n;
      bus_bitval_t pci_ack64_n;
      bus_bitval_t pci_c_be[8];
      bus_bitval_t pci_ad[64];
      bus_bitval_t pci_par;
      bus_bitval_t pci_par64;
};


extern void __pci_request_bus(simbus_pci_t pci);

#endif
