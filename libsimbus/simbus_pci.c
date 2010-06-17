/*
 * Copyright (c) 2008-2010 Stephen Williams (steve@icarus.com)
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
# include  <unistd.h>
# include  <limits.h>
# include  <stdlib.h>
# include  <string.h>
# include  <stdio.h>
# include  <assert.h>


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

      pci->config_need32 = 0;
      pci->config_recv32 = 0;

      for (idx = 0 ; idx < TARGET_MEM_REGIONS ; idx += 1) {
	    memset(&pci->mem_target[idx], 0, sizeof(pci->mem_target[idx]));
      }

      pci->target_state = TARG_IDLE;
}

void simbus_pci_config_need32(simbus_pci_t pci, need32_fun_t fun)
{
      pci->config_need32 = fun;
}

void simbus_pci_config_recv32(simbus_pci_t pci, recv32_fun_t fun)
{
      pci->config_recv32 = fun;
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
      snprintf(buf, sizeof(buf), "READY %" PRIu64 "e%d", pci->time_mant, pci->time_exp);

      char*cp = buf + strlen(buf);

      strcpy(cp, " RESET#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_reset_n);

      strcpy(cp, " REQ#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_req_n);

      strcpy(cp, " REQ64#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_req64_n);

      strcpy(cp, " FRAME#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_frame_n);

      strcpy(cp, " IRDY#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_irdy_n);

      strcpy(cp, " TRDY#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_trdy_n);

      strcpy(cp, " STOP#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_stop_n);

      strcpy(cp, " DEVSEL#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_devsel_n);

      strcpy(cp, " ACK64#=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_ack64_n);

      strcpy(cp, " C/BE#=");
      cp += strlen(cp);
      for (rc = 0 ; rc < 8 ; rc += 1)
	    *cp++ = __bitval_to_char(pci->out_c_be[7-rc]);

      strcpy(cp, " AD=");
      cp += strlen(cp);
      for (rc = 0 ; rc < 64 ; rc += 1)
	    *cp++ = __bitval_to_char(pci->out_ad[63-rc]);

      strcpy(cp, " PAR=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_par);

      strcpy(cp, " PAR64=");
      cp += strlen(cp);
      *cp++ = __bitval_to_char(pci->out_par64);

      if (pci->debug) {
	    *cp = 0;
	    fprintf(pci->debug, "SEND %s\n", buf);
      }

	/* Terminate the message string. */
      *cp++ = '\n';
      *cp = 0;

      char*argv[2048];
      int argc = __simbus_server_send_recv(pci->fd, buf, sizeof(buf),
					   2048, argv, pci->debug);

      if (argc == 0) {
	    if (pci->debug) {
		  fprintf(pci->debug, "Abort by error on stream\n");
		  fflush(pci->debug);
	    }
	    return SIMBUS_PCI_FINISHED;
      }

      if (strcmp(argv[0],"FINISH") == 0) {
	    if (pci->debug) {
		  fprintf(pci->debug, "Abort by FINISH command\n");
		  fflush(pci->debug);
	    }
	    return SIMBUS_PCI_FINISHED;
      }

      assert(strcmp(argv[0],"UNTIL") == 0);

	/* Parse the time token */
      assert(argc >= 1);
      __parse_time_token(argv[1], &pci->time_mant, &pci->time_exp);


      int idx;
      for (idx = 2 ; idx < argc ; idx += 1) {
	    cp = strchr(argv[idx],'=');
	    assert(cp && *cp=='=');

	    *cp++ = 0;

	    if (strcmp(argv[idx],"PCI_CLK") == 0) {
		  pci->pci_clk = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"GNT#") == 0) {
		  pci->pci_gnt_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"INTA#") == 0) {
		  assert(strlen(cp) == 16);
		  int bit;
		  for (bit = 0 ; bit < 16 ; bit += 1)
			pci->pci_inta_n[bit] = __char_to_bitval(cp[15-bit]);

	    } else if (strcmp(argv[idx],"FRAME#") == 0) {
		  pci->pci_frame_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"REQ64#") == 0) {
		  pci->pci_req64_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"IRDY#") == 0) {
		  pci->pci_irdy_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"TRDY#") == 0) {
		  pci->pci_trdy_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"STOP#") == 0) {
		  pci->pci_stop_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"DEVSEL#") == 0) {
		  pci->pci_devsel_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"ACK64#") == 0) {
		  pci->pci_ack64_n = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"C/BE#") == 0) {
		  assert(strlen(cp) == 8);
		  int bit;
		  for (bit = 0 ; bit < 8 ; bit += 1) {
			assert(*cp);
			pci->pci_c_be[7-bit] = __char_to_bitval(*cp);
			cp += 1;
		  }

	    } else if (strcmp(argv[idx],"AD") == 0) {
		  assert(strlen(cp) == 64);
		  int bit;
		  for (bit = 0 ; bit < 64 ; bit += 1) {
			assert(*cp);
			pci->pci_ad[63-bit] = __char_to_bitval(*cp);
			cp += 1;
		  }

	    } else if (strcmp(argv[idx],"PAR") == 0) {
		  pci->pci_par = __char_to_bitval(*cp);

	    } else if (strcmp(argv[idx],"PAR64") == 0) {
		  pci->pci_par64 = __char_to_bitval(*cp);

	    } else {
		    /* Skip signals not of interest to me. */
	    }
      }

      return 0;
}

simbus_pci_t simbus_pci_connect(const char*server, const char*name)
{
      int server_fd = __simbus_server_socket(server);
      assert(server_fd >= 0);

      unsigned ident = 0;
      int rc = __simbus_server_hello(server_fd, name, &ident);

      if (rc < 0)
	    return 0;

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

void __pci_next_posedge(simbus_pci_t pci)
{
      int idx;

      while (pci->pci_clk == BIT_1) {
	    send_ready_command(pci);
      }

      while (pci->pci_clk == BIT_0) {
	    send_ready_command(pci);
      }

	/* On the clock posedge, we clocked out AD and C/BE#
	   values. Now we can calculate the PAR and PAR64 bits
	   that well me transmitted on the next clock. */

      pci->out_par = BIT_Z;
      for (idx = 0 ; idx < 32 && pci->out_par==BIT_Z ; idx += 1)
	    if (pci->out_ad[idx] != BIT_Z)
		  pci->out_par = BIT_0;

      if (pci->out_par != BIT_Z) {
	    for (idx = 0 ; idx < 32 ; idx += 1)
		  pci->out_par = bit_xor(pci->out_par, pci->out_ad[idx]);
	      /* Include the C/BE# signals in the parity. If we
		 are not driving the C/BE#, then assume this is
		 a target cycle and include the C/BE# values
		 driven from the outside. */
	    if (pci->out_c_be[0] == BIT_Z)
		  for (idx = 0 ; idx < 4 ; idx += 1)
			pci->out_par = bit_xor(pci->out_par, pci->pci_c_be[idx]);
	    else
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

int simbus_pci_wait(simbus_pci_t pci, unsigned clks, unsigned irq)
{
	/* Special case: if the clks is 0, then we are not really here
	   to wait, but just to test if there are any interrupts
	   pending. */
      if (clks == 0) {
	    unsigned mask = 0;
	    int idx;
	    for (idx = 0 ; idx < 16 ; idx += 1) {
		  if (pci->pci_inta_n[idx] == BIT_0)
			mask |= 1<<idx;
	    }

	    return mask & irq;
      }

      int rc = 0;
	/* Wait for the clock to go low, and to go high again. */
      assert(clks > 0);
      pci->break_flag = 0;
      unsigned mask = 0;
      while (clks > 0 && ! (mask&irq)) {
	    while (pci->pci_clk != BIT_0 && rc >= 0)
		  rc = send_ready_command(pci);

	    while (pci->pci_clk != BIT_1 && rc >= 0)
		  rc = send_ready_command(pci);

	    if (rc < 0) {
		  return rc;
	    }

	    clks -= 1;

	      /* Advance my target machine, if present. */
	    __pci_target_state_machine(pci);

	    if (pci->break_flag && pci->target_state==TARG_IDLE)
		  return SIMBUS_PCI_BREAK;

	      /* Collect the interrupts that are now being driven. */
	    mask = 0;
	    int idx;
	    for (idx = 0 ; idx < 16 ; idx += 1) {
		  if (pci->pci_inta_n[idx] == BIT_0)
			mask |= 1<<idx;
	    }
      }

      return mask & irq;
}

int simbus_pci_wait_break(simbus_pci_t pci)
{
      pci->break_flag = 1;
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
void __address_command(simbus_pci_t pci, uint64_t addr, unsigned cmd, int flag64, int burst_flag)
{
      int idx;
      pci->out_req64_n = flag64? BIT_0 : BIT_1;
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

	/* If this is intended to be a 64bit transaction, then fill in
	   the high bits of the address as well. */
      if (pci->out_req64_n == BIT_0) {
	    uint64_t highbits = addr_tmp;
	    for (idx = 32 ; idx < 64 ; idx += 1) {
		  pci->out_ad[idx] = (highbits&1) ? BIT_1 : BIT_0;
		  highbits >>= (uint64_t)1;
	    }
      }

	/* Ah, there is more address data. That means this is a 64bit
	   address and a DAC is necessary. (Even if this is a 64bit
	   cycle.) Generate the DAC to clock out the low bits, and let
	   the remaining bits be taken care of by the next clock. */
      if (addr_tmp != 0) {
	    pci->out_c_be[0] = BIT_1;
	    pci->out_c_be[1] = BIT_0;
	    pci->out_c_be[2] = BIT_1;
	    pci->out_c_be[3] = BIT_1;

	      /* Clock the DAC command and low address bits */
	    __pci_next_posedge(pci);

	      /* Get the remaining address bits ready. */
	    for (idx = 0 ; idx < 32 ; idx += 1) {
		  pci->out_ad[idx] = (addr_tmp&1)? BIT_1 : BIT_0;
		  addr_tmp >>= (uint64_t)1;
	    }
       }

      for (idx = 0 ; idx < 4 ; idx += 1)
	    pci->out_c_be[idx] = (cmd & (1<<idx)) ? BIT_1 : BIT_0;

	/* Clock the Command and address */
      __pci_next_posedge(pci);

	/* Stage the IRDY#. If this is a 32bit transaction, then at
	   the same time withdraw the FRAME# signal. If we are
	   requesting a 64bit transaction, then leave the FRAME#
	   active in case the target acknowledges only 32bits, and
	   this turns into a burst. */
      if (pci->out_req64_n!=BIT_0 && burst_flag==0)
	    pci->out_frame_n = BIT_1;

      pci->out_irdy_n  = BIT_0;
}

int __wait_for_devsel(simbus_pci_t pci)
{
      if (pci->pci_devsel_n != BIT_0) { /* FAST decode... */
	    __pci_next_posedge(pci);
	    if (pci->pci_devsel_n != BIT_0) { /* SLOW decode... */
		  __pci_next_posedge(pci);
		  if (pci->pci_devsel_n != BIT_0) { /* Subtractive decode... */
			__pci_next_posedge(pci);
			if (pci->pci_devsel_n != BIT_0) { /* give up. */
			      __undrive_bus(pci);
			      return GPCI_MASTER_ABORT;
			}
		  }
	    }
      }

	/* If the target is acknowledging a 64bit transaction, then
	   we can release the FRAME# and transmit the data in a single
	   clock. */
      if (pci->out_req64_n==BIT_0 && pci->pci_ack64_n==BIT_0) {
	    pci->out_frame_n = BIT_1;
	    pci->out_req64_n = BIT_1;
      }

      return 0;
}

int __wait_for_read(simbus_pci_t pci, uint64_t*val)
{

	/* Wait for TRDY# */
      int count = 256;
      while (pci->pci_trdy_n != BIT_0) {
	      /* If STOP# is low while TRDY# is high, then this is a Retry */
	    if (pci->pci_stop_n == BIT_0)
		  return GPCI_TARGET_RETRY;
	    __pci_next_posedge(pci);
	    count -= 1;
	    assert(count > 0);
      }

	/* Collect the result read from the device. */
      uint64_t result = 0;
      uint64_t mask = 1;
      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] != BIT_0)
		  result |= mask;
      }

      if (pci->pci_ack64_n == BIT_0) {
	    for (idx = 0 ; idx < 64 ; idx += 1, mask <<= 1) {
		  if (pci->pci_ad[idx] != BIT_0)
			result |= mask;
	    }
      }

      *val = result;
      return 0;
}

int __generic_pci_read32(simbus_pci_t pci, uint64_t addr, int cmd,
			 int BEn, uint32_t*result)
{
      int idx;
      int rc;

	/* Arbitrate for the bus. This may return immediately if the
	   bus is parked here, or it may return after some clocks and
	   a REQ#/GNT# handshake. */
      __pci_request_bus(pci);

      pci->out_req_n = BIT_1;

      __address_command(pci, addr, cmd, 0, 0);

	/* Collect the BE# bits. */
      pci->out_c_be[0] = BEn&0x1? BIT_1 : BIT_0;
      pci->out_c_be[1] = BEn&0x2? BIT_1 : BIT_0;
      pci->out_c_be[2] = BEn&0x4? BIT_1 : BIT_0;
      pci->out_c_be[3] = BEn&0x8? BIT_1 : BIT_0;
	/* Make sure address lines are undriven */
      for (idx = 0 ; idx < 64 ; idx += 1) {
	    pci->out_ad[idx] = BIT_Z;
      }

	/* Clock the IRDY and BE#s (and PAR), and un-drive the AD bits. */
      __pci_next_posedge(pci);

      if (__wait_for_devsel(pci) < 0) {
	    *result = 0xffffffff;
	    return GPCI_MASTER_ABORT;
      }

      pci->out_frame_n = BIT_1;
      pci->out_req64_n = BIT_1;

      uint64_t val;
      if ( (rc = __wait_for_read(pci, &val)) < 0) {
	      /* Release all the signals I've been driving. */
	    __undrive_bus(pci);

	      /* This clocks the drivers to the next state, and clocks in
		 the final parity from the target. */
	    __pci_next_posedge(pci);
	    *result = 0xffffffff;
	    return rc;
      }

      *result = val;

	/* Release all the signals I've been driving. */
      __undrive_bus(pci);
      __pci_next_posedge(pci);

	/* XXXX Here we should check the pci_par parity bit */

	/* Done. Return the result. */
      return 0;
}

void __setup_for_write(simbus_pci_t pci, uint64_t val, int BEn, int flag64)
{
      pci->out_c_be[0] = BEn&1 ? BIT_1 : BIT_0;
      pci->out_c_be[1] = BEn&2 ? BIT_1 : BIT_0;
      pci->out_c_be[2] = BEn&4 ? BIT_1 : BIT_0;
      pci->out_c_be[3] = BEn&8 ? BIT_1 : BIT_0;
      if (flag64) {
	    pci->out_c_be[4] = BEn&0x10 ? BIT_1 : BIT_0;
	    pci->out_c_be[5] = BEn&0x20 ? BIT_1 : BIT_0;
	    pci->out_c_be[6] = BEn&0x40 ? BIT_1 : BIT_0;
	    pci->out_c_be[7] = BEn&0x80 ? BIT_1 : BIT_0;
      } else {
	    pci->out_c_be[4] = BIT_1;
	    pci->out_c_be[5] = BIT_1;
	    pci->out_c_be[6] = BIT_1;
	    pci->out_c_be[7] = BIT_1;
      }

      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1, val >>= 1)
	    pci->out_ad[idx] = (val&1) ? BIT_1 : BIT_0;
      if (flag64) {
	    for (idx = 32; idx < 64 ; idx += 1, val >>= 1)
		  pci->out_ad[idx] = (val&1) ? BIT_1 : BIT_0;
      } else {
	    for (idx = 32 ; idx < 64; idx += 1)
		  pci->out_ad[idx] = BIT_Z;
      }

	/* Clock the IRDY and BE#s (and PAR). */
      __pci_next_posedge(pci);

}

void __undrive_bus(simbus_pci_t pci)
{
      int idx;

      pci->out_frame_n = BIT_Z;
      pci->out_req64_n = BIT_Z;
      pci->out_irdy_n  = BIT_Z;
      pci->out_trdy_n  = BIT_Z;
      pci->out_stop_n  = BIT_Z;
      pci->out_devsel_n= BIT_Z;

      for (idx = 0 ; idx < 8 ; idx += 1)
	    pci->out_c_be[idx] = BIT_Z;
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;
}

int __generic_pci_write32(simbus_pci_t pci, uint64_t addr, int cmd,
			  uint32_t val, int BEn)
{
      int rc;

	/* Arbitrate for the bus. This may return immediately if the
	   bus is parked here, or it may return after some clocks and
	   a REQ#/GNT# handshake. */
      __pci_request_bus(pci);

      pci->out_req_n = BIT_1;

      __address_command(pci, addr, cmd, 0, 0);

      __setup_for_write(pci, val, BEn, 0);

      if ( (rc = __wait_for_devsel(pci)) < 0)
	    return rc;

	/* Wait for the target to TRDY# in order to clock the data out. */
      while (pci->pci_trdy_n != BIT_0) {
	    __pci_next_posedge(pci);
      }

	/* Release the bus and settle. */
      __undrive_bus(pci);
      __pci_next_posedge(pci);

      return 0;
}

void simbus_pci_end_simulation(simbus_pci_t pci)
{
	/* Send the FINISH command */
      __simbus_server_finish(pci->fd);
	/* Clean up connection. */
      simbus_pci_disconnect(pci);
}

void simbus_pci_disconnect(simbus_pci_t pci)
{
      close(pci->fd);
      free(pci->name);
      free(pci);
}
