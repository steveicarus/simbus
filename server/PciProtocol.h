#ifndef __PciProtocol_H
#define __PciProtocol_H
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

# include  "protocol.h"

class PciProtocol  : public protocol_t {

    public:
      PciProtocol(struct bus_state&);
      ~PciProtocol();

      void run_init();
      void run_run();

    private:
      void advance_pci_clock_(void);
      bit_state_t calculate_reset_n_(void);
      void arbitrate_(void);
      void route_interrupts_(void);
      void blend_bi_signals_(void);

    private:
      typedef struct {
	    bit_state_t clk_val;
	    uint64_t duration_ps;
      } clock_phase_map_t;

      static const clock_phase_map_t clock_phase_map33[4];
      static const clock_phase_map_t clock_phase_map66[4];

      const clock_phase_map_t*clock_phase_map_;

	// Current state of the PCI clock. (It toggles.)
      int phase_;
	// Device that is currently granted, or -1 if none.
      int granted_;
};

#endif
