#ifndef __PointToPoint_H
#define __PointToPoint_H
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

# include  "protocol.h"

class PointToPoint  : public protocol_t {

    public:
      PointToPoint(struct bus_state&);
      ~PointToPoint();

      void run_init();
      void run_run();

    private:
      void advance_bus_clock_(void);


    private:
      int phase_;
	// Timings for the phases, in ps.
      uint64_t clock_phase_map_[4];

      enum clock_mode_t { CLOCK_RUN,
			  CLOCK_STOP_0,
			  CLOCK_STOP_1,
			  CLOCK_STOP_Z };
      clock_mode_t master_clock_mode_;
      static std::string clock_mode_string_(clock_mode_t);

      unsigned wid_o_;
      unsigned wid_i_;

	// The point-to-point bus should have exactly two ends, which
	// we arbitrarily name master and slave.
      bus_device_map_t::iterator master_;
      bus_device_map_t::iterator slave_;
};

#endif
