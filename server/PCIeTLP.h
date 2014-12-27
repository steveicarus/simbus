#ifndef __PCIeTLP_H
#define __PCIeTLP_H
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
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  "protocol.h"

class PCIeTLP  : public protocol_t {

    public:
      PCIeTLP(struct bus_state*);
      ~PCIeTLP();

      void trace_init();
      void run_init();
      void run_run();

    private:
      void advance_bus_clock_(void);


    private:

	// Current state of the PCI clock. (It toggles.)
      int phase_;
      uint64_t clock_phase_map_[4];

	// The PCIe bus should have exactly two ends, which
	// we arbitrarily name master and slave.
      bus_device_map_t::iterator master_;
      bus_device_map_t::iterator slave_;
};

#endif
