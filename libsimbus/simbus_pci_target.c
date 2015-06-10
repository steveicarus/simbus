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

# include  "simbus_pci.h"
# include  "simbus_pci_priv.h"
# include  "simbus_priv.h"
# include  <unistd.h>
# include  <limits.h>
# include  <stdlib.h>
# include  <string.h>
# include  <stdio.h>
# include  <assert.h>

/*
 * Return true if the frame master is requesting a 64bit cycle and the
 * target device supports 64bit transactions.
 */
static inline int pci64_mode(simbus_pci_t pci, const struct simbus_translation*bar)
{ return (bar->flags&SIMBUS_XLATE_FLAG64) && pci->pci_req64_n == BIT_0; }


static int get_command(simbus_pci_t pci)
{
      int rc = 0;
      if (pci->pci_c_be[3] == BIT_1)
	    rc |= 0x08;
      if (pci->pci_c_be[2] == BIT_1)
	    rc |= 0x04;
      if (pci->pci_c_be[1] == BIT_1)
	    rc |= 0x02;
      if (pci->pci_c_be[0] == BIT_1)
	    rc |= 0x01;

      return rc;
}

static uint64_t get_data64(simbus_pci_t pci)
{
      uint64_t rc = 0;
      uint64_t mask = 1;
      int idx;
      for (idx = 0, mask=1 ; idx < 64 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] == BIT_1) rc |= mask;
      }

      return rc;
}

static uint64_t get_addr32(simbus_pci_t pci)
{
      uint64_t rc = 0;
      uint64_t mask = 1;
      int idx;
      for (idx = 0, mask=1 ; idx < 32 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] == BIT_1) rc |= mask;
      }

      return rc;
}

/*
 * This function collects the received address. Get the address bits
 * from AD[31:0], then check if there was a DAC cycle before. If there
 * was, then put these new bits up high to make a 64bit address.
 *
 * NOTE: In pci, we can rely on 64bit addresses always coming in a
 * DAC, even when the bus is 64bits. This allows 32bit devices to
 * co-exist on the bus.
 */
static uint64_t get_addr(simbus_pci_t pci)
{
      uint64_t rc = 0;
      uint64_t mask = 1;
      int idx;
      for (idx = 0, mask=1 ; idx < 32 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] == BIT_1) rc |= mask;
      }

      if (pci->target_state != TARG_DAC)
	    return rc;

	/* If there was a DAC, then build the complete address */
      rc  = (rc << 32) + pci->dac_addr_lo;

      return rc;
}

static int get_idsel(simbus_pci_t pci)
{
      assert(pci->ident < 16);
      if (pci->pci_ad[16 + pci->ident] == BIT_1)
	    return 1;
      else
	    return 0;
}

/*
 * Read the PCI-X attribute word from the AD lines. It is up to the
 * caller to know that the attribute is ready.
 */
static uint32_t get_xattr(simbus_pci_t pci)
{
      uint32_t rc = 0;
      uint32_t mask = 1;
      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1, mask <<= 1) {
	    if (pci->pci_ad[idx] == BIT_1) rc |= mask;
      }

      return rc;
}

static int get_c_be(simbus_pci_t pci, int bytes)
{
      int rc = 0;
      int mask = 1;
      int idx;
      for (idx = 0 ; idx < bytes ; idx += 1, mask <<= 1) {
	    if (pci->pci_c_be[idx] == BIT_1) rc |= mask;
      }
      return rc;
}

static const struct simbus_translation*match_mem_target(simbus_pci_t pci)
{
      int idx;
      uint64_t addr = get_addr(pci);

      for (idx = 0 ; idx < TARGET_MEM_REGIONS ; idx += 1) {
	    const struct simbus_translation*cur = pci->mem_target+idx;
	    if (cur->mask == 0)
		  continue;

	    if ((cur->mask & cur->base) == (cur->mask & addr))
		  return cur;
      }

      return 0;
}

/*
 * Process a Dual Address Command by stashing the low 32bits of the
 * address and changing the state. The real command after this will be
 * able to notice the DAC state when building its address.
 */
static void do_target_DAC(simbus_pci_t pci)
{
      pci->dac_addr_lo = get_addr32(pci);
      pci->target_state = TARG_DAC;
}

/*
 * An attempt to read from the config space leads me to complete the
 * config cycle.
 */
static void do_target_config_read(simbus_pci_t pci)
{
      uint64_t addr = get_addr32(pci) & 0x0000ffff;
      uint32_t val = 0xffffffff;

      if (pci->config_need32)
	    val = pci->config_need32(pci, addr, 0);

	/* Emit DEVSEL# but drive TRDY# high to insert a turnaround
	   cycle for the AD bus. */
      pci->out_devsel_n = BIT_0;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

      __pci_next_posedge(pci);

	/* Drive TRDY# and the AD. */
      pci->out_trdy_n = BIT_0;

      int idx;
      for (idx = 0 ; idx < 32 ; idx += 1, val >>= 1)
	    pci->out_ad[idx] = (val&1) ? BIT_1 : BIT_0;
      for (idx = 32 ; idx < 64; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

	/* Wait for the master to read the data. */
      do {
	    __pci_next_posedge(pci);
      } while (pci->pci_irdy_n == BIT_1);

      pci->out_devsel_n = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;
	/* This is turnaround time for AD. */
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

      __pci_next_posedge(pci);

	/* Release the bus and settle. */
      __undrive_bus(pci);
#if 0
      __pci_half_clock(pci);
      __pci_half_clock(pci);
#endif
      pci->target_state = TARG_IDLE;
}

static void do_target_config_write(simbus_pci_t pci)
{
      assert(pci->pci_clk == BIT_1);

      uint64_t addr = get_addr32(pci) & 0x0000ffff;
	/* Emit DEVSEL# and TRDY# . */
      pci->out_devsel_n = BIT_0;
      pci->out_trdy_n = BIT_0;
      pci->out_stop_n = BIT_1;

	/* Wait for the master to be ready with the data. */
      do {
	    __pci_next_posedge(pci);
      } while (pci->pci_irdy_n == BIT_1);

      uint32_t val = get_addr32(pci);
      int BEn = 0;
      if (pci->pci_c_be[0] == BIT_1) BEn |= 1;
      if (pci->pci_c_be[1] == BIT_1) BEn |= 2;
      if (pci->pci_c_be[2] == BIT_1) BEn |= 4;
      if (pci->pci_c_be[3] == BIT_1) BEn |= 8;

      if (pci->config_recv32)
	    pci->config_recv32(pci, addr, val, BEn);

      pci->out_devsel_n = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

      __pci_next_posedge(pci);

      pci->out_devsel_n = BIT_Z;
      pci->out_trdy_n   = BIT_Z;
      pci->out_stop_n   = BIT_Z;

      pci->target_state = TARG_IDLE;
}

static void do_target_memory_read(simbus_pci_t pci, const struct simbus_translation*bar)
{
      int idx;
      uint64_t addr = get_addr(pci);
      uint32_t valL = 0xffffffff;
      uint32_t valH = 0xffffffff;
      int byte_count = 0;
      int word_count = 0;
      int burst_len = 0;
      int word_size = 4;

	/* If this is a PCI-X bus, then there is an additional ATTR
	   phase that contains the attribute word. */
      if (pcix_mode(pci)) {
	    __pci_next_posedge(pci);
	    uint32_t attr = get_xattr(pci);

	      /* byte count is included in attribute */
	    byte_count = get_command(pci) << 8;
	    byte_count |= attr & 0xff;

	      /* Decide if this is a 64bit or 32bit data cycle, and
		 use that to calculate the word count. */
	    if (pci64_mode(pci, bar) && (addr%4 + byte_count > 4)) {
		  word_size = 8;
		  word_count = (addr%8 + byte_count + 7) / 8;

	    } else {
		  word_size = 4;
		  word_count = (addr%4 + byte_count + 3) / 4;
	    }
      }

	/* Emit DEVSEL# but drive TRDY# high to insert a turnaround
	   cycle for the AD bus. */
      pci->out_devsel_n = BIT_0;
      pci->out_ack64_n = word_size==8? BIT_0 : BIT_1;
      pci->out_stop_n = BIT_1;
      if (pcix_mode(pci)) __pci_next_posedge(pci);
      pci->out_trdy_n = BIT_1;

      __pci_next_posedge(pci);

      do {
	      /* The C/BE# contains byte enables only if this is NOT
		 PCI-X mode. */
	    int BEnL = pcix_mode(pci) ? 0 : get_c_be(pci, 4);
	    int BEnH = 0;
	    if (bar->need32) {
		  valL = bar->need32(pci, addr&~3, BEnL);
		  if (word_size==8)
			valH = bar->need32(pci, (addr+4)&~3, BEnH);
	    }

	      /* Drive TRDY# and the AD. */
	    pci->out_trdy_n = BIT_0;

	    for (idx = 0 ; idx < 32 ; idx += 1, valL >>= 1)
		  pci->out_ad[idx] = (valL&1) ? BIT_1 : BIT_0;
	    if (word_size==8) {
		  for (idx = 32 ; idx < 64 ; idx += 1, valH >>= 1)
			pci->out_ad[idx] = (valH&1)? BIT_1 : BIT_0;
	    } else {
		  for (idx = 32 ; idx < 64; idx += 1)
			pci->out_ad[idx] = BIT_Z;
	    }

	      /* Wait for the master to read the data. */
	    do {
		  __pci_next_posedge(pci);
	    } while (pci->pci_irdy_n == BIT_1);

	    addr += word_size;
	    burst_len += 1;

	    if (pcix_mode(pci) && burst_len == word_count)
		  break;

      } while (pci->pci_frame_n == BIT_0);

      if (pcix_mode(pci) && burst_len != word_count) {
	    fprintf(stderr, "simbus_pci ERROR: Expected to read %d words, but read %d.\n", word_count, burst_len);
      }

	/* De-assert target signals. */
      pci->out_devsel_n = BIT_1;
      pci->out_ack64_n  = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

	/* This is turnaround time for AD. */
      for (idx = 0 ; idx < 64 ; idx += 1)
	    pci->out_ad[idx] = BIT_Z;

      __pci_next_posedge(pci);

	/* Release the bus and settle. */
      __undrive_bus(pci);
      pci->target_state = TARG_IDLE;
}

static void make_ben4x(int*first_ben, int*last_ben, uint64_t addr, int byte_count)
{
      switch (addr%4) {
	  case 0:
	    switch (byte_count) {
		case 1:
		  *first_ben = 0xe;
		  break;
		case 2:
		  *first_ben = 0xc;
		  break;
		case 3:
		  *first_ben = 0x8;
		  break;
		default:
		  *first_ben = 0x0;
	    }
	    break;
	  case 1:
	    switch (byte_count) {
		case 1:
		 * first_ben = 0xd;
		  break;
		case 2:
		  *first_ben = 0x9;
		  break;
		default:
		  *first_ben = 0x1;
		  break;
	    }
	    break;
	  case 2:
	    switch (byte_count) {
		case 1:
		  *first_ben = 0xb;
		  break;
		default:
		  *first_ben = 0x3;
		  break;
	    }
	    break;
	  case 3:
	    *first_ben = 0x7;
	    break;
      }

      switch ((addr%4 + byte_count) % 4) {
	  case 0:
	    *last_ben = 0;
	    break;
	  case 1:
	    *last_ben = 0xe;
	    break;
	  case 2:
	    *last_ben = 0xc;
	    break;
	  case 3:
	    *last_ben = 0x8;
	    break;
      }

      *first_ben |= 0xf0;
      *last_ben  |= 0xf0;
}

static void make_ben8x(int*first_ben, int*last_ben, uint64_t addr, int byte_count)
{
      int ben1 = (-1) << addr%8;
      ben1 = 0xff & ~ben1;

      int ben2 = (-1) << (addr+byte_count)%8;
      ben2 = 0xff & ben2;
      if (ben2 == 0xff) ben2 = 0;

      if (addr%8 + byte_count < 8)
	    ben1 |= ben2;

      *first_ben = ben1;
      *last_ben = ben2;
}

static void do_target_memory_write(simbus_pci_t pci, const struct simbus_translation*bar)
{
      uint64_t addr = get_addr(pci);
      int byte_count = 0;
      int word_count = 0;
      int burst_len = 0;
      int first_ben = 0;
      int last_ben = 0;
      int word_size = 4;

	/* If this is a PCI-X bus, then there is an additional ATTR
	   phase that contains the attribute word. */
      if (pcix_mode(pci)) {
	    __pci_next_posedge(pci);
	    uint32_t attr = get_xattr(pci);

	      /* byte count is included in attribute */
	    byte_count = get_command(pci) << 8;
	    byte_count |= attr & 0xff;

	      /* Decide if this is a 64bit or 32bit data cycle, and
		 use that to calculate the word count and byte enables
		 that we want to use. */
	    if (pci64_mode(pci, bar) && (addr%4 + byte_count > 4)) {

		  word_size = 8;
		  word_count = (addr%8 + byte_count + 7) / 8;
		  make_ben8x(&first_ben, &last_ben, addr, byte_count);

	    } else {

		  word_size = 4;
		  word_count = (addr%4 + byte_count + 3) / 4;
		  make_ben4x(&first_ben, &last_ben, addr, byte_count);
	    }
      } else {
	    word_size = 4;
      }
      

	/* Emit DEVSEL# and TRDY# */
      pci->out_devsel_n = BIT_0;
      pci->out_ack64_n = word_size==8? BIT_0 : BIT_1;
      pci->out_stop_n = BIT_1;
      if (pcix_mode(pci)) __pci_next_posedge(pci);
      pci->out_trdy_n = BIT_0;

      do {
	      /* Wait for the master to be ready with the data. */
	    do {
		  __pci_next_posedge(pci);
	    } while (pci->pci_irdy_n == BIT_1);

	    if (bar->recv32) {
		  int pcix_ben;
		  if (burst_len == 0)
			pcix_ben = first_ben;
		  else if (burst_len+1 == word_count)
			pcix_ben = last_ben;
		  else
			pcix_ben = 0;
		  int BEn = pcix_mode(pci)? pcix_ben : get_c_be(pci, word_size);
		  if (word_size==4) {
			uint32_t val = get_addr32(pci);
			bar->recv32(pci, addr&~3, val, BEn);
		  } else {
			assert(word_size==8);
			uint64_t val = get_data64(pci);
			uint32_t val1 = val & 0xffffffff;
			val >>= 32;
			uint32_t val2 = val & 0xffffffff;
			int BEn1 = (BEn>>0)&15;
			int BEn2 = (BEn>>4)&15;
			bar->recv32(pci, (addr+0)&~3, val1, BEn1);
			bar->recv32(pci, (addr+4)&~3, val2, BEn2);
		  }
	    }

	    addr += word_size;
	    burst_len += 1;

	      /* If this is PCI-X, we know by word count when we are
		 done, we don't need to rely on FRAME# or IRDY#. */
	    if (pcix_mode(pci) && burst_len == word_count) {
		  if (pci->pci_frame_n == BIT_0 && word_count>2) {
			fprintf(stderr, "simbus_pci ERROR: "
				"FRAME# is lingering after burst!\n");
		  }
		  break;
	    }

	      /* If this is PCI-X, then the frame can be inactive for
		 the two last bursts, so allow for that. */
	      /* Otherwise, continue while the frame is active. */
      } while (pci->pci_frame_n == BIT_0
	       || (pcix_mode(pci) && burst_len+1 == word_count)
	       || (pcix_mode(pci) && burst_len+2 == word_count));


      if (pcix_mode(pci) && burst_len != word_count) {
	    fprintf(stderr, "simbus_pci ERROR: Expected to write %d words, but wrote %d.\n", word_count, burst_len);
      }

	/* De-assert target signals. */
      pci->out_devsel_n = BIT_1;
      pci->out_ack64_n = BIT_1;
      pci->out_trdy_n = BIT_1;
      pci->out_stop_n = BIT_1;

      __pci_next_posedge(pci);

	/* Release the bus and settle. */
      pci->out_devsel_n = BIT_Z;
      pci->out_ack64_n = BIT_Z;
      pci->out_trdy_n   = BIT_Z;
      pci->out_stop_n   = BIT_Z;

	/* Enforce a turnaround cycle. */
      __pci_next_posedge(pci);

      pci->target_state = TARG_IDLE;
}

static void do_target_memory_read_multiple(simbus_pci_t pci, const struct simbus_translation*bar)
{
      do_target_memory_read(pci, bar);
}

static void do_target_memory_read_line(simbus_pci_t pci, const struct simbus_translation*bar)
{
      do_target_memory_read(pci, bar);
}

/*
 * This function is called on the rising edge of any PCI clock. If
 * this device supports target operations, then handle them here.
 */
void __pci_target_state_machine(simbus_pci_t pci)
{
      switch (pci->target_state) {

	  case TARG_IDLE:
	    if (pci->pci_frame_n == BIT_0) {
		  int command = get_command(pci);
		  int idsel = get_idsel(pci);
		  const struct simbus_translation*bar = match_mem_target(pci);
#if 0
		  printf("XXXX __pci_target_state_machine: "
			 "TARG_IDLE: Got command=%x, idsel=%d, bar=0x%016" PRIx64 "\n",
			 command, idsel, bar? bar->base : 0);
#endif
		  if (bar && command == 0x06) {
			do_target_memory_read(pci, bar);

		  } else if (bar && command == 0x07) {
			do_target_memory_write(pci, bar);

		  } else if (idsel && command == 0x0a) {
			do_target_config_read(pci);

		  } else if (idsel && command == 0x0b) {
			do_target_config_write(pci);

		  } else if (bar && command == 0x0c) {
			do_target_memory_read_multiple(pci, bar);

		  } else if (command == 0x0d) {
			  /* Dual Address Cycle */
			do_target_DAC(pci);

		  } else if (bar && command == 0x0e) {
			do_target_memory_read_line(pci, bar);

		  } else if (pcix_mode(pci) && bar && command == 0x0f) {
			  /* Write block */
			do_target_memory_write(pci, bar);

		  } else {
			  /* Ignore any other cycles. */
			pci->target_state = TARG_BUS_BUSY;
		  }
	    }
	    break;

	  case TARG_DAC:
	      /* Only a limited set of commands are recognized after a
		 DAC cycle, so do a reduced check. */
	    if (pci->pci_frame_n == BIT_0) {
		  int command = get_command(pci);
		  const struct simbus_translation*bar = match_mem_target(pci);
#if 0
		  printf("XXXX __pci_target_state_machine: "
			 "TARG_DAC: Got command=%x, bar=0x%016" PRIx64 "\n",
			 command, bar? bar->base : 0);
#endif
		  if (bar && command == 0x06) {
			do_target_memory_read(pci, bar);

		  } else if (bar && command == 0x07) {
			do_target_memory_write(pci, bar);

		  } else if (bar && command == 0x0c) {
			do_target_memory_read_multiple(pci, bar);

		  } else if (bar && command == 0x0e) {
			do_target_memory_read_line(pci, bar);

		  } else if (pcix_mode(pci) && bar && command == 0x0f) {
			  /* Write block */
			do_target_memory_write(pci, bar);

		  } else {
			  /* Ignore any other cycles. */
			pci->target_state = TARG_BUS_BUSY;
		  }
	    } else {
		    /* FRAME# Went away after DAC? Release bus. */
		  pci->target_state = TARG_IDLE;
	    }
	    break;

	  case TARG_BUS_BUSY:
	    if (pci->pci_frame_n == BIT_1) {
		  pci->target_state = TARG_IDLE;
	    }
	    break;
      }
}

/*
 * Add a translation. Replace (or remove) any existing translation
 * with the new translation.
 */
void simbus_pci_mem_xlate(simbus_pci_t pci, unsigned idx,
			  const struct simbus_translation*drv)
{
      struct simbus_translation*cur;

      if (idx >= TARGET_MEM_REGIONS)
	    return;

      cur = &pci->mem_target[idx];
      if (drv == 0) {
	    memset(cur, 0, sizeof(*cur));
	    return;
      }

      memcpy(cur, drv, sizeof(*cur));
}
