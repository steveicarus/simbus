#ifndef __AXI4Protocol_H
#define __AXI4Protocol_H
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

# include  "protocol.h"

class AXI4Protocol  : public protocol_t {

    public:
      AXI4Protocol(struct bus_state*);
      ~AXI4Protocol();

	// This is called after the devices have all connected. Wrap
	// up the configuration and get ready for processing.
      bool wrap_up_configuration();
	// After wrap-up, this is called to make lxt2 traces. This is
	// only called if tracing is enabled.
      void trace_init();

      void run_init();
      void run_run();

    private:
      void advance_bus_clock_(void);

      void run_master_to_slave_(const char*trace, size_t bits);
      void run_slave_to_master_(const char*trace, size_t bits);

    private:
      unsigned data_width_;
      unsigned addr_width_;

	// The AXI4 bus should have exactly two ends, which
	// we name master and slave. The "host" is the master and the
	// device is the slave.
      bus_device_map_t::iterator master_;
      bus_device_map_t::iterator slave_;

	// Clock controls
      int phase_;
	// Timings for the phases, in ps.
      uint64_t clock_phase_map_[4];
};

#endif
