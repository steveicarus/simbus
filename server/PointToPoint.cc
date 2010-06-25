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

# include  "PointToPoint.h"
# include  <iostream>
# include  <cassert>

using namespace std;

PointToPoint::PointToPoint(struct bus_state*b)
: protocol_t(b)
{
      phase_ = 0;
      master_clock_mode_ = CLOCK_RUN;

      string opt_width = b->options["WIDTH"];
      if (! opt_width.empty()) {
	    wid_i_ = strtoul(opt_width.c_str(), 0, 0);
	    wid_o_ = wid_i_;
      } else {
	    wid_i_ = 0;
	    wid_o_ = 0;
      }

      opt_width = b->options["WIDTH_O"];
      if (! opt_width.empty()) {
	    wid_o_ = strtoul(opt_width.c_str(), 0, 0);
      }

      opt_width = b->options["WIDTH_I"];
      if (! opt_width.empty()) {
	    wid_i_ = strtoul(opt_width.c_str(), 0, 0);
      }

      string clock_high_str  = b->options["CLOCK_high"];
      string clock_low_str   = b->options["CLOCK_low"];
      string clock_hold_str  = b->options["CLOCK_hold"];
      string clock_setup_str = b->options["CLOCK_setup"];

      uint64_t clock_hold = strtoul(clock_hold_str.c_str(), 0, 10);
      uint64_t clock_high = strtoul(clock_high_str.c_str(), 0, 10);
      assert(clock_hold > 0 && clock_hold < clock_high);

      uint64_t clock_setup = strtoul(clock_setup_str.c_str(), 0, 10);
      uint64_t clock_low   = strtoul(clock_low_str.c_str(), 0, 10);
      assert(clock_setup > 0 && clock_setup < clock_low);

      clock_phase_map_[0] = clock_hold;
      clock_phase_map_[1] = clock_high - clock_hold;
      clock_phase_map_[2] = clock_low - clock_setup;
      clock_phase_map_[3] = clock_setup;
}

PointToPoint::~PointToPoint()
{
}

string PointToPoint::clock_mode_string_(clock_mode_t mode)
{
      switch (mode) {
	  case CLOCK_RUN:
	    return "Run";
	  case CLOCK_STOP_0:
	    return "Stop-0";
	  case CLOCK_STOP_1:
	    return "Stop-1";
	  case CLOCK_STOP_Z:
	    return "HiZ";
	  default:
	    return "????";
      }
}

void PointToPoint::trace_init()
{
      make_trace_("CLOCK", PT_BITS);
      make_trace_("CLOCK_MODE", PT_STRING);
      if (wid_i_ > 0)
	    make_trace_("DATA_I", PT_BITS, wid_i_);
      if (wid_o_ > 0)
	    make_trace_("DATA_O", PT_BITS, wid_o_);
}

void PointToPoint::run_init()
{
      assert(device_map().size() == 2);

      bus_device_map_t::iterator dev0 = device_map().begin();
      bus_device_map_t::iterator dev1 = dev0;
      dev1 ++;

      assert(dev1 != device_map().end());
      assert(dev1->second->host_flag != dev0->second->host_flag);

      if (dev0->second->host_flag) {
	    master_ = dev0;
	    slave_  = dev1;
      } else {
	    master_ = dev1;
	    slave_  = dev0;
      }

      master_->second->send_signals["CLOCK"].resize(1);
      master_->second->send_signals["CLOCK"][0] = BIT_1;

      master_->second->send_signals["DATA_I"].resize(wid_i_);
      for (unsigned idx = 0 ; idx < wid_i_ ; idx += 1)
	    master_->second->send_signals["DATA_I"][idx] = BIT_Z;

      slave_->second->send_signals["CLOCK"].resize(1);
      slave_->second->send_signals["CLOCK"][0] = BIT_1;

      slave_->second->send_signals["DATA_O"].resize(wid_o_);
      for (unsigned idx = 0 ; idx < wid_o_ ; idx += 1)
	    slave_->second->send_signals["DATA_O"][idx] = BIT_Z;

      set_trace_("CLOCK", BIT_1);
      set_trace_("CLOCK_MODE", clock_mode_string_(master_clock_mode_));
}

void PointToPoint::run_run()
{
      advance_bus_clock_();

	// The bus clock is high for phase 0 and 1, and low for phases
	// 2 and 3.
      bit_state_t bus_clk = phase_/2 ? BIT_0 : BIT_1;

      master_->second->send_signals["CLOCK"][0] = bus_clk;

      switch (master_clock_mode_) {
	  case CLOCK_RUN:
	    slave_ ->second->send_signals["CLOCK"][0] = bus_clk;
	    break;
	  case CLOCK_STOP_0:
	    slave_ ->second->send_signals["CLOCK"][0] = BIT_0;
	    break;
	  case CLOCK_STOP_1:
	    slave_ ->second->send_signals["CLOCK"][0] = BIT_1;
	    break;
	  case CLOCK_STOP_Z:
	    slave_ ->second->send_signals["CLOCK"][0] = BIT_Z;
	    break;
      }

	// Interpret the CLOCK_MODE signal from the master. This is
	// output-only from the master, the slave as no such control
      valarray<bit_state_t>&clock_mode = master_->second->client_signals["CLOCK_MODE"];
      if (clock_mode.size() > 0) {
	    unsigned val = 0;
	    for (unsigned idx = 0 ; idx < clock_mode.size() ; idx += 1) {
		  if (clock_mode[idx] == BIT_1)
			val |= 1<<idx;
	    }
	    switch (val) {
		case 0:
		  master_clock_mode_ = CLOCK_RUN;
		  break;
		case 1:
		  master_clock_mode_ = CLOCK_STOP_0;
		  break;
		case 2:
		  master_clock_mode_ = CLOCK_STOP_1;
		  break;
		case 3:
		  master_clock_mode_ = CLOCK_STOP_Z;
		  break;
		default:
		  master_clock_mode_ = CLOCK_RUN;
		  break;
	    }
      } else {
	    master_clock_mode_ = CLOCK_RUN;
      }

	// Copy DATA_O from master to slave...
      valarray<bit_state_t>master_data_o = master_->second->client_signals["DATA_O"];
      valarray<bit_state_t> data_o (wid_o_);
      for (unsigned idx = 0 ; idx < wid_o_ ; idx += 1) {
	    data_o[idx] = BIT_Z;
	    if (idx < master_data_o.size())
		  data_o[idx] = master_data_o[idx];
      }
      slave_->second->send_signals["DATA_O"] = data_o;

	// Copy DATA_I from slave to master...
      valarray<bit_state_t>slave_data_i = slave_->second->client_signals["DATA_I"];
      valarray<bit_state_t> data_i (wid_i_);
      for (unsigned idx = 0 ; idx < wid_i_ ; idx += 1) {
	    data_i[idx] = BIT_Z;
	    if (idx < slave_data_i.size())
		  data_i[idx] = slave_data_i[idx];
      }
      master_->second->send_signals["DATA_I"] = data_i;

      set_trace_("CLOCK", bus_clk);
      set_trace_("CLOCK_MODE", clock_mode_string_(master_clock_mode_));
      if (wid_o_) set_trace_("DATA_O", data_o);
      if (wid_i_) set_trace_("DATA_I", data_i);
}

void PointToPoint::advance_bus_clock_(void)
{
	// Advance the phase pointer
      phase_ = (phase_ + 1) % 4;

	// Advance time for the next phase. (Note that the table times
	// are in pico-seconds.
      advance_time_(clock_phase_map_[phase_], -12);
}

