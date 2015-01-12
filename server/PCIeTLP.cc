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

# include  "PCIeTLP.h"
# include  <iostream>
# include  <cassert>

using namespace std;

/*
 * The clock_phase_map describes the phases of the clock. The phase
 * states are chosen to give clients a chance to participate in the
 * protocol in time.
 *
 *   Phase:         C        D A B        C
 *                  |        | | |        |
 *                  V        V V V        V
 *       +----------+          +----------+
 *       |          |          |          |
 *       |          |          |          |
 *  -----+          +----------+          +----------
 */

PCIeTLP::PCIeTLP(struct bus_state*b)
: protocol_t(b)
{
      phase_ = 0;

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

PCIeTLP::~PCIeTLP()
{
}

void PCIeTLP::trace_init()
{
      make_trace_("user_clk",    PT_BITS);
      make_trace_("user_reset",  PT_BITS);
      make_trace_("user_lnk_up", PT_BITS);

      make_trace_("m_axis_rx_tdata", PT_BITS, 64);
      make_trace_("m_axis_rx_tkeep", PT_BITS,  8);
      make_trace_("m_axis_rx_tlast", PT_BITS);
      make_trace_("m_axis_rx_tready",PT_BITS);
      make_trace_("m_axis_rx_tvalid",PT_BITS);

      make_trace_("s_axis_tx_tdata", PT_BITS, 64);
      make_trace_("s_axis_tx_tkeep", PT_BITS,  8);
      make_trace_("s_axis_tx_tlast", PT_BITS);
      make_trace_("s_axis_tx_tready",PT_BITS);
      make_trace_("s_axis_tx_tvalid",PT_BITS);
      make_trace_("s_axis_tx_tuser", PT_BITS,  4);

      make_trace_("tx_buf_av",       PT_BITS,  6);
}

void PCIeTLP::run_init()
{
	// PCIe links are onlt 2 devices: root and endpoint
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

	/* Common Interface signals */
      master_->second->send_signals["user_clk"].resize(1);
      master_->second->send_signals["user_clk"][0] = BIT_1;

      slave_->second->send_signals["user_clk"].resize(1);
      slave_->second->send_signals["user_clk"][0] = BIT_1;

      set_trace_("user_clk_out", BIT_1);

      slave_->second->send_signals["user_reset"].resize(1);
      slave_->second->send_signals["user_reset"][0] = BIT_1;

      set_trace_("user_reset", BIT_1);

      slave_->second->send_signals["user_lnk_up"].resize(1);
      slave_->second->send_signals["user_lnk_up"][0] = BIT_1;

      set_trace_("user_lnk_up", BIT_1);

      slave_->second->send_signals["tx_buf_av"].resize(6);
      for (size_t idx = 0 ; idx < 6 ; idx += 1)
	    slave_->second->send_signals["tx_buf_av"][idx] = BIT_1;

	/* Receive channel signals */
      slave_->second->send_signals["m_axis_rx_tdata"].resize(64);
      for (size_t idx = 0 ; idx < 64 ; idx += 1)
	    slave_->second->send_signals["m_axis_rx_tdata"][idx] = BIT_X;

      slave_->second->send_signals["m_axis_rx_tkeep"].resize(8);
      for (size_t idx = 0 ; idx < 8 ; idx += 1)
	    slave_->second->send_signals["m_axis_rx_tkeep"][idx] = BIT_X;

      slave_->second->send_signals["m_axis_rx_tlast"].resize(1);
      slave_->second->send_signals["m_axis_rx_tlast"][0] = BIT_1;

      master_->second->send_signals["m_axis_rx_tready"].resize(1);
      master_->second->send_signals["m_axis_rx_tready"][0] = BIT_X;
      set_trace_("m_axis_rx_tready", BIT_X);

      slave_->second->send_signals["m_axis_rx_tvalid"].resize(1);
      slave_->second->send_signals["m_axis_rx_tvalid"][0] = BIT_X;

	/* Transmit channel signals */
      master_->second->send_signals["s_axis_tx_tdata"].resize(64);
      for (size_t idx = 0 ; idx < 64 ; idx += 1)
	    master_->second->send_signals["s_axis_tx_tdata"][idx] = BIT_X;

      master_->second->send_signals["s_axis_tx_tkeep"].resize(8);
      for (size_t idx = 0 ; idx < 8 ; idx += 1)
	    master_->second->send_signals["s_axis_tx_tkeep"][idx] = BIT_X;

      master_->second->send_signals["s_axis_tx_tlast"].resize(1);
      master_->second->send_signals["s_axis_tx_tlast"][0] = BIT_1;

      slave_->second->send_signals["s_axis_tx_tready"].resize(1);
      slave_->second->send_signals["s_axis_tx_tready"][0] = BIT_X;

      master_->second->send_signals["s_axis_tx_tvalid"].resize(1);
      master_->second->send_signals["s_axis_tx_tvalid"][0] = BIT_X;

      master_->second->send_signals["s_axis_tx_user"].resize(22);
      for (size_t idx = 0 ; idx < 22 ; idx += 1)
	    master_->second->send_signals["s_axis_tx_user"][idx] = BIT_X;
}

void PCIeTLP::run_run()
{
	// Step the PCI clock.
      advance_bus_clock_();

	// The bus clock is high for phase 0 and 1, and low for phases
	// 2 and 3.
      bit_state_t bus_clk = phase_/2 ? BIT_0 : BIT_1;

      master_->second->send_signals["user_clk"][0] = bus_clk;
      slave_ ->second->send_signals["user_clk"][0] = bus_clk;
      set_trace_("user_clk",   bus_clk);

      valarray<bit_state_t>tmp;

      tmp = master_->second->client_signals["user_reset"];
      slave_->second->send_signals["user_reset"] = tmp;
      set_trace_("user_reset", tmp);

      tmp = master_->second->client_signals["user_lnk_up"];
      slave_->second->send_signals["user_lnk_up"] = tmp;
      set_trace_("user_lnk_up", tmp);

      tmp = master_->second->client_signals["tx_buf_av"];
      slave_->second->send_signals["tx_buf_av"] = tmp;
      set_trace_("tx_buf_av", tmp);

	/* Receive channel AXI4 Stream */
      tmp = master_->second->client_signals["m_axis_rx_tdata"];
      slave_->second->send_signals["m_axis_rx_tdata"] = tmp;
      set_trace_("m_axis_rx_tdata", tmp);

      tmp = master_->second->client_signals["m_axis_rx_tkeep"];
      slave_->second->send_signals["m_axis_rx_tkeep"] = tmp;
      set_trace_("m_axis_rx_tkeep", tmp);

      tmp = master_->second->client_signals["m_axis_rx_tlast"];
      slave_->second->send_signals["m_axis_rx_tlast"] = tmp;
      set_trace_("m_axis_rx_tlast", tmp);

      tmp = slave_->second->client_signals["m_axis_rx_tready"];
      master_->second->send_signals["m_axis_rx_tready"] = tmp;
      set_trace_("m_axis_rx_tready", tmp);

      tmp = master_->second->client_signals["m_axis_rx_tvalid"];
      slave_->second->send_signals["m_axis_rx_tvalid"] = tmp;
      set_trace_("m_axis_rx_tvalid", tmp);

	/* Transmit channel AXI4 Stream */
      tmp = slave_->second->client_signals["s_axis_tx_tdata"];
      master_->second->send_signals["s_axis_tx_tdata"] = tmp;
      set_trace_("s_axis_tx_tdata", tmp);

      tmp = slave_->second->client_signals["s_axis_tx_tkeep"];
      master_->second->send_signals["s_axis_tx_tkeep"] = tmp;
      set_trace_("s_axis_tx_tkeep", tmp);

      tmp = slave_->second->client_signals["s_axis_tx_tlast"];
      master_->second->send_signals["s_axis_tx_tlast"] = tmp;
      set_trace_("s_axis_tx_tlast", tmp);

      tmp = master_->second->client_signals["s_axis_tx_tready"];
      slave_->second->send_signals["s_axis_tx_tready"] = tmp;
      set_trace_("s_axis_tx_tready", tmp);

      tmp = slave_->second->client_signals["s_axis_tx_tvalid"];
      master_->second->send_signals["s_axis_tx_tvalid"] = tmp;
      set_trace_("s_axis_tx_tvalid", tmp);

      tmp = slave_->second->client_signals["s_axis_tx_tuser"];
      master_->second->send_signals["s_axis_tx_tuser"] = tmp;
      set_trace_("s_axis_tx_user", tmp);
}

void PCIeTLP::advance_bus_clock_(void)
{
	// Advance the phase pointer
      phase_ = (phase_ + 1) % 4;

	// Advance time for the next phase
      advance_time_(clock_phase_map_[phase_], -12);
}


