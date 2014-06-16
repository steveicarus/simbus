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

# include  "AXI4Protocol.h"
# include  <cassert>

using namespace std;

AXI4Protocol::AXI4Protocol(struct bus_state*b)
: protocol_t(b)
{
      phase_ = 0;

      data_width_ = 0;
      addr_width_ = 0;
      wid_width_ = 0;
      rid_width_ = 0;
      irq_width_ = 0;

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

AXI4Protocol::~AXI4Protocol()
{
}

bool AXI4Protocol::wrap_up_configuration()
{
      string str = get_option("data_width");
      data_width_ = strtoul(str.c_str(), 0, 0);

      str = get_option("addr_width");
      addr_width_ = strtoul(str.c_str(), 0, 0);

      str = get_option("wid_width");
      wid_width_ = strtoul(str.c_str(), 0, 0);

      str = get_option("rid_width");
      rid_width_ = strtoul(str.c_str(), 0, 0);

      str = get_option("irq_width");
      irq_width_ = strtoul(str.c_str(), 0, 0);

      if (data_width_ == 0)
	    data_width_ = 32;
      if (addr_width_ == 0)
	    addr_width_ = 32;
      if (wid_width_ == 0)
	    wid_width_ = 4;
      if (irq_width_ == 0)
	    irq_width_ = 1;

      return true;
}

void AXI4Protocol::trace_init()
{
	// global signals
      make_trace_("ACLK",    PT_BITS);
      make_trace_("ARESETn", PT_BITS);
	// write address channel
      make_trace_("AWVALID", PT_BITS);
      make_trace_("AWREADY", PT_BITS);
      make_trace_("AWADDR",  PT_BITS, addr_width_);
      make_trace_("AWLEN",   PT_BITS, 8);
      make_trace_("AWSIZE",  PT_BITS, 3);
      make_trace_("AWBURST", PT_BITS, 2);
      make_trace_("AWLOCK",  PT_BITS, 2);
      make_trace_("AWCACHE", PT_BITS, 4);
      make_trace_("AWPROT",  PT_BITS, 3);
      make_trace_("AWQOS",   PT_BITS, 4);
      make_trace_("AWID",    PT_BITS, wid_width_);
	// write data channel
      make_trace_("WVALID",  PT_BITS);
      make_trace_("WREADY",  PT_BITS);
      make_trace_("WDATA",   PT_BITS, data_width_);
      make_trace_("WSTRB",   PT_BITS, data_width_/8);
	// write response channel
      make_trace_("BVALID",  PT_BITS);
      make_trace_("BREADY",  PT_BITS);
      make_trace_("BRESP",   PT_BITS, 2);
      make_trace_("BID",     PT_BITS, wid_width_);
	// read address channel
      make_trace_("ARVALID", PT_BITS);
      make_trace_("ARREADY", PT_BITS);
      make_trace_("ARADDR",  PT_BITS, addr_width_);
      make_trace_("ARLEN",   PT_BITS, 8);
      make_trace_("ARSIZE",  PT_BITS, 3);
      make_trace_("ARBURST", PT_BITS, 2);
      make_trace_("ARLOCK",  PT_BITS, 2);
      make_trace_("ARCACHE", PT_BITS, 4);
      make_trace_("ARPROT",  PT_BITS, 3);
      make_trace_("ARQOS",   PT_BITS, 4);
      make_trace_("ARID",    PT_BITS, rid_width_);
	// read data channel
      make_trace_("RVALID",  PT_BITS);
      make_trace_("RREADY",  PT_BITS);
      make_trace_("RDATA",   PT_BITS, data_width_);
      make_trace_("RRESP",   PT_BITS, 2);
      make_trace_("RID",     PT_BITS, rid_width_);
	// Interrupts
      make_trace_("IRQ",     PT_BITS, irq_width_);
}

void AXI4Protocol::run_init()
{
	// AXI4 is point-to-point
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

      valarray<bit_state_t>tmp_1 (1);
      tmp_1[0] = BIT_1;

      valarray<bit_state_t>tmp_z (1);
      tmp_z[0] = BIT_Z;

      valarray<bit_state_t>tmp_addr (addr_width_);
      for (size_t idx = 0 ; idx < tmp_addr.size() ; idx += 1)
	    tmp_addr[idx] = BIT_Z;

      valarray<bit_state_t>tmp_data (data_width_);
      for (size_t idx = 0 ; idx < tmp_data.size() ; idx += 1)
	    tmp_data[idx] = BIT_Z;

      valarray<bit_state_t>tmp_strb (data_width_/8);
      for (size_t idx = 0 ; idx < tmp_strb.size() ; idx += 1)
	    tmp_strb[idx] = BIT_Z;

      valarray<bit_state_t>tmp_wid (wid_width_);
      for (size_t idx = 0 ; idx < tmp_wid.size() ; idx += 1)
	    tmp_wid[idx] = BIT_0;

      valarray<bit_state_t>tmp_rid (rid_width_);
      for (size_t idx = 0 ; idx < tmp_rid.size() ; idx += 1)
	    tmp_rid[idx] = BIT_0;

      valarray<bit_state_t>tmp_irq (irq_width_);
      for (size_t idx = 0 ; idx < tmp_irq.size() ; idx += 1)
	    tmp_irq[idx] = BIT_Z;

      valarray<bit_state_t>tmp_resp (2);
      for (size_t idx = 0 ; idx < tmp_resp.size() ; idx += 1)
	    tmp_resp[idx] = BIT_Z;

      valarray<bit_state_t>tmp_prot (3);
      for (size_t idx = 0 ; idx < tmp_prot.size() ; idx += 1)
	    tmp_prot[idx] = BIT_Z;

      valarray<bit_state_t>tmp_lock (2);
      for (size_t idx = 0 ; idx < tmp_resp.size() ; idx += 1)
	    tmp_resp[idx] = BIT_0;

      valarray<bit_state_t>tmp_cache (4);
      for (size_t idx = 0 ; idx < tmp_prot.size() ; idx += 1)
	    tmp_prot[idx] = BIT_0;

      valarray<bit_state_t>tmp_qos (4);
      for (size_t idx = 0 ; idx < tmp_qos.size() ; idx += 1)
	    tmp_qos[idx] = BIT_Z;

      valarray<bit_state_t>tmp_len (8);
      for (size_t idx = 0 ; idx < tmp_len.size() ; idx += 1)
	    tmp_len[idx] = BIT_Z;

	// global signals
      master_->second->send_signals["ACLK"]    = tmp_1;
      slave_ ->second->send_signals["ACLK"]    = tmp_1;
      slave_ ->second->send_signals["ARESETn"] = tmp_1;

      set_trace_("ACLK",    BIT_1);
      set_trace_("ARESETn", BIT_1);

	// write address channel
      slave_ ->second->send_signals["AWVALID"] = tmp_z;
      master_->second->send_signals["AWREADY"] = tmp_z;
      slave_ ->second->send_signals["AWADDR" ] = tmp_addr;
      slave_ ->second->send_signals["AWLEN"  ] = tmp_len;
      slave_ ->second->send_signals["AWSIZE" ] = tmp_prot;
      slave_ ->second->send_signals["AWBURST"] = tmp_resp;
      slave_ ->second->send_signals["AWLOCK" ] = tmp_lock;
      slave_ ->second->send_signals["AWCACHE"] = tmp_cache;
      slave_ ->second->send_signals["AWPROT" ] = tmp_prot;
      slave_ ->second->send_signals["AWQOS"  ] = tmp_qos;
      slave_ ->second->send_signals["AWID"   ] = tmp_wid;

      set_trace_("AWVALID", BIT_Z);
      set_trace_("AWREADY", BIT_Z);
      set_trace_("AWADDR",  tmp_addr);
      set_trace_("AWLEN",   tmp_len);
      set_trace_("AWSIZE",  tmp_prot);
      set_trace_("AWBURST", tmp_resp);
      set_trace_("AWLOCK",  tmp_lock);
      set_trace_("AWCACHE", tmp_cache);
      set_trace_("AWPROT",  tmp_prot);
      set_trace_("AWQOS",   tmp_qos);
      set_trace_("AWID",    tmp_wid);

	// write data channel
      slave_ ->second->send_signals["WVALID"] = tmp_z;
      master_->second->send_signals["WREADY"] = tmp_z;
      slave_ ->second->send_signals["WDATA" ] = tmp_data;
      slave_ ->second->send_signals["WSTRB" ] = tmp_strb;

      set_trace_("WVALID", BIT_Z);
      set_trace_("WREADY", BIT_Z);
      set_trace_("WDATA",  tmp_data);
      set_trace_("WSTRB",  tmp_strb);

	// write response channel
      master_->second->send_signals["BVALID"] = tmp_z;
      slave_ ->second->send_signals["BREADY"] = tmp_z;
      master_->second->send_signals["BRESP" ] = tmp_resp;
      master_->second->send_signals["BID"   ] = tmp_wid;

      set_trace_("BVALID", BIT_Z);
      set_trace_("BREADY", BIT_Z);
      set_trace_("BRESP",  tmp_resp);

	// read address channel
      slave_ ->second->send_signals["ARVALID"] = tmp_z;
      master_->second->send_signals["ARREADY"] = tmp_z;
      slave_ ->second->send_signals["ARADDR" ] = tmp_addr;
      slave_ ->second->send_signals["ARLEN"  ] = tmp_len;
      slave_ ->second->send_signals["ARSIZE" ] = tmp_prot;
      slave_ ->second->send_signals["ARBURST"] = tmp_resp;
      slave_ ->second->send_signals["ARLOCK" ] = tmp_lock;
      slave_ ->second->send_signals["ARCACHE"] = tmp_cache;
      slave_ ->second->send_signals["ARPROT" ] = tmp_prot;
      slave_ ->second->send_signals["ARQOS"  ] = tmp_qos;
      slave_ ->second->send_signals["ARID"   ] = tmp_rid;

      set_trace_("ARVALID", BIT_Z);
      set_trace_("ARREADY", BIT_Z);
      set_trace_("ARADDR",  tmp_addr);
      set_trace_("ARLEN",   tmp_len);
      set_trace_("ARSIZE",  tmp_prot);
      set_trace_("ARBURST", tmp_resp);
      set_trace_("ARLOCK",  tmp_lock);
      set_trace_("ARCACHE", tmp_cache);
      set_trace_("ARPROT",  tmp_prot);
      set_trace_("ARQOS",   tmp_qos);
      set_trace_("ARID",    tmp_rid);

	// read data channel
      master_->second->send_signals["RVALID"] = tmp_z;
      slave_ ->second->send_signals["RREADY"] = tmp_z;
      master_->second->send_signals["RDATA" ] = tmp_data;
      master_->second->send_signals["RRESP" ] = tmp_resp;
      master_->second->send_signals["RID"   ] = tmp_rid;
      master_->second->send_signals["IRQ"   ] = tmp_irq;

      set_trace_("RVALID", BIT_Z);
      set_trace_("RREADY", BIT_Z);
      set_trace_("RDATA",  tmp_data);
      set_trace_("RRESP",  tmp_resp);
      set_trace_("RID",    tmp_rid);
      set_trace_("IRQ",    tmp_irq);
}

void AXI4Protocol::run_master_to_slave_(const char*name, size_t bits)
{
      valarray<bit_state_t>tmp;

      tmp = master_->second->client_signals[name];
      assert(tmp.size() == bits);

      slave_->second->send_signals[name] = tmp;

      if (tmp.size()==1)
	    set_trace_(name, tmp[0]);
      else
	    set_trace_(name, tmp);
}

void AXI4Protocol::run_slave_to_master_(const char*name, size_t bits)
{
      valarray<bit_state_t>tmp;

      tmp = slave_->second->client_signals[name];
      assert(tmp.size() == bits);

      master_->second->send_signals[name] = tmp;

      if (tmp.size()==1)
	    set_trace_(name, tmp[0]);
      else
	    set_trace_(name, tmp);
}

void AXI4Protocol::run_run()
{
      advance_bus_clock_();

	// The bus clock is high for phase 0 and 1, and low for phases
	// 2 and 3. In any case, drive the clock to both devices.
      bit_state_t bus_clk = phase_/2 ? BIT_0 : BIT_1;

	// The ACLK is driven by the protocol server.
      master_->second->send_signals["ACLK"][0] = bus_clk;
      slave_ ->second->send_signals["ACLK"][0] = bus_clk;
      set_trace_("ACLK", bus_clk);

	// Global signals...
      run_master_to_slave_("ARESETn", 1);

	// write address channel
      run_master_to_slave_("AWVALID", 1);
      run_slave_to_master_("AWREADY", 1);
      run_master_to_slave_("AWADDR",  addr_width_);
      run_master_to_slave_("AWLEN",   8);
      run_master_to_slave_("AWSIZE",  3);
      run_master_to_slave_("AWBURST", 2);
      run_master_to_slave_("AWLOCK",  2);
      run_master_to_slave_("AWCACHE", 4);
      run_master_to_slave_("AWPROT",  3);
      run_master_to_slave_("AWQOS",   4);
      run_master_to_slave_("AWID",    wid_width_);

	// write data channel
      run_master_to_slave_("WVALID",  1);
      run_slave_to_master_("WREADY",  1);
      run_master_to_slave_("WDATA",   data_width_);
      run_master_to_slave_("WSTRB",   data_width_/8);

	// write response channel
      run_slave_to_master_("BVALID",  1);
      run_master_to_slave_("BREADY",  1);
      run_slave_to_master_("BRESP",   2);
      run_slave_to_master_("BID",     wid_width_);

	// read address channel
      run_master_to_slave_("ARVALID", 1);
      run_slave_to_master_("ARREADY", 1);
      run_master_to_slave_("ARADDR",  addr_width_);
      run_master_to_slave_("ARLEN",   8);
      run_master_to_slave_("ARSIZE",  3);
      run_master_to_slave_("ARBURST", 2);
      run_master_to_slave_("ARLOCK",  2);
      run_master_to_slave_("ARCACHE", 4);
      run_master_to_slave_("ARPROT",  3);
      run_master_to_slave_("ARQOS",   4);
      run_master_to_slave_("ARID",    wid_width_);

	// read data channel
      run_slave_to_master_("RVALID",  1);
      run_master_to_slave_("RREADY",  1);
      run_slave_to_master_("RDATA",   data_width_);
      run_slave_to_master_("RRESP",   2);
      run_slave_to_master_("RID",     rid_width_);

	// Interrupts
      run_slave_to_master_("IRQ",     irq_width_);
}

void AXI4Protocol::advance_bus_clock_(void)
{
	// Advance the phase pointer
      phase_ = (phase_ + 1) % 4;

	// Advance time for the next phase. (Note that the table times
	// are in pico-seconds.
      advance_time_(clock_phase_map_[phase_], -12);
}

