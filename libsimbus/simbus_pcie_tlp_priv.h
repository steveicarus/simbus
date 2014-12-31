#ifndef __simbus_pcie_tlp_priv_H
#define __simbus_pcie_tlp_priv_H
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
 * This is the maximum number of words for a TLP, whatever the direction.
 */
# define MAX_TLP (1024+8)

struct simbus_pcie_tlp_s {
	/* The name given in the simbus_pci_connect function. This is
	   also the name sent to the server in order to get my id. */
      char*name;
	/* POSIX fd for the socket used to connect with the server. */
      int fd;
	/* Identifier returned by the server during connect. */
      unsigned ident;

	/* Support for receiving a TLP. */
      uint32_t s_tlp_buf[MAX_TLP];
      size_t s_tlp_cnt;

	/* Callback functions for read/write from the remote. */
      simbus_pcie_tlp_write_t write_fun;
      simbus_pcie_tlp_cookie_t write_cookie;

      simbus_pcie_tlp_read_t read_fun;
      simbus_pcie_tlp_cookie_t read_cookie;

	/* The bus structure automatically generates tags for
	   transactions, use this for the next tag. */
      uint8_t tlp_next_tag;

	/* Completions from the remote are collected as TLPs, and
	   stored here. A requestor expecting a completion can wait
	   it to show up here. */
      uint32_t*completions[256];

	/* Debug output file. */
      FILE*debug;

	/* Current simulation time. */
      uint64_t time_mant;
      int time_exp;

	/* Common Interface signals -- out, except for the clk. */
      bus_bitval_t user_clk;
      bus_bitval_t user_reset_out;
      bus_bitval_t user_lnk_up;

	/* Receive Interface signals -- out */
      bus_bitval_t m_axis_rx_tdata[64];
      bus_bitval_t m_axis_rx_tkeep[8];
      bus_bitval_t m_axis_rx_tlast;
      bus_bitval_t m_axis_rx_tvalid;
      bus_bitval_t m_axis_rx_tuser[22];
	/* Receive Interface signals -- in */
      bus_bitval_t m_axis_rx_tready;

	/* Transmit Interface signals -- out */
      bus_bitval_t s_axis_tx_tready;
	/* Transmit Interface signals -- in */
      bus_bitval_t s_axis_tx_tdata[64];
      bus_bitval_t s_axis_tx_tkeep[8];
      bus_bitval_t s_axis_tx_tlast;
      bus_bitval_t s_axis_tx_tvalid;
      bus_bitval_t s_axis_tx_tuser[4];
};

extern uint8_t __pcie_tlp_choose_tag(simbus_pcie_tlp_t bus);

/*
 * Wait for the next posedge of the transaction clock.
 */
extern void __pcie_tlp_next_posedge(simbus_pcie_tlp_t bus);

/*
 * Send a TLP to the PCIe remote. This function takes the assembled
 * TLP, arranged as 32bit words, maps it to the 64bit AXI4Stream and
 * clocks it out.
 */
extern void __pcie_tlp_send_tlp(simbus_pcie_tlp_t bus,
				const uint32_t*data, size_t ndata);

/*
 * Run the state machine for receiving TLPs from the slave.
 */
extern void __pcie_tlp_recv_tlp(simbus_pcie_tlp_t bus);

#endif
