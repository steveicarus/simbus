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

# include  "simbus_priv.h"

# define TARGET_MEM_REGIONS 8

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

	/* Callback functions to handle target cycles. */
      need32_fun_t config_need32;
      recv32_fun_t config_recv32;

	/* Description of target memory regions */
      struct simbus_translation mem_target[TARGET_MEM_REGIONS];

	/* Low bits of a 64bit DAC cycle */
      uint64_t dac_addr_lo;

      int break_flag;

      enum target_machine_e {
	    TARG_IDLE = 0,
	    TARG_DAC,
	    TARG_BUS_BUSY
      } target_state;
};

/*
 * Run target state machine.
 */
extern void __pci_target_state_machine(simbus_pci_t pci);

/*
 * Advance to the next PCI clock. After this function returns,
 * the pci_clk will be high.
 */
extern void __pci_next_posedge(simbus_pci_t pci);

extern void __pci_request_bus(simbus_pci_t pci);

/*
 * Transmit the FRAME#, the address and the command. If the address
 * needs 64bits then handle a DAC as well. If flag64!=0, then also
 * attempt to start a 64bit (data) transaction.
 */
extern void __address_command(simbus_pci_t pci, uint64_t addr, unsigned cmd, int flag64, int burst_flag);

/*
 * Wait for the DEVSEL#. Return <0 if it doesn't arrive.
 */
extern int __wait_for_devsel(simbus_pci_t pci);

extern void __setup_for_write(simbus_pci_t pci, uint64_t val, int BEn, int flag64);
extern int __wait_for_read(simbus_pci_t pci, uint64_t*val);

/*
 * The __generic_pci_read32 function performs a PCI read32 bus
 * transaction to read a single word. The cmd is the bus command to
 * use, and the BEn is the BE# values to use. The result is written
 * into result[0], and the return code is >=0 if the transaction
 * completes.
 *
 * If the transaction is terminated, the result is one of the GPCI
 * error codes, which are all <0.
 */
extern int __generic_pci_read32(simbus_pci_t pci, uint64_t addr, int cmd,
				int BEn, uint32_t*result);
# define GPCI_MASTER_ABORT (-1)
# define GPCI_TARGET_RETRY (-2)

extern int __generic_pci_write32(simbus_pci_t pci, uint64_t addr, int cmd,
				 uint32_t val, int BEn);

extern void __undrive_bus(simbus_pci_t pci);
#endif
